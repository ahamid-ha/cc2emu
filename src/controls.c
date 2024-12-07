#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include "controls.h"
#include "nk_sdl.h"

struct {
    struct nk_context *ctx;
    struct machine_status *machine;

    struct {
        char *path;
    } disks[4];

    char *cartridge_path;
    char *cassette_path;

    float joystick_selection;

    bool settings_window_state;

    enum nk_collapse_states settings_cartridge_state;
    enum nk_collapse_states settings_disks_state;
    enum nk_collapse_states settings_cassette_state;
} controls;

void error_msg(const char *msg) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", msg, controls.machine->window);
}

void error_general_file(const char *path) {
    char error_buffer[2000];
    snprintf(error_buffer, sizeof(error_buffer), "Error opening file '%s': %s", path, strerror(errno));
    error_msg(error_buffer);
}

void controls_init(struct machine_status *machine) {
    memset(&controls, 0, sizeof(controls));
    controls.ctx = nk_sdl_init(machine->window, machine->renderer);
    controls.machine = machine;

    // TODO: icons using https://wiki.libsdl.org/SDL3/SDL_LoadBMP or https://github.com/libsdl-org/SDL_image
}

void controls_reinit(void) {
    nk_sdl_update_renderer(controls.machine->window, controls.machine->renderer);
}

void _settings_close_window()
{
    controls.settings_window_state = false;
}

void _settings_open_window(bool open_all)
{
    enum nk_collapse_states section_state = open_all ? NK_MAXIMIZED : NK_MINIMIZED;

    controls.settings_cartridge_state = section_state;
    controls.settings_disks_state = section_state;
    controls.settings_cassette_state = section_state;

    controls.settings_window_state = true;
}

void _settings_toggle_window(bool open_all)
{
    if (controls.settings_window_state) {
        _settings_close_window();
    } else {
        _settings_open_window(open_all);
    }
}

static void SDLCALL _disk_selection_cb(void* data, const char* const* filelist, int filter)
{
    int disk_no = (intptr_t)data;
    if (!filelist) {
        fprintf(stderr, "An error occured: %s", SDL_GetError());
        return;
    } else if (!*filelist) {
        return;
    }

    const char *rom_path = *filelist;
    if(!disk_drive_load_disk(controls.machine->disk_drive, disk_no, rom_path)) {
        if (controls.disks[disk_no].path) free(controls.disks[disk_no].path);
        controls.disks[disk_no].path = strdup(rom_path);
    }
}

static void SDLCALL _cassette_selection_cb(void* data, const char* const* filelist, int filter)
{
    if (!filelist) {
        fprintf(stderr, "An error occured: %s", SDL_GetError());
        return;
    } else if (!*filelist) {
        return;
    }

    const char *rom_path = *filelist;
    if(!adc_load_cassette(controls.machine->adc, rom_path)) {
        if (controls.cassette_path) free(controls.cassette_path);
        controls.cassette_path = strdup(rom_path);
    }
}

static void SDLCALL _cartridge_selection_cb(void* data, const char* const* filelist, int filter)
{
    if (!filelist) {
        fprintf(stderr, "An error occured: %s", SDL_GetError());
        return;
    } else if (!*filelist) {
        return;
    }

    const char *rom_path = *filelist;
    machine_reset(controls.machine);

    if(!sam_load_rom(controls.machine->sam, 2, rom_path)) {
        controls.machine->cart_sense = 1;
        if (controls.cartridge_path) free(controls.cartridge_path);
        controls.cartridge_path = strdup(rom_path);
    }
}

static void SDLCALL _disk_new_cb(void* data, const char* const* filelist, int filter)
{
    int disk_no = (intptr_t)data;
    if (!filelist) {
        fprintf(stderr, "An error occured: %s", SDL_GetError());
        return;
    } else if (!*filelist) {
        return;
    }

    const char *rom_path = *filelist;

    if(disk_drive_load_disk(controls.machine->disk_drive, disk_no, rom_path)) {
        if (controls.disks[disk_no].path) free(controls.disks[disk_no].path);
        controls.disks[disk_no].path = strdup(rom_path);
    }
}

