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
    sam_load_rom(machine->sam, 1, app_settings.rom_basic_path);
    sam_load_rom(machine->sam, 0, app_settings.rom_extended_basic_path);
    sam_load_rom(machine->sam, 3, app_settings.rom_disc_basic_path);
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

    machine->_joy_emulation[0] = 0;
    machine->_joy_emulation[1] = 0;
    machine->joysticks[0] = 0;
    machine->joysticks[1] = 0;

    machine->_next_keyboard_poll_ns = 0;

    for (int i = 0; i < 4; i++) {
        if (!app_settings.disks[i].path || !app_settings.disks[i].path[0]) continue;
        disk_drive_load_disk(machine->disk_drive, i, app_settings.disks[i].path);
    }
    if(app_settings.cartridge_path && app_settings.cartridge_path[0]) {
        sam_load_rom(machine->sam, 2, app_settings.cartridge_path);
        machine->cart_sense = 1;
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
int machine_process_frame(struct machine_status *machine) {
    uint64_t next_video_call_after_ns = video_start_field(machine->video);
    uint64_t next_video_call = next_video_call_after_ns + machine->p._virtual_time_nano;
    while (next_video_call_after_ns > 0) {
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
            // log_message(LOG_INFO, "machine->video->h_sync=%d", machine->video->h_sync);
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

        machine->p._halt = machine->disk_drive->HALT && !machine->disk_drive->status_2_3.DATA_REQUEST;

        if (machine->disk_drive->next_command_after_nano) {
            // schedule next call to the disk drive
            machine->_next_disk_drive_call = machine->p._virtual_time_nano + machine->disk_drive->next_command_after_nano;
            machine->disk_drive->next_command_after_nano = 0;
        }

        machine->p._irq = mc6821_interrupt_state(machine->sam->pia1);  // TODO: should be done in a better way
        machine->p._firq = mc6821_interrupt_state(machine->sam->pia2);  // TODO: should be done in a better way
    }
    if (!next_video_call_after_ns) {
        video_end_field(machine->video);
    }
    return (int)next_video_call_after_ns;
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

int machine_handle_joystick_event(struct machine_status *machine, SDL_Event *event) {
    int event_was_handled = 0;
    switch (event->type)
    {
        case SDL_EVENT_JOYSTICK_ADDED:
        case SDL_EVENT_JOYSTICK_REMOVED:
            // Nuke everything and re-initialize the 1st two joysticks
            {
                for (int i=0; i < 2; i++) {
                    if (machine->joysticks[i]) {
                        SDL_CloseJoystick(machine->joysticks[i]);
                        machine->joysticks[i] = NULL;
                    }
                }
                int joy_count;
                SDL_JoystickID *ids = SDL_GetJoysticks(&joy_count);
                log_message(LOG_INFO, "Joysticks connected count: %d", joy_count);
                for (int i=0; i < 2 && i < joy_count; i++) {
                    machine->joysticks[i] = SDL_OpenJoystick(ids[i]);
                    machine->joystick_ids[i] = ids[i];
                }
            }
            event_was_handled = 1;
            break;

        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
            for (int joy_number = 0; joy_number < 2; joy_number++) {
                if (
                    (app_settings.joy_emulation_mode[joy_number] == Joy_Emulation_Joy1 && machine->joysticks[0] && machine->joystick_ids[0] == event->jaxis.which) ||
                    (app_settings.joy_emulation_mode[joy_number] == Joy_Emulation_Joy2 && machine->joysticks[1] && machine->joystick_ids[1] == event->jaxis.which)
                ) {
                    switch(joy_number) {
                        case 0: // left
                            if (event->jaxis.axis == 0) machine->adc->input_joy_0 = event->jaxis.value * 2.5 / 32767 + 2.5;
                            if (event->jaxis.axis == 1) machine->adc->input_joy_1 = event->jaxis.value * 2.5 / 32767 + 2.5;
                            break;
                        case 1: // right
                            if (event->jaxis.axis == 0) machine->adc->input_joy_2 = event->jaxis.value * 2.5 / 32767 + 2.5;
                            if (event->jaxis.axis == 1) machine->adc->input_joy_3 = event->jaxis.value * 2.5 / 32767 + 2.5;
                            break;
                    }
                    event_was_handled = 1;
                }
            }
            break;

        case SDL_EVENT_JOYSTICK_BUTTON_UP: /* MOUSEBUTTONUP & MOUSEBUTTONDOWN share same routine */
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            for (int joy_number = 0; joy_number < 2; joy_number++) {
                if (
                    (app_settings.joy_emulation_mode[joy_number] == Joy_Emulation_Joy1 && machine->joysticks[0] && machine->joystick_ids[0] == event->jbutton.which) ||
                    (app_settings.joy_emulation_mode[joy_number] == Joy_Emulation_Joy2 && machine->joysticks[1] && machine->joystick_ids[1] == event->jbutton.which)
                ) {
                    event_was_handled = 1;

                    switch(joy_number) {
                        case 0: // left
                            if (event->type == SDL_EVENT_JOYSTICK_BUTTON_DOWN) machine->keyboard->other_inputs &= 0b11111110;
                            else machine->keyboard->other_inputs |= 0b1;
                            mc6821_peripheral_input(machine->sam->pia1, 0, machine->keyboard->other_inputs, 0b1);
                            break;
                        case 1: // right
                            if (event->type == SDL_EVENT_JOYSTICK_BUTTON_DOWN) machine->keyboard->other_inputs &= 0b11111101;
                            else machine->keyboard->other_inputs |= 0b10;
                            mc6821_peripheral_input(machine->sam->pia1, 0, machine->keyboard->other_inputs, 0b10);
                            break;
                    }
                }
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            for (int joy_number = 0; joy_number < 2; joy_number++) {
                if (app_settings.joy_emulation_mode[joy_number] != Joy_Emulation_Mouse || !machine->_joy_emulation[joy_number]) continue;
                event_was_handled = 1;
                float scale_x = 5.0 / machine->video->_output_port.w;
                float scale_y = 5.0 / machine->video->_output_port.h;

                switch(joy_number) {
                    case 0: // left
                        machine->adc->input_joy_0 += event->motion.xrel * scale_x;
                        machine->adc->input_joy_1 += event->motion.yrel * scale_y;
                        if (machine->adc->input_joy_0 < 0) machine->adc->input_joy_0 = 0;
                        if (machine->adc->input_joy_0 > 5) machine->adc->input_joy_0 = 5;
                        if (machine->adc->input_joy_1 < 0) machine->adc->input_joy_1 = 0;
                        if (machine->adc->input_joy_1 > 5) machine->adc->input_joy_1 = 5;
                        break;
                    case 1: // right
                        machine->adc->input_joy_2 += event->motion.xrel * scale_x;
                        machine->adc->input_joy_3 += event->motion.yrel * scale_y;
                        if (machine->adc->input_joy_2 < 0) machine->adc->input_joy_2 = 0;
                        if (machine->adc->input_joy_2 > 5) machine->adc->input_joy_2 = 5;
                        if (machine->adc->input_joy_3 < 0) machine->adc->input_joy_3 = 0;
                        if (machine->adc->input_joy_3 > 5) machine->adc->input_joy_3 = 5;
                        break;
                }
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP: /* MOUSEBUTTONUP & MOUSEBUTTONDOWN share same routine */
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            for (int joy_number = 0; joy_number < 2; joy_number++) {
                if (app_settings.joy_emulation_mode[joy_number] != Joy_Emulation_Mouse || !machine->_joy_emulation[joy_number]) continue;
                event_was_handled = 1;

                if (event->button.button == SDL_BUTTON_RIGHT) {
                    machine->_joy_emulation[0] = 0;
                    machine->_joy_emulation[1] = 0;
                    SDL_SetWindowRelativeMouseMode(machine->window, machine->_joy_emulation[0] || machine->_joy_emulation[1]);
                    continue;
                }

                switch(joy_number) {
                    case 0: // left
                        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) machine->keyboard->other_inputs &= 0b11111110;
                        else machine->keyboard->other_inputs |= 0b1;
                        mc6821_peripheral_input(machine->sam->pia1, 0, machine->keyboard->other_inputs, 0b1);
                        break;
                    case 1: // right
                        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) machine->keyboard->other_inputs &= 0b11111101;
                        else machine->keyboard->other_inputs |= 0b10;
                        mc6821_peripheral_input(machine->sam->pia1, 0, machine->keyboard->other_inputs, 0b10);
                        break;
                }
            }
            break;

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            for (int joy_number = 0; joy_number < 2; joy_number++) {
                if (app_settings.joy_emulation_mode[joy_number] != Joy_Emulation_Keyboard || !machine->_joy_emulation[joy_number]) continue;
                if (!(event->key.key == SDLK_LEFT || event->key.key == SDLK_RIGHT ||
                    event->key.key == SDLK_UP || event->key.key == SDLK_DOWN ||
                    event->key.key == SDLK_SPACE || event->key.key == SDLK_RETURN)) continue;
                event_was_handled = 1;

                switch(joy_number) {
                    case 0: // left
                        switch(event->key.key) {
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
                        break;
                    case 1: // right
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
                        break;
                }
            }
            break;
    }

    return event_was_handled;
}

int machine_handle_input(struct machine_status *machine, SDL_Event *event) {
    if (machine->settings_page_is_open) {
        if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
            machine->settings_page_is_open = false;
        }
        return 0;
    }

    if (machine_handle_joystick_event(machine, event))
        return 1;


    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_F5) {
        int emulation = machine->_joy_emulation[0] || machine->_joy_emulation[1];
        emulation = !emulation;
        machine->_joy_emulation[0] = emulation;
        machine->_joy_emulation[1] = emulation;

        if (app_settings.joy_emulation_mode[0] == Joy_Emulation_Mouse || app_settings.joy_emulation_mode[1] == Joy_Emulation_Mouse) {
            SDL_SetWindowRelativeMouseMode(machine->window, emulation);
        }
        log_message(LOG_INFO, "Set Joy Emulator %d", emulation);
    }

    if ((event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) && !(event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT))) {
        keyboard_buffer_push(event);
        return 1;
    }

    if (event->type == SDL_EVENT_KEY_DOWN && event->key.mod & SDL_KMOD_CTRL && event->key.key == SDLK_V) {
        clipboard_copy();
        return 1;
    }
    return 0;
}