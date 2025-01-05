#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>
#include "machine.h"
#include "controls.h"
#include "utils.h"
#include "nk_sdl.h"
#include "settings.h"


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

int main(int argc, char* argv[]) {
    signal(SIGSEGV, segv_handler);

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        printf("Error initializing SDL: %s\n", SDL_GetError());
        return -1;
    }

    settings_init();

    struct machine_status *machine = malloc(sizeof(struct machine_status));

    // Create a window and renderer
    machine->window = SDL_CreateWindow("Emulator", 256 * 4 + 40, 192 * 4 + 40 + 40, 0);
    if (!machine->window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // Renderer for drawing graphics
    machine->renderer = SDL_CreateRenderer(machine->window, NULL);
    if (!machine->renderer) {
        printf("Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(machine->window);
        SDL_Quit();
        return -1;
    }

    controls_init(machine);

    machine_init(machine);

    bool running = true;
    processor_reset(&machine->p);
    disk_drive_reset(machine->disk_drive);

    while (running) {
        if (machine->p._instruction_fault)
            SDL_SetRenderDrawColor(machine->renderer, 100, 0, 0, 255);
        else
            SDL_SetRenderDrawColor(machine->renderer, 0, 0, 0, 255);
        SDL_RenderClear(machine->renderer);

        machine_process_frame(machine);

        controls_display();

        // Update the renderer
        SDL_RenderPresent(machine->renderer);

        uint64_t time_ns = nanos();
        machine_handle_input_begin(machine);

        controls_input_begin();
        while (machine->p._virtual_time_nano > time_ns) {
            // Handle events
            SDL_Event event;
            while (SDL_WaitEventTimeout(&event, 10)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }

                if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
                    SDL_DestroyRenderer(machine->renderer);
                    machine->renderer = SDL_CreateRenderer(machine->window, NULL);
                    if (!machine->renderer) {
                        printf("Failed to create renderer: %s\n", SDL_GetError());
                        SDL_DestroyWindow(machine->window);
                        SDL_Quit();
                        return -1;
                    }
                    video_reinitialize(machine->video, machine->renderer);
                    controls_reinit();
                }

                machine_handle_input(machine, &event);

                if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F10) {
                    machine_reset(machine);
                    machine->cart_sense = 0;
                    disk_drive_reset(machine->disk_drive);
                }
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F5) {
                    machine->_joy_emulation = !machine->_joy_emulation;
                    printf("Set Joy Emulator %d\n", machine->_joy_emulation);
                }

                nk_sdl_handle_event(&event);
            }

            time_ns = nanos();
        }
    }
    controls_input_end();

    // Clean up resources before exiting
    SDL_DestroyRenderer(machine->renderer);
    SDL_DestroyWindow(machine->window);
    SDL_Quit();

    return 0;
}
