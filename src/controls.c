#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "controls.h"
#include "settings.h"
#include "nk_sdl.h"
#include "icons/joystick.xpm"
#include "icons/joystick_kbd.xpm"

struct {
    struct nk_context *ctx;
    struct machine_status *machine;

    float joystick_selection;  // to be able to track which joystick is accessed

    bool settings_window_state;

    enum nk_collapse_states settings_cartridge_state;
    enum nk_collapse_states settings_disks_state;
    enum nk_collapse_states settings_cassette_state;

    SDL_Texture *joystick_icon;
    SDL_Texture *joystick_kbd_icon;

    char *empty_value_place_holder;  // just a buffer that represents an empty buffer
} controls;

void error_msg(const char *msg) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", msg, controls.machine->window);
}

void error_general_file(const char *path) {
    char error_buffer[2000];
    snprintf(error_buffer, sizeof(error_buffer), "Error opening file '%s': %s", path, strerror(errno));
    error_msg(error_buffer);
}

SDL_Texture *init_icon_texture(char **icon) {
    SDL_Surface *surface = IMG_ReadXPMFromArray(icon);
    if (!surface) {
        SDL_Log("Couldn't load icon %s\n", SDL_GetError());
    }
    return SDL_CreateTextureFromSurface(controls.machine->renderer, surface);
}

void controls_init(struct machine_status *machine) {
    memset(&controls, 0, sizeof(controls));
    controls.ctx = nk_sdl_init(machine->window, machine->renderer);
    controls.machine = machine;
    controls.empty_value_place_holder = strdup("<Empty>");

    controls.joystick_icon = init_icon_texture(joystick_xpm);
    controls.joystick_kbd_icon = init_icon_texture(joystick_kbd_xpm);
}