void _settings_window_display() {
    int window_w, window_h;
    SDL_GetWindowSizeInPixels(controls.machine->window, &window_w, &window_h);
    struct nk_style_button button_style_original = controls.ctx->style.button;
    if(nk_begin(controls.ctx, "Settings", nk_rect(50, 50, window_w - 100, window_h - 100), NK_WINDOW_BORDER | NK_WINDOW_TITLE))
    {
        if (nk_tree_state_push(controls.ctx, NK_TREE_NODE, "Cartridge", &controls.settings_cartridge_state)) {
            nk_layout_row_template_begin(controls.ctx, 30);
            nk_layout_row_template_push_dynamic(controls.ctx);
            nk_layout_row_template_push_static(controls.ctx, 50);
            nk_layout_row_template_push_static(controls.ctx, 50);
            nk_layout_row_template_end(controls.ctx);
            nk_label(controls.ctx, controls.cartridge_path ? controls.cartridge_path : "<Empty>", NK_TEXT_LEFT);
            if (nk_button_label(controls.ctx, "Load")) {
                SDL_ShowOpenFileDialog(_cartridge_selection_cb, (void*)NULL, controls.machine->window, NULL, 0, "roms/cartridges", false);
            }
            if (nk_button_label(controls.ctx, "Unload")) {
                if (controls.cartridge_path) free(controls.cartridge_path);
                controls.cartridge_path = NULL;
                keyboard_buffer_reset();
                machine_reset(controls.machine);
                controls.machine->cart_sense = 0;
                disk_drive_reset(controls.machine->disk_drive);
            }
            nk_tree_state_pop(controls.ctx);
        }

        if (nk_tree_state_push(controls.ctx, NK_TREE_NODE, "Disks", &controls.settings_disks_state)) {
            nk_layout_row_template_begin(controls.ctx, 30);
            nk_layout_row_template_push_static(controls.ctx, 15);
            nk_layout_row_template_push_dynamic(controls.ctx);
            nk_layout_row_template_push_static(controls.ctx, 50);
            nk_layout_row_template_push_static(controls.ctx, 50);
            nk_layout_row_template_push_static(controls.ctx, 50);
            nk_layout_row_template_end(controls.ctx);

            for (int disk_no=0; disk_no < 4; disk_no++) {
                char disk_label[3] = {'1' + disk_no, '.', 0};

                nk_label(controls.ctx, disk_label, NK_TEXT_LEFT);
                nk_label(controls.ctx, controls.disks[disk_no].path ? controls.disks[disk_no].path : "<Empty>", NK_TEXT_LEFT);

                if (nk_button_label(controls.ctx, "New")) {
                    SDL_ShowSaveFileDialog(_disk_new_cb, (void*)((intptr_t)disk_no), controls.machine->window, NULL, 0, "roms/cartridges");
                }

                if (nk_button_label(controls.ctx, "Load")) {
                    SDL_ShowOpenFileDialog(_disk_selection_cb, (void*)((intptr_t)disk_no), controls.machine->window, NULL, 0, "roms/cartridges", false);
                }

                if (nk_button_label(controls.ctx, "Unload")) {
                    if (controls.disks[disk_no].path) free(controls.disks[disk_no].path);
                    controls.disks[disk_no].path = NULL;
                    disk_drive_load_disk(controls.machine->disk_drive, disk_no, NULL);
                }

            }
            nk_tree_state_pop(controls.ctx);
        }

        if (nk_tree_state_push(controls.ctx, NK_TREE_NODE, "Cassette", &controls.settings_cassette_state)) {
            nk_layout_row_template_begin(controls.ctx, 30);
            nk_layout_row_template_push_dynamic(controls.ctx);
            nk_layout_row_template_push_static(controls.ctx, 80);
            nk_layout_row_template_push_static(controls.ctx, 50);
            nk_layout_row_template_end(controls.ctx);
            nk_label(controls.ctx, controls.cassette_path ? controls.cassette_path : "<Empty>", NK_TEXT_LEFT);
            if (nk_button_label(controls.ctx, "Load")) {
                SDL_ShowOpenFileDialog(_cassette_selection_cb, (void*)NULL, controls.machine->window, NULL, 0, "roms/cartridges", false);
            }
            if (nk_button_label(controls.ctx, "Unload")) {
                if (controls.cassette_path) free(controls.cassette_path);
                controls.cassette_path = NULL;
                adc_load_cassette(controls.machine->adc, NULL);
            }
            nk_slider_int(controls.ctx, 0, &controls.machine->adc->cassette_audio_location, controls.machine->adc->cassette_audio_len, 1);
            if (controls.machine->adc->cassette_motor) {
                controls.ctx->style.button.normal = controls.ctx->style.button.active;
                controls.ctx->style.button.hover = controls.ctx->style.button.active;
            }
            if (nk_button_label(controls.ctx, "Play/Stop")) {
                controls.machine->adc->cassette_motor = !controls.machine->adc->cassette_motor;
            }
            controls.ctx->style.button = button_style_original;
            if (nk_button_label(controls.ctx, "Rewind")) {
                controls.machine->adc->cassette_audio_location = 0;
            }
            nk_tree_state_pop(controls.ctx);
        }
    }
    nk_end(controls.ctx);
}

void controls_display() {
    int window_w, window_h;
    SDL_GetWindowSizeInPixels(controls.machine->window, &window_w, &window_h);
    if (nk_begin(controls.ctx, "tool bar", nk_rect(0, window_h - 40, window_w, 40), NK_WINDOW_NO_SCROLLBAR))
    {
        nk_layout_row_static(controls.ctx, 30, 80, 8);
        if (nk_button_label(controls.ctx, "Clear")) {
            keyboard_buffer_reset();
            machine_send_key(SDLK_CLEAR);
        }

        if (nk_button_label(controls.ctx, "Break")) {
            keyboard_buffer_reset();
            machine_send_key(SDLK_ESCAPE);
        }

        if (nk_button_label(controls.ctx, "Reset")) {
            if (controls.cartridge_path) free(controls.cartridge_path);
            controls.cartridge_path = NULL;
            keyboard_buffer_reset();
            machine_reset(controls.machine);
            controls.machine->cart_sense = 0;
            disk_drive_reset(controls.machine->disk_drive);
        }

        struct nk_color button_border_color = nk_rgba(0,0,0,255);
        if (controls.machine->disk_drive->status_1.BUSY) button_border_color = nk_rgba(255,0,0,255);
        else if (controls.machine->disk_drive->MOTOR_ON) button_border_color = nk_rgba(0,255,0,255);
        else if (controls.machine->disk_drive->_drive_data[0]) button_border_color = nk_rgba(255,255,255,255);
        nk_style_push_color(controls.ctx, &controls.ctx->style.button.border_color, button_border_color);
        if (nk_button_label(controls.ctx, "Diskette")) {
            _settings_toggle_window(false);
            controls.settings_disks_state = NK_MAXIMIZED;
        }
        nk_style_pop_color(controls.ctx);

        button_border_color = nk_rgba(0,0,0,255);
        if (controls.machine->adc->cassette_motor) button_border_color = nk_rgba(255,0,0,255);
        else if (controls.cassette_path) button_border_color = nk_rgba(255,255,255,255);
        nk_style_push_color(controls.ctx, &controls.ctx->style.button.border_color, button_border_color);
        if (nk_button_label(controls.ctx, "Cassette")) {
            _settings_toggle_window(false);
            controls.settings_cassette_state = NK_MAXIMIZED;
        }
        nk_style_pop_color(controls.ctx);

        button_border_color = nk_rgba(0,0,0,255);
        // visual indication that the left joystick is being pulled
        if (!controls.machine->adc->sound_enabled && controls.joystick_selection != controls.machine->adc->adc_level && controls.machine->adc->switch_selection < 2) button_border_color = nk_rgba(255,255,255,255);
        nk_style_push_color(controls.ctx, &controls.ctx->style.button.border_color, button_border_color);
        if (nk_button_label(controls.ctx, "Left Joy")) {
            controls.settings_window_state = !controls.settings_window_state;
        }
        nk_style_pop_color(controls.ctx);
        button_border_color = nk_rgba(0,0,0,255);
        // visual indication that the right joystick is being pulled
        if (controls.joystick_selection != controls.machine->adc->adc_level && controls.machine->adc->switch_selection > 1) button_border_color = nk_rgba(255,255,255,255);
        nk_style_push_color(controls.ctx, &controls.ctx->style.button.border_color, button_border_color);
        if (nk_button_label(controls.ctx, "Right Joy")) {
            controls.settings_window_state = !controls.settings_window_state;
        }
        nk_style_pop_color(controls.ctx);
        controls.joystick_selection = controls.machine->adc->adc_level;

        if (nk_button_label(controls.ctx, "Settings")) {
            _settings_toggle_window(true);
        }

    }
    nk_end(controls.ctx);

    if (controls.settings_window_state){
        _settings_window_display();
    }

    nk_sdl_render(NK_ANTI_ALIASING_ON);
}
