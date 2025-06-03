#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <execinfo.h>
#endif
#include <signal.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>
#include "machine.h"
#include "controls.h"
#include "utils.h"
#include "nk_sdl.h"
#include "settings.h"


#ifndef _WIN32
void segv_handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, fileno(stderr));
  exit(1);
}
#endif

int main(int argc, char* argv[]) {
#ifndef _WIN32
    signal(SIGSEGV, segv_handler);
#endif

    init_utils();

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
        log_message(LOG_ERROR, "Error initializing SDL: %s", SDL_GetError());
        return -1;
    }

    settings_init();

    struct machine_status *machine = malloc(sizeof(struct machine_status));
    memset(machine, 0, sizeof(struct machine_status));

    // Create a window and renderer
    machine->window = SDL_CreateWindow("Emulator", 256 * 4 + 40, 192 * 4 + 40 + 40, 0);
    if (!machine->window) {
        log_message(LOG_ERROR, "Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // Renderer for drawing graphics
    machine->renderer = SDL_CreateRenderer(machine->window, NULL);
    if (!machine->renderer) {
        log_message(LOG_ERROR, "Failed to create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(machine->window);
        SDL_Quit();
        return -1;
    }

    if(!SDL_SetRenderVSync(machine->renderer, -1)) {
        log_message(LOG_INFO,  "Could not enable adaptive VSync, SDL error: %s. Trying VSync instead.", SDL_GetError());
        if(!SDL_SetRenderVSync(machine->renderer, 1)) {
            log_message(LOG_ERROR,  "Could not enable VSync! SDL error: %s", SDL_GetError() );
        }
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

        if(machine_process_frame(machine)) {
            video_reinitialize(machine->video, machine->renderer);
            controls_reinit();
        }

        controls_display();

        uint64_t time_ns = nanos();
        if (time_ns > machine->p._virtual_time_nano && time_ns - machine->p._virtual_time_nano > SEC_TO_NS(1)) {
            // we are out of sync, so re-sync the processor time
            log_message(LOG_INFO, "re-sync the processor time %ld", time_ns - machine->p._virtual_time_nano);
            machine->p._virtual_time_nano = time_ns + 1;
        }

        machine_handle_input_begin(machine);

        controls_input_begin();

        // Update the renderer
        SDL_RenderPresent(machine->renderer);

        // Handle events
        SDL_Event event;
        do {
            while(SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }

                if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
                    video_reinitialize(machine->video, machine->renderer);
                    controls_reinit();
                }

                if (!machine_handle_input(machine, &event)) {
                    // event not handled, so pass it to the gui controls
                    nk_sdl_handle_event(&event);
                }

                if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F10) {
                    machine_reset(machine);
                    machine->cart_sense = 0;
                    disk_drive_reset(machine->disk_drive);
                }
                time_ns = nanos();
            };
        } while (machine->p._virtual_time_nano > time_ns && SDL_WaitEventTimeout(NULL, 5));

        controls_input_end();
    }

    // Clean up resources before exiting
    SDL_DestroyRenderer(machine->renderer);
    SDL_DestroyWindow(machine->window);
    SDL_Quit();

    return 0;
}