void controls_reinit(void) {
    nk_sdl_update_renderer(controls.machine->window, controls.machine->renderer);

    SDL_DestroyTexture(controls.joystick_icon);
    controls.joystick_icon = init_icon_texture(joystick_xpm);

    SDL_DestroyTexture(controls.joystick_kbd_icon);
    controls.joystick_kbd_icon = init_icon_texture(joystick_kbd_xpm);
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

void machine_reset_and_save(void) {
    _settings_close_window();
    keyboard_buffer_reset();
    machine_reset(controls.machine);
    disk_drive_reset(controls.machine->disk_drive);
    settings_save();
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
        if (app_settings.disks[disk_no].path) free(app_settings.disks[disk_no].path);
        app_settings.disks[disk_no].path = strdup(rom_path);
        settings_save();
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
        if (app_settings.cassette_path) free(app_settings.cassette_path);
        app_settings.cassette_path = strdup(rom_path);
        settings_save();
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

    int rom_no = (intptr_t)data;

    const char *rom_path = *filelist;

    if(!sam_load_rom(controls.machine->sam, rom_no, rom_path)) {
        switch (rom_no) {
            case 2:
                controls.machine->cart_sense = 1;
                if (app_settings.cartridge_path) free(app_settings.cartridge_path);
                app_settings.cartridge_path = strdup(rom_path);
                break;
            case 1:
                if (app_settings.rom_basic_path) free(app_settings.rom_basic_path);
                app_settings.rom_basic_path = strdup(rom_path);
                break;
            case 0:
                if (app_settings.rom_extended_basic_path) free(app_settings.rom_extended_basic_path);
                app_settings.rom_extended_basic_path = strdup(rom_path);
                break;
            case 3:
                if (app_settings.rom_disc_basic_path) free(app_settings.rom_disc_basic_path);
                app_settings.rom_disc_basic_path = strdup(rom_path);
                break;
        }
        machine_reset_and_save();
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
        if (app_settings.disks[disk_no].path) free(app_settings.disks[disk_no].path);
        app_settings.disks[disk_no].path = strdup(rom_path);
        settings_save();
    }
}

int _input_with_actions(const char *label, char *value, ... /*actions*/) {
    int ret = 0;
    if (label)
        nk_label(controls.ctx, label, NK_TEXT_LEFT);
    if (!value) value = controls.empty_value_place_holder;
    nk_edit_string_zero_terminated(controls.ctx, NK_EDIT_READ_ONLY|NK_EDIT_SELECTABLE, value, strlen(value) + 1, nk_filter_default);

    va_list argptr;
    va_start (argptr, value);
    const char * control_label = va_arg (argptr, const char *);
    for (int control_n = 1; control_label; control_n++) {
        if (nk_button_label(controls.ctx, control_label)) {
            // the button was clicked
            ret = control_n;
        }
        control_label = va_arg (argptr, const char *);
    }
    va_end (argptr);
    return ret;
}

void _settings_window_display() {
    int window_w, window_h;
    SDL_GetWindowSizeInPixels(controls.machine->window, &window_w, &window_h);
    struct nk_style_button button_style_original = controls.ctx->style.button;
    if(nk_begin(controls.ctx, "Settings", nk_rect(50, 50, window_w - 100, window_h - 100), NK_WINDOW_BORDER | NK_WINDOW_TITLE))
    {
        if (nk_tree_state_push(controls.ctx, NK_TREE_NODE, "Video", &controls.settings_cartridge_state)) {
            int artifact_colors = app_settings.artifact_colors == cfg_true ? 1 : 0;
            nk_checkbox_label(controls.ctx, "Enable Artifact Colors", &artifact_colors);
            if (artifact_colors != (app_settings.artifact_colors == cfg_true ? 1 : 0)) {
                app_settings.artifact_colors = cfg_true ? artifact_colors : cfg_false;
                settings_save();
            }
            nk_tree_state_pop(controls.ctx);
        }

        if (nk_tree_state_push(controls.ctx, NK_TREE_NODE, "Rom", &controls.settings_cartridge_state)) {
            nk_layout_row_template_begin(controls.ctx, 30);
            nk_layout_row_template_push_static(controls.ctx, 100);
            nk_layout_row_template_push_dynamic(controls.ctx);
            nk_layout_row_template_push_static(controls.ctx, 50);
            nk_layout_row_template_push_static(controls.ctx, 50);
            nk_layout_row_template_end(controls.ctx);

            switch (_input_with_actions("Basic: ", app_settings.rom_basic_path, "Load", "Unload", NULL)) {
                case 1:
                    // Load
                    SDL_ShowOpenFileDialog(_cartridge_selection_cb, (void*)((intptr_t)1), controls.machine->window, NULL, 0, NULL, false);
                    break;
                case 2:
                    // Unload
                    if (app_settings.rom_basic_path) free(app_settings.rom_basic_path);
                    app_settings.rom_basic_path = NULL;
                    sam_unload_rom(controls.machine->sam, 1);
                    machine_reset_and_save();
                    break;
            }

            switch (_input_with_actions("Extended Basic: ", app_settings.rom_extended_basic_path, "Load", "Unload", NULL)) {
                case 1:
                    // Load
                    SDL_ShowOpenFileDialog(_cartridge_selection_cb, (void*)((intptr_t)0), controls.machine->window, NULL, 0, NULL, false);
                    break;
                case 2:
                    // Unload
                    if (app_settings.rom_extended_basic_path) free(app_settings.rom_extended_basic_path);
                    app_settings.rom_extended_basic_path = NULL;
                    sam_unload_rom(controls.machine->sam, 0);
                    machine_reset_and_save();
                    break;
            }

            switch (_input_with_actions("Disk Basic: ", app_settings.rom_disc_basic_path, "Load", "Unload", NULL)) {
                case 1:
                    // Load
                    SDL_ShowOpenFileDialog(_cartridge_selection_cb, (void*)((intptr_t)3), controls.machine->window, NULL, 0, NULL, false);
                    break;
                case 2:
                    // Unload
                    if (app_settings.rom_disc_basic_path) free(app_settings.rom_disc_basic_path);
                    app_settings.rom_disc_basic_path = NULL;
                    sam_unload_rom(controls.machine->sam, 3);
                    machine_reset_and_save();
                    break;
            }

            nk_tree_state_pop(controls.ctx);
        }

        if (nk_tree_state_push(controls.ctx, NK_TREE_NODE, "Cartridge", &controls.settings_cartridge_state)) {
            nk_layout_row_template_begin(controls.ctx, 30);
            nk_layout_row_template_push_dynamic(controls.ctx);
            nk_layout_row_template_push_static(controls.ctx, 50);
            nk_layout_row_template_push_static(controls.ctx, 50);
            nk_layout_row_template_end(controls.ctx);

            switch (_input_with_actions(NULL, app_settings.cartridge_path, "Load", "Unload", NULL)) {
                case 1:
                    // Load
                    SDL_ShowOpenFileDialog(_cartridge_selection_cb, (void*)((intptr_t)2), controls.machine->window, NULL, 0, NULL, false);
                    break;
                case 2:
                    // Unload
                    if (app_settings.cartridge_path) free(app_settings.cartridge_path);
                    app_settings.cartridge_path = NULL;
                    controls.machine->cart_sense = 0;
                    sam_unload_rom(controls.machine->sam, 2);
                    machine_reset_and_save();
                    break;
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

                switch (_input_with_actions(disk_label, app_settings.disks[disk_no].path, "New", "Load", "Unload", NULL)) {
                    case 1:
                        // New
                        SDL_ShowSaveFileDialog(_disk_new_cb, (void*)((intptr_t)disk_no), controls.machine->window, NULL, 0, NULL);
                        break;
                    case 2:
                        // Load
                        SDL_ShowOpenFileDialog(_disk_selection_cb, (void*)((intptr_t)disk_no), controls.machine->window, NULL, 0, NULL, false);
                        break;
                    case 3:
                        // Unload
                        if (app_settings.disks[disk_no].path) free(app_settings.disks[disk_no].path);
                        app_settings.disks[disk_no].path = NULL;
                        disk_drive_load_disk(controls.machine->disk_drive, disk_no, NULL);
                        settings_save();
                        break;
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

            switch (_input_with_actions(NULL, app_settings.cassette_path, "Load", "Unload", NULL)) {
                case 1:
                    // Load
                    SDL_ShowOpenFileDialog(_cassette_selection_cb, (void*)NULL, controls.machine->window, NULL, 0, NULL, false);
                    break;
                case 2:
                    // Unload
                    if (app_settings.cassette_path) free(app_settings.cassette_path);
                    app_settings.cassette_path = NULL;
                    adc_load_cassette(controls.machine->adc, NULL);
                    settings_save();
                    break;
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
            keyboard_buffer_reset();
            machine_reset(controls.machine);
            disk_drive_reset(controls.machine->disk_drive);
            _settings_close_window();
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
        else if (app_settings.cassette_path) button_border_color = nk_rgba(255,255,255,255);
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
        // if (nk_button_label(controls.ctx, "Left Joy")) {
        if (nk_button_image(controls.ctx, nk_image_ptr(controls.machine->_joy_emulation && controls.machine->_joy_emulation_side == 0 ? controls.joystick_kbd_icon : controls.joystick_icon))) {
            if (controls.machine->_joy_emulation && controls.machine->_joy_emulation_side == 0) {
                controls.machine->_joy_emulation = 0;
            } else {
                controls.machine->_joy_emulation = 1;
                controls.machine->_joy_emulation_side = 0;
            }
        }
        nk_style_pop_color(controls.ctx);
        button_border_color = nk_rgba(0,0,0,255);
        // visual indication that the right joystick is being pulled
        if (controls.joystick_selection != controls.machine->adc->adc_level && controls.machine->adc->switch_selection > 1) button_border_color = nk_rgba(255,255,255,255);
        nk_style_push_color(controls.ctx, &controls.ctx->style.button.border_color, button_border_color);
        if (nk_button_image(controls.ctx, nk_image_ptr(controls.machine->_joy_emulation && controls.machine->_joy_emulation_side == 1 ? controls.joystick_kbd_icon : controls.joystick_icon))) {
            if (controls.machine->_joy_emulation && controls.machine->_joy_emulation_side == 1) {
                controls.machine->_joy_emulation = 0;
            } else {
                controls.machine->_joy_emulation = 1;
                controls.machine->_joy_emulation_side = 1;
            }
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
