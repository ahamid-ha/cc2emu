#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>
#include <time.h>
#include "processor_6809.h"

#define kb_map(sym, row, column) case sym: data->keyboard_keys_status[row][column] = is_pressed; break;

void pia1_set_key(struct bus_adaptor *p, int sym, int is_pressed) {
    struct pia1_status *data = (struct pia1_status *)p->data;

    switch(sym) {
        kb_map(SDLK_w, 2, 7)
        kb_map(SDLK_v, 2, 6)
        kb_map(SDLK_u, 2, 5)
        kb_map(SDLK_t, 2, 4)
        kb_map(SDLK_s, 2, 3)
        kb_map(SDLK_r, 2, 2)
        kb_map(SDLK_q, 2, 1)
        kb_map(SDLK_p, 2, 0)
    }
}

#define SEC_TO_NS(sec) ((sec)*1000000000)
uint64_t nanos()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    uint64_t ns = SEC_TO_NS((uint64_t)ts.tv_sec) + (uint64_t)ts.tv_nsec;
    return ns;
}

TTF_Font* font;

char display_chars[] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]  "
    " !\"#$%&'()*+,-./0123456789:;<=>?";
void render_text(uint8_t *memory, SDL_Renderer* renderer) {
    char text_line[33];
    SDL_Color foreground = { 255, 255, 255 };
    SDL_Rect dest;

    for (int line=0; line < 16; line++) {
        for (int col=0; col < 32; col++) {
            uint8_t data = memory[1024 + (line * 32) + col];

            uint8_t display_char = display_chars[data & 63];
            if (data >= 128) display_char = 32;
            // printf("%02x%c ", data, display_char);
            // putc(display_char, stdout);
            text_line[col] = display_char;
        }
        text_line[33] = 0;

        SDL_Surface* text_surf = TTF_RenderText_Solid(font, text_line, foreground);
		SDL_Texture *text = SDL_CreateTextureFromSurface(renderer, text_surf);

		dest.x = 20;
		dest.y = 20 * line;
		dest.w = text_surf->w;
		dest.h = text_surf->h;
		SDL_RenderCopy(renderer, text, NULL, &dest);

		SDL_DestroyTexture(text);
		SDL_FreeSurface(text_surf);
    }
}

int main2(int argc, char* argv[]) {
    uint8_t a= 3;
    uint8_t b= 5;
    uint16_t num = a - b;
    printf("value = %d\n", num);
}

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Error initializing SDL: %s\n", SDL_GetError());
        return -1;
    }

    if ( TTF_Init() < 0 ) {
		printf("Error initializing TTF %s\n", TTF_GetError());
        SDL_Quit();
		return -1;
	}

    // Create a window and renderer
    SDL_Window* window = SDL_CreateWindow("Simple Graphics", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // Renderer for drawing graphics
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        printf("Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    font = TTF_OpenFont("/usr/share/fonts/TTF/NotoMonoNerdFont-Regular.ttf", 20);
	if ( !font ) {
		printf("Error creating font %s\n", TTF_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
		return -1;
	}

    bool running = true;

    struct processor_state p;
    processor_init(&p);

    struct bus_adaptor *basic_rom = bus_create_rom("roms/BASIC.ROM", 0xA000);
    processor_register_bus_adaptor(&p.bus, basic_rom);

    // struct bus_adaptor *extended_rom = bus_create_rom('roms/extbas11.rom', 0x8000);
    // processor_register_bus_adaptor(&p.bus, extended_rom);

    struct bus_adaptor *ram = bus_create_ram(16 * 1024, 0x0000);
    processor_register_bus_adaptor(&p.bus, ram);
    uint8_t *memory_buffer = (uint8_t *)ram->data;

    struct bus_adaptor *pia1 = bus_create_pia1();
    processor_register_bus_adaptor(&p.bus, pia1);

    struct bus_adaptor *pia2 = bus_create_pia2();
    processor_register_bus_adaptor(&p.bus, pia2);

    processor_reset(&p);

    while (running) {
        // Handle events
        SDL_Event event;

        uint64_t next_frame_ns = nanos() + frame_time_nano;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q)) {
                running = false;
            }

            if (event.type == SDL_KEYDOWN) {
                pia1_set_key(pia1, event.key.keysym.sym, 1);
            }
            if (event.type == SDL_KEYUP) {
                pia1_set_key(pia1, event.key.keysym.sym, 0);
            }
        }

        p._nano_time_passed = 0;
        while (p._nano_time_passed < frame_time_nano) {
            processor_next_opcode(&p);
        }

        // Clear the renderer with a black color
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Draw something (in this case, white lines)
        // for (int i = 10; i < 630; i += 20) {
        //     SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        //     SDL_RenderDrawLine(renderer, i, 0, i, 480);
        // }

        render_text(memory_buffer, renderer);

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
    }

    // Clean up resources before exiting
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
