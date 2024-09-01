#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <time.h>
#include "processor_6809.h"
#include "keyboard.h"
#include "video.h"


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

int main2(int argc, char* argv[]) {
    uint8_t a= 100;
    uint8_t b= 100;
    uint16_t num = a * b;
    printf("value = %d\n", num);
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
    keyboard_buffer[keyboard_buffer_end++] = *event;  // save a copy in the buffer

    // extend the buffer
    if (keyboard_buffer_end == KEY_BOARD_BUFFER_LENGTH) keyboard_buffer_end = 0;

    // the buffer is already full, so drop from the start
    if (keyboard_buffer_end == keyboard_buffer_start) keyboard_buffer_start++;
    if (keyboard_buffer_start == KEY_BOARD_BUFFER_LENGTH) keyboard_buffer_start = 0;
}

SDL_Event keyboard_buffer_pull() {
    int pos = keyboard_buffer_start++;
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

        event.key.keysym.sym = ch;

        event.type = SDL_KEYDOWN;
        keyboard_buffer_push(&event);
        event.type = SDL_KEYUP;
        keyboard_buffer_push(&event);

        p++;
    }
    SDL_free(text);
}

int main(int argc, char* argv[]) {
    signal(SIGSEGV, segv_handler);

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Error initializing SDL: %s\n", SDL_GetError());
        return -1;
    }

    // Create a window and renderer
    SDL_Window* window = SDL_CreateWindow("Simple Graphics", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 256 * 4 + 40, 192 * 4 + 40, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // Renderer for drawing graphics
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    bool running = true;

    struct processor_state p;
    processor_init(&p);

    struct bus_adaptor *basic_rom = bus_create_rom("roms/BASIC.ROM", 0xA000);
    processor_register_bus_adaptor(&p.bus, basic_rom);

    struct bus_adaptor *extended_rom = bus_create_rom("roms/extbas11.rom", 0x8000);
    processor_register_bus_adaptor(&p.bus, extended_rom);

    struct bus_adaptor *ram = bus_create_ram(32 * 1024, 0x0000);
    processor_register_bus_adaptor(&p.bus, ram);
    uint8_t *memory_buffer = (uint8_t *)ram->data;

    struct bus_adaptor *pia1 = bus_create_pia1();
    processor_register_bus_adaptor(&p.bus, pia1);

    struct bus_adaptor *pia2 = bus_create_pia2();
    processor_register_bus_adaptor(&p.bus, pia2);

    struct bus_adaptor *sam = bus_create_sam();
    processor_register_bus_adaptor(&p.bus, sam);

    struct keyboard_status *keyboard = keyboard_initialize(PIA(pia1));
    struct video_status *video = video_initialize(SAM(sam), PIA(pia2), memory_buffer, renderer);

    processor_reset(&p);

    while (running) {
        // Handle events
        SDL_Event event;

        uint64_t next_frame_ns = nanos() + frame_time_nano;
        int is_1st_key_event_processed = 0;  // only one key is processed per one iteration to give time to the machine to process it

        while (!keyboard_buffer_empty() && !is_1st_key_event_processed) {
            event = keyboard_buffer_pull();
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                is_1st_key_event_processed = keyboard_set_key(keyboard, &event.key, event.type == SDL_KEYDOWN ? 1 : 0);
            }
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && !(event.key.keysym.mod & (KMOD_CTRL | KMOD_ALT))) {
                if (!is_1st_key_event_processed)
                    is_1st_key_event_processed = keyboard_set_key(keyboard, &event.key, event.type == SDL_KEYDOWN ? 1 : 0);
                else
                    keyboard_buffer_push(&event);
            }

            if (event.type == SDL_KEYDOWN && event.key.keysym.mod & KMOD_CTRL && event.key.keysym.sym == SDLK_v) {
                clipboard_copy();
                is_1st_key_event_processed = 1;
            }

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F10) {
                p._dump_execution = 1;
                PIA(pia1)->a._dump_read = 1;
            }
            if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_F10) {
                p._dump_execution = 0;
            }
        }

        p._nano_time_passed = 0;
        while (p._nano_time_passed < frame_time_nano) {
            processor_next_opcode(&p);
            p._irq = mc6821_interrupt_state(PIA(pia1));  // TODO: should be done in a better way
        }

        // Clear the renderer with a black color
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        video_render(video);

        // Update the renderer
        SDL_RenderPresent(renderer);

        // Cap at 60 FPS to avoid excessive CPU usage
        SDL_Delay(1000 / 60);

        uint64_t time_ns = nanos();
        if (next_frame_ns > time_ns) {
            struct timespec res;
            res.tv_sec = 0;
            res.tv_nsec = next_frame_ns - time_ns;

            clock_nanosleep(CLOCK_MONOTONIC, 0, &res, NULL);
        }

        // generate an interrupt at every v sync at CB1 pin of PIA1
        mc6821_interrupt_1_input(PIA(pia1), 1, 1);
        mc6821_interrupt_1_input(PIA(pia1), 1, 0);
    }

    // Clean up resources before exiting
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
