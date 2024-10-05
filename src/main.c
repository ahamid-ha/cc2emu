#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>
#include <time.h>
#include "processor_6809.h"
#include "keyboard.h"
#include "video.h"
#include "adc.h"
#include "disk_drive.h"


struct {
    SDL_Window* window;
    SDL_Renderer* renderer;

    struct processor_state *p;
    struct sam_status *sam;
    struct keyboard_status *keyboard;
    struct video_status *video;
    struct adc_status *adc;
    struct disk_drive_status *disk_drive;
    int cart_sense;
}machine;

void machine_reset() {
    bus_reset_pia(machine.sam->pia1);
    bus_reset_pia(machine.sam->pia2);
    sam_reset(machine.sam);
    keyboard_reset(machine.keyboard);
    video_reset(machine.video);
    adc_reset(machine.adc);
    processor_reset(machine.p);
}


#define SEC_TO_NS(sec) ((sec)*1000000000)
uint64_t nanos()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    uint64_t ns = SEC_TO_NS((uint64_t)ts.tv_sec) + (uint64_t)ts.tv_nsec;
    return ns;
}

void segv_handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

#define KEY_BOARD_BUFFER_LENGTH 2000
SDL_Event keyboard_buffer[KEY_BOARD_BUFFER_LENGTH];  // ring buffer
int keyboard_buffer_length;
int keyboard_buffer_start = 0;
int keyboard_buffer_end = 0;

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

bool str_ends_with(const char *str, const char *substr) {
    if (!str || !substr) return false;
    if (strlen(str) < strlen(substr)) return false;
    return strncasecmp(str + strlen(str) - strlen(substr), substr, strlen(substr)) == 0;
}

static void SDLCALL rom_selection_cb(void* data, const char* const* filelist, int filter)
{
    if (!filelist) {
        fprintf(stderr, "An error occured: %s", SDL_GetError());
        return;
    } else if (!*filelist) {
        return;
    }

    const char *rom_path = *filelist;
    if (str_ends_with(rom_path, ".wav")) {
        adc_load_cassette(machine.adc, rom_path);
    } else if (str_ends_with(rom_path, ".dsk")) {
        disk_drive_load_disk(machine.disk_drive, 0, rom_path);
    } else {
        machine_reset();
        sam_load_rom(machine.sam, 2, rom_path);
        machine.cart_sense = 1;
    }
}

