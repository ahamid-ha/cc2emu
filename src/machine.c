#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "machine.h"
#include "utils.h"
#include "settings.h"


// Just by testing, I found that 70ms provide a stable keyboard with no misses with extended color basic
#define KEYBOARD_POLL_PERIOD_NS 70000000

int keyboard_buffer_empty();
SDL_Event keyboard_buffer_pull();


void machine_init(struct machine_status *machine) {
    processor_init(&machine->p);

    machine->sam = bus_create_sam();
    machine->p.bus = machine->sam;
    sam_load_rom(machine->sam, 1, "roms/BASIC.ROM");
    sam_load_rom(machine->sam, 0, "roms/extbas11.rom");
    sam_load_rom(machine->sam, 2, "roms/DSKBASIC.ROM");
    machine->sam->pia1 = pia_create();
    machine->sam->pia2 = pia_create();

    machine->keyboard = keyboard_initialize(machine->sam->pia1);
    machine->video = video_initialize(machine->sam, machine->sam->pia2, machine->renderer);
    machine->adc = adc_initialize(machine->sam->pia1, machine->sam->pia2);

    machine->disk_drive = disk_drive_create();
    machine->sam->pia_cartridge = machine->disk_drive;
    machine->sam->pia_cartridge_read = disk_drive_read_register;
    machine->sam->pia_cartridge_write = disk_drive_write_register;

    machine->cart_sense = 0;
    machine->_next_disk_drive_call = 0;
    machine->p._virtual_time_nano = nanos();  // sync time

    machine->_joy_emulation = 0;
    machine->_joy_emulation_side = 1;  // 0: left, 1: right

    machine->_next_keyboard_poll_ns = 0;

    for (int i = 0; i < 4; i++) {
        if (!app_settings.disks[i].path || !app_settings.disks[i].path[0]) continue;
        disk_drive_load_disk(machine->disk_drive, i, app_settings.disks[i].path);
    }
    if(app_settings.cartridge_path && app_settings.cartridge_path[0]) {
        sam_load_rom(machine->sam, 2, app_settings.cartridge_path);
    }
    if(app_settings.cassette_path && app_settings.cassette_path[0]) {
        adc_load_cassette(machine->adc, app_settings.cassette_path);
    }
}

void machine_reset(struct machine_status *machine) {
    bus_reset_pia(machine->sam->pia1);
    bus_reset_pia(machine->sam->pia2);
    sam_reset(machine->sam);
    keyboard_reset(machine->keyboard);
    video_reset(machine->video);
    adc_reset(machine->adc);
    processor_reset(&machine->p);
}

/*
    Runs as much processor instructions that are equivalent to one vertical sync frame
    Also runs the devices according to the processor virtual time
    This includes the video rendering
*/
void machine_process_frame(struct machine_status *machine) {
    uint64_t next_video_call_after_ns = video_start_field(machine->video);
    uint64_t next_video_call = next_video_call_after_ns + machine->p._virtual_time_nano;
    while (next_video_call_after_ns) {
        processor_next_opcode(&machine->p);

        if (machine->p._virtual_time_nano >= machine->_next_keyboard_poll_ns && !keyboard_buffer_empty() ) {
            SDL_Event event = keyboard_buffer_pull();
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                keyboard_set_key(machine->keyboard, &event.key, event.type == SDL_EVENT_KEY_DOWN ? 1 : 0);
                machine->_next_keyboard_poll_ns = machine->p._virtual_time_nano + KEYBOARD_POLL_PERIOD_NS;
            }
        }

        machine->p._nmi = machine->disk_drive->irq && machine->disk_drive->DDEN;

        adc_process(machine->adc, machine->p._virtual_time_nano);

        while (next_video_call_after_ns && machine->p._virtual_time_nano >= next_video_call) {
            next_video_call_after_ns = video_process_next(machine->video);
            next_video_call += next_video_call_after_ns;
            // printf("machine->video->h_sync=%d\n", machine->video->h_sync);
            mc6821_interrupt_1_input(machine->sam->pia1, 0, machine->video->h_sync);
            mc6821_interrupt_1_input(machine->sam->pia1, 1, machine->video->signal_fs);

            if (machine->cart_sense) {
                mc6821_interrupt_1_input(machine->sam->pia2, 1, 1);
                mc6821_interrupt_1_input(machine->sam->pia2, 1, 0);
            }
        }

        if (machine->_next_disk_drive_call && machine->p._virtual_time_nano >= machine->_next_disk_drive_call) {
            disk_drive_process_next(machine->disk_drive);
            machine->_next_disk_drive_call = 0;
        }

        machine->p._stopped = machine->disk_drive->HALT && !machine->disk_drive->status_2_3.DATA_REQUEST;

        if (machine->disk_drive->next_command_after_nano) {
            // schedule next call to the disk drive
            machine->_next_disk_drive_call = machine->p._virtual_time_nano + machine->disk_drive->next_command_after_nano;
            machine->disk_drive->next_command_after_nano = 0;
        }

        machine->p._irq = mc6821_interrupt_state(machine->sam->pia1);  // TODO: should be done in a better way
        machine->p._firq = mc6821_interrupt_state(machine->sam->pia2);  // TODO: should be done in a better way
    }
    video_end_field(machine->video);
}

#define KEY_BOARD_BUFFER_LENGTH 2000
SDL_Event keyboard_buffer[KEY_BOARD_BUFFER_LENGTH];  // ring buffer
int keyboard_buffer_start = 0;
int keyboard_buffer_end = 0;

