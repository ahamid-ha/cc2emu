#include "controls.h"
#include "nk_sdl.h"

struct {
    struct nk_context *ctx;
} controls;

void controls_init(struct machine_status *machine) {
    controls.ctx = nk_sdl_init(machine->window, machine->renderer);
}

void controls_display(struct machine_status *machine) {
    int window_w, window_h;
    SDL_GetWindowSizeInPixels(machine->window, &window_w, &window_h);
    if (nk_begin(controls.ctx, "tool bar", nk_rect(0, window_h - 40, window_w, 40), NK_WINDOW_NO_SCROLLBAR))
    {
        nk_layout_row_static(controls.ctx, 30, 80, 3);
        if (nk_button_label(controls.ctx, "Clear"))
            machine_send_key(SDLK_CLEAR);

        if (nk_button_label(controls.ctx, "Break"))
            machine_send_key(SDLK_ESCAPE);

        if (nk_button_label(controls.ctx, "Reset")) {
            machine_reset(machine);
            machine->cart_sense = 0;
            disk_drive_reset(machine->disk_drive);
        }
    }
    nk_end(controls.ctx);

    nk_sdl_render(NK_ANTI_ALIASING_ON);
}