int main(int argc, char* argv[]) {
    signal(SIGSEGV, segv_handler);

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        printf("Error initializing SDL: %s\n", SDL_GetError());
        return -1;
    }

    // Create a window and renderer
    machine.window = SDL_CreateWindow("Emulator", 256 * 4 + 40, 192 * 4 + 40, 0);
    if (!machine.window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // Renderer for drawing graphics
    machine.renderer = SDL_CreateRenderer(machine.window, NULL);
    if (!machine.renderer) {
        printf("Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(machine.window);
        SDL_Quit();
        return -1;
    }

    bool running = true;

    struct processor_state p;
    machine.p = &p;
    processor_init(&p);

    machine.sam = bus_create_sam();
    p.bus = machine.sam;
    sam_load_rom(machine.sam, 1, "roms/BASIC.ROM");
    sam_load_rom(machine.sam, 0, "roms/extbas11.rom");
    sam_load_rom(machine.sam, 2, "roms/DSKBASIC.ROM");
    machine.sam->pia1 = pia_create();
    machine.sam->pia2 = pia_create();

    machine.keyboard = keyboard_initialize(machine.sam->pia1);
    machine.video = video_initialize(machine.sam, machine.sam->pia2, machine.renderer);
    machine.adc = adc_initialize(machine.sam->pia1, machine.sam->pia2);

    machine.disk_drive = disk_drive_create();
    machine.sam->pia_cartridge = machine.disk_drive;
    machine.sam->pia_cartridge_read = disk_drive_read_register;
    machine.sam->pia_cartridge_write = disk_drive_write_register;

    machine.cart_sense = 0;

    processor_reset(&p);

    int is_1st_key_event_processed = 0;  // only one key is processed per one iteration to give time to the machine to process it
    int joy_emulation = 0;
    int joy_emulation_side = 1;  // 0: left, 1: right

    p._virtual_time_nano = nanos();  // sync time
    uint64_t next_disk_drive_call = 0;
    while (running) {
        if (is_1st_key_event_processed > 4) is_1st_key_event_processed = 0;  // wait for max 4 V. scan to send next key
        if (is_1st_key_event_processed) is_1st_key_event_processed++;

        uint64_t next_video_call_after_ns = video_start_field(machine.video);
        uint64_t next_video_call = next_video_call_after_ns + p._virtual_time_nano;
        while (next_video_call_after_ns) {
            processor_next_opcode(&p);

            adc_process(machine.adc, p._virtual_time_nano);

            while (next_video_call_after_ns && p._virtual_time_nano >= next_video_call) {
                next_video_call_after_ns = video_process_next(machine.video);
                next_video_call += next_video_call_after_ns;
                // printf("machine.video->h_sync=%d\n", machine.video->h_sync);
                mc6821_interrupt_1_input(machine.sam->pia1, 0, machine.video->h_sync);
                mc6821_interrupt_1_input(machine.sam->pia1, 1, machine.video->signal_fs);

                if (machine.cart_sense) {
                    mc6821_interrupt_1_input(machine.sam->pia2, 1, 1);
                    mc6821_interrupt_1_input(machine.sam->pia2, 1, 0);
                }
            }

            if (next_disk_drive_call && p._virtual_time_nano >= next_disk_drive_call) {
                disk_drive_process_next(machine.disk_drive);
                next_disk_drive_call = 0;
            }

            p._stopped = machine.disk_drive->HALT && !machine.disk_drive->status_2_3.DATA_REQUEST;

            if (machine.disk_drive->next_command_after_nano) {
                // schedule next call to the disk drive
                next_disk_drive_call = p._virtual_time_nano + machine.disk_drive->next_command_after_nano;
                machine.disk_drive->next_command_after_nano = 0;
            }

            if (machine.disk_drive->irq && machine.disk_drive->DDEN) {
                p._nmi = 1;
                machine.disk_drive->irq = 0;
            }

            p._irq = mc6821_interrupt_state(machine.sam->pia1);  // TODO: should be done in a better way
            p._firq = mc6821_interrupt_state(machine.sam->pia2);  // TODO: should be done in a better way
        }
        video_end_field(machine.video);

        // Update the renderer
        SDL_RenderPresent(machine.renderer);

        uint64_t time_ns = nanos();
        while (p._virtual_time_nano > time_ns) {
            // Handle events
            SDL_Event event;
            while (!keyboard_buffer_empty() && (!is_1st_key_event_processed || machine.keyboard->columns_used > 24)) {
                event = keyboard_buffer_pull();
                if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                    is_1st_key_event_processed += keyboard_set_key(machine.keyboard, &event.key, event.type == SDL_EVENT_KEY_DOWN ? 1 : 0);
                }
            }

            while (SDL_WaitEventTimeout(&event, 10)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }

                if (joy_emulation &&
                        (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) &&
                        !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT)) &&
                        (event.key.key == SDLK_LEFT || event.key.key == SDLK_RIGHT ||
                            event.key.key == SDLK_UP || event.key.key == SDLK_DOWN ||
                            event.key.key == SDLK_SPACE || event.key.key == SDLK_RETURN ||
                            event.key.key == '[' || event.key.key == ']'
                        )
                    ) {
                        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == '[') {
                            joy_emulation_side = 0;
                            printf("Keyboard switched to Left Joystick\n");
                        }
                        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == ']') {
                            joy_emulation_side = 1;
                            printf("Keyboard switched to Right Joystick\n");
                        }
                        if (joy_emulation_side == 1) {
                            switch(event.key.key) {
                                case SDLK_LEFT: machine.adc->input_joy_2 = event.type == SDL_EVENT_KEY_DOWN ? 1 : 2.5; break;
                                case SDLK_RIGHT: machine.adc->input_joy_2 = event.type == SDL_EVENT_KEY_DOWN ? 4 : 2.5; break;
                                case SDLK_UP: machine.adc->input_joy_3 = event.type == SDL_EVENT_KEY_DOWN ? 1 : 2.5; break;
                                case SDLK_DOWN: machine.adc->input_joy_3 = event.type == SDL_EVENT_KEY_DOWN ? 4 : 2.5; break;
                                case SDLK_RETURN:
                                case SDLK_SPACE:
                                    if (event.type == SDL_EVENT_KEY_DOWN) machine.keyboard->other_inputs &= 0b11111101;
                                    else machine.keyboard->other_inputs |= 0b10;
                                    mc6821_peripheral_input(machine.sam->pia1, 0, machine.keyboard->other_inputs, 0b10);
                                    break;
                            }
                        } else {
                            switch(event.key.key) {
                                // left
                                case SDLK_LEFT: machine.adc->input_joy_0 = event.type == SDL_EVENT_KEY_DOWN ? 1 : 2.5; break;
                                case SDLK_RIGHT: machine.adc->input_joy_0 = event.type == SDL_EVENT_KEY_DOWN ? 4 : 2.5; break;
                                case SDLK_UP: machine.adc->input_joy_1 = event.type == SDL_EVENT_KEY_DOWN ? 1 : 2.5; break;
                                case SDLK_DOWN: machine.adc->input_joy_1 = event.type == SDL_EVENT_KEY_DOWN ? 4 : 2.5; break;
                                case SDLK_RETURN:
                                case SDLK_SPACE:
                                    if (event.type == SDL_EVENT_KEY_DOWN) machine.keyboard->other_inputs &= 0b11111110;
                                    else machine.keyboard->other_inputs |= 0b1;
                                    mc6821_peripheral_input(machine.sam->pia1, 0, machine.keyboard->other_inputs, 0b1);
                                    break;
                            }
                        }
                        // printf("event.key.key=%d \n", event.key.key);
                } else {
                    if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT))) {
                        if ((!is_1st_key_event_processed || machine.keyboard->columns_used > 24))
                            is_1st_key_event_processed += keyboard_set_key(machine.keyboard, &event.key, event.type == SDL_EVENT_KEY_DOWN ? 1 : 0);
                        else
                            keyboard_buffer_push(&event);
                    }
                }

                if (event.type == SDL_EVENT_KEY_DOWN && event.key.mod & SDL_KMOD_CTRL && event.key.key == SDLK_V) {
                    clipboard_copy();
                }

                if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F9) {
                    SDL_ShowOpenFileDialog(rom_selection_cb, NULL, machine.window, NULL, 0, "roms/cartridges", false);
                }
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F10) {
                    machine_reset();
                    machine.cart_sense = 0;
                }
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F5) {
                    joy_emulation = !joy_emulation;
                    printf("Set Joy Emulator %d\n", joy_emulation);
                }
            }

            time_ns = nanos();
        }
    }

    // Clean up resources before exiting
    SDL_DestroyRenderer(machine.renderer);
    SDL_DestroyWindow(machine.window);
    SDL_Quit();

    return 0;
}