void keyboard_buffer_reset() {
    keyboard_buffer_start = 0;
    keyboard_buffer_end = 0;
}

int keyboard_buffer_empty() {
    return keyboard_buffer_start == keyboard_buffer_end;
}

void keyboard_buffer_push(SDL_Event *event) {
    keyboard_buffer[keyboard_buffer_end] = *event;  // save a copy in the buffer
    keyboard_buffer_end++;

    // extend the buffer
    if (keyboard_buffer_end == KEY_BOARD_BUFFER_LENGTH) keyboard_buffer_end = 0;

    // the buffer is already full, so drop from the start
    if (keyboard_buffer_end == keyboard_buffer_start) keyboard_buffer_start++;
    if (keyboard_buffer_start == KEY_BOARD_BUFFER_LENGTH) keyboard_buffer_start = 0;
}

void machine_send_key(uint32_t key_code) {
    SDL_Event event;
    memset(&event, 0, sizeof(SDL_Event));
    event.key.key = key_code;

    event.type = SDL_EVENT_KEY_DOWN;
    keyboard_buffer_push(&event);
    event.type = SDL_EVENT_KEY_UP;
}

SDL_Event keyboard_buffer_pull() {
    int pos = keyboard_buffer_start;
    keyboard_buffer_start++;
    if (keyboard_buffer_start == KEY_BOARD_BUFFER_LENGTH) keyboard_buffer_start = 0;
    return keyboard_buffer[pos];
}

void clipboard_copy() {
    char *text = SDL_GetClipboardText();
    SDL_Event event;

    if (!text) return;

    char *p = text;
    while (*p) {
        char ch = *p;
        if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';  // send only lowercase key syms
        memset(&event, 0, sizeof(SDL_Event));

        event.key.key = ch;

        event.type = SDL_EVENT_KEY_DOWN;
        keyboard_buffer_push(&event);
        event.type = SDL_EVENT_KEY_UP;
        keyboard_buffer_push(&event);

        p++;
    }
    SDL_free(text);
}

void machine_handle_input_begin(struct machine_status *machine) {
    // nothing for now
}

void machine_handle_input(struct machine_status *machine, SDL_Event *event) {
    if (machine->_joy_emulation &&
            (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) &&
            !(event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT)) &&
            (event->key.key == SDLK_LEFT || event->key.key == SDLK_RIGHT ||
                event->key.key == SDLK_UP || event->key.key == SDLK_DOWN ||
                event->key.key == SDLK_SPACE || event->key.key == SDLK_RETURN ||
                event->key.key == '[' || event->key.key == ']'
            )
        ) {
            if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == '[') {
                machine->_joy_emulation_side = 0;
                printf("Keyboard switched to Left Joystick\n");
            }
            if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == ']') {
                machine->_joy_emulation_side = 1;
                printf("Keyboard switched to Right Joystick\n");
            }
            if (machine->_joy_emulation_side == 1) {
                switch(event->key.key) {
                    case SDLK_LEFT: machine->adc->input_joy_2 = event->type == SDL_EVENT_KEY_DOWN ? 1 : 2.5; break;
                    case SDLK_RIGHT: machine->adc->input_joy_2 = event->type == SDL_EVENT_KEY_DOWN ? 4 : 2.5; break;
                    case SDLK_UP: machine->adc->input_joy_3 = event->type == SDL_EVENT_KEY_DOWN ? 1 : 2.5; break;
                    case SDLK_DOWN: machine->adc->input_joy_3 = event->type == SDL_EVENT_KEY_DOWN ? 4 : 2.5; break;
                    case SDLK_RETURN:
                    case SDLK_SPACE:
                        if (event->type == SDL_EVENT_KEY_DOWN) machine->keyboard->other_inputs &= 0b11111101;
                        else machine->keyboard->other_inputs |= 0b10;
                        mc6821_peripheral_input(machine->sam->pia1, 0, machine->keyboard->other_inputs, 0b10);
                        break;
                }
            } else {
                switch(event->key.key) {
                    // left
                    case SDLK_LEFT: machine->adc->input_joy_0 = event->type == SDL_EVENT_KEY_DOWN ? 1 : 2.5; break;
                    case SDLK_RIGHT: machine->adc->input_joy_0 = event->type == SDL_EVENT_KEY_DOWN ? 4 : 2.5; break;
                    case SDLK_UP: machine->adc->input_joy_1 = event->type == SDL_EVENT_KEY_DOWN ? 1 : 2.5; break;
                    case SDLK_DOWN: machine->adc->input_joy_1 = event->type == SDL_EVENT_KEY_DOWN ? 4 : 2.5; break;
                    case SDLK_RETURN:
                    case SDLK_SPACE:
                        if (event->type == SDL_EVENT_KEY_DOWN) machine->keyboard->other_inputs &= 0b11111110;
                        else machine->keyboard->other_inputs |= 0b1;
                        mc6821_peripheral_input(machine->sam->pia1, 0, machine->keyboard->other_inputs, 0b1);
                        break;
                }
            }
            // printf("event->key.key=%d \n", event->key.key);
    } else {
        if ((event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) && !(event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT))) {
            keyboard_buffer_push(event);
        }
    }

    if (event->type == SDL_EVENT_KEY_DOWN && event->key.mod & SDL_KMOD_CTRL && event->key.key == SDLK_V) {
        clipboard_copy();
    }

}