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


struct {
    SDL_Window* window;
    SDL_Renderer* renderer;

    struct processor_state p;
    struct bus_adaptor *basic_rom;
    struct bus_adaptor *extended_rom;
    struct bus_adaptor *cartridg;
    struct bus_adaptor *ram;
    struct bus_adaptor *pia1;
    struct bus_adaptor *pia2;
    struct bus_adaptor *sam;
    struct keyboard_status *keyboard;
    struct video_status *video;
    struct adc_status *adc;
}machine;


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
    return strncmp(str + strlen(str) - strlen(substr), substr, strlen(substr)) == 0;
}

static void SDLCALL rom_selection_cb(void* data, const char* const* filelist, int filter)
{
    if (!filelist) {
        fprintf(stderr, "An error occured: %s", SDL_GetError());
        return;
    } else if (!*filelist) {
        return;
    }

    struct bus_adaptor *cartridge=(struct bus_adaptor *)data;
    const char *rom_path = *filelist;
    if (str_ends_with(rom_path, ".bin")) {
        bus_load_ram(cartridge, rom_path, 0x4000);
    } else {
        bus_load_rom(cartridge, rom_path);

        mc6821_interrupt_1_input(PIA(machine.pia2), 1, 1);
        mc6821_interrupt_1_input(PIA(machine.pia2), 1, 0);
    }
}

int main(int argc, char* argv[]) {
    signal(SIGSEGV, segv_handler);

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
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
    processor_init(&p);

    struct bus_adaptor *basic_rom = bus_create_rom(0xA000);
    bus_load_rom(basic_rom, "roms/BASIC.ROM");
    processor_register_bus_adaptor(&p.bus, basic_rom);

    struct bus_adaptor *extended_rom = bus_create_rom(0x8000);
    bus_load_rom(extended_rom, "roms/extbas11.rom");
    processor_register_bus_adaptor(&p.bus, extended_rom);

    struct bus_adaptor *cartridge = bus_create_rom(0xC000);
    processor_register_bus_adaptor(&p.bus, cartridge);

    machine.ram = bus_create_ram(32 * 1024, 0x0000);
    processor_register_bus_adaptor(&p.bus, machine.ram);
    uint8_t *memory_buffer = (uint8_t *)machine.ram->data;

    machine.pia1 = bus_create_pia1();
    processor_register_bus_adaptor(&p.bus, machine.pia1);

    machine.pia2 = bus_create_pia2();
    processor_register_bus_adaptor(&p.bus, machine.pia2);

    machine.sam = bus_create_sam();
    processor_register_bus_adaptor(&p.bus, machine.sam);

    machine.keyboard = keyboard_initialize(PIA(machine.pia1));
    machine.video = video_initialize(SAM(machine.sam), PIA(machine.pia2), memory_buffer, machine.renderer);

    machine.adc = adc_initialize(PIA(machine.pia1), PIA(machine.pia2));

    processor_reset(&p);

    int is_1st_key_event_processed = 0;  // only one key is processed per one iteration to give time to the machine to process it
    int joy_emulation = 0;
    int joy_emulation_side = 1;  // 0: left, 1: right

    p._virtual_time_nano = nanos();  // sync time
    while (running) {
        if (is_1st_key_event_processed > 4) is_1st_key_event_processed = 0;  // wait for max 4 V. scan to send next key
        if (is_1st_key_event_processed) is_1st_key_event_processed++;

        uint64_t next_video_call_after_ns = video_start_field(machine.video);
        uint64_t next_video_call = next_video_call_after_ns + p._virtual_time_nano;
        while (next_video_call_after_ns) {
            processor_next_opcode(&p);

            while (next_video_call_after_ns && p._virtual_time_nano >= next_video_call) {
                next_video_call_after_ns = video_process_next(machine.video);
                next_video_call += next_video_call_after_ns;
                // printf("machine.video->h_sync=%d\n", machine.video->h_sync);
                mc6821_interrupt_1_input(PIA(machine.pia1), 0, machine.video->h_sync);
                mc6821_interrupt_1_input(PIA(machine.pia1), 1, machine.video->signal_fs);
            }

            p._irq = mc6821_interrupt_state(PIA(machine.pia1));  // TODO: should be done in a better way
            p._firq = mc6821_interrupt_state(PIA(machine.pia2));  // TODO: should be done in a better way
        }
        video_end_field(machine.video);

        // Update the renderer
        SDL_RenderPresent(machine.renderer);

        uint64_t time_ns = nanos();
        if (p._virtual_time_nano > time_ns) {
            struct timespec res;
            res.tv_sec = 0;
            res.tv_nsec = p._virtual_time_nano - time_ns;

            clock_nanosleep(CLOCK_MONOTONIC, 0, &res, NULL);
        }

        // Handle events
        SDL_Event event;
        while (!keyboard_buffer_empty() && (!is_1st_key_event_processed || machine.keyboard->columns_used > 24)) {
            event = keyboard_buffer_pull();
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                is_1st_key_event_processed += keyboard_set_key(machine.keyboard, &event.key, event.type == SDL_EVENT_KEY_DOWN ? 1 : 0);
            }
        }

        while (SDL_PollEvent(&event)) {
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
                    }
                    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == ']') {
                        joy_emulation_side = 1;
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
                                mc6821_peripheral_input(PIA(machine.pia1), 0, machine.keyboard->other_inputs, 0b10);
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
                                mc6821_peripheral_input(PIA(machine.pia1), 0, machine.keyboard->other_inputs, 0b1);
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
                SDL_ShowOpenFileDialog(rom_selection_cb, cartridge, machine.window, NULL, 0, "roms/cartridges", false);
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F10) {
                p._dump_execution = 1;
            }
            if (event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_F10) {
                p._dump_execution = 0;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F5) {
                joy_emulation = !joy_emulation;
                printf("Set Joy Emulator %d\n", joy_emulation);
            }
        }
    }

    // Clean up resources before exiting
    SDL_DestroyRenderer(machine.renderer);
    SDL_DestroyWindow(machine.window);
    SDL_Quit();

    return 0;
}
