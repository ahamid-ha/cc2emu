#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "keyboard.h"


void _keyboard_column_change_cb(struct mc6821_status *pia, int peripheral_address, uint8_t columns, void *data) {
    struct keyboard_status *kb = (struct keyboard_status *)data;
    uint8_t value = 0x7f;

    if (kb->last_columns_value != columns) kb->columns_used++;

    kb->last_columns_value = columns;

    for (uint8_t row=0; row < 7; row++) {
        for (uint8_t col=0; col < 8; col++) {
            if (((1 << col) & columns) == 0 && kb->keyboard_keys_status[row][col]) {
                value &= ~(1 << row);
            }
        }
    }
    value &= kb->other_inputs;
    // printf("Keyboard input %02X out %02X\n", columns, value);

    // send the row values
    mc6821_peripheral_input(pia, 0, value, 0x7f);
}

void keyboard_reset(struct keyboard_status *ks) {
    memset(ks->keyboard_keys_status, 0, sizeof(ks->keyboard_keys_status));
    ks->other_inputs = 0xff;
    ks->last_columns_value = 0;
    ks->columns_used = 0;
}

struct keyboard_status *keyboard_initialize(struct mc6821_status *pia) {
    struct keyboard_status *ks=malloc(sizeof(struct keyboard_status));
    memset(ks, 0, sizeof(struct keyboard_status));
    ks->other_inputs = 0xff;
    ks->pia = pia;

    mc6821_register_cb(pia, 1, (mc6821_cb)_keyboard_column_change_cb, ks);

    return ks;
}

#define kb_map(sym, row, column) case sym: ks->keyboard_keys_status[row][column] = is_pressed; ret=1; break;
#define kb_map_unshifted(sym, row, column) case sym: ks->keyboard_keys_status[row][column] = is_pressed; ks->keyboard_keys_status[6][7] = 0; ret=1; break;
#define kb_map_shifted(sym, row, column) case sym: ks->keyboard_keys_status[row][column] = is_pressed; ks->keyboard_keys_status[6][7] = is_pressed; ret=1; break;

int keyboard_set_char(struct keyboard_status *ks, int sym, int is_pressed) {
    int ret = 0;

    switch(sym) {
        kb_map_unshifted('0', 4, 0)

        kb_map_unshifted('1', 4, 1)
        kb_map_shifted('!', 4, 1)

        kb_map_unshifted('2', 4, 2)
        kb_map_shifted('"', 4, 2)

        kb_map_unshifted('3', 4, 3)
        kb_map_shifted('#', 4, 3)

        kb_map_unshifted('4', 4, 4)
        kb_map_shifted('$', 4, 4)

        kb_map_unshifted('5', 4, 5)
        kb_map_shifted('%', 4, 5)

        kb_map_unshifted('6', 4, 6)
        kb_map_shifted('&', 4, 6)

        kb_map_unshifted('7', 4, 7)
        kb_map_shifted('\'', 4, 7)

        kb_map_unshifted('8', 4, 8)
        kb_map_shifted('(', 4, 8)

        kb_map_unshifted('9', 4, 9)
        kb_map_shifted(')', 4, 9)

        kb_map_unshifted('-', 5, 5)
        kb_map_shifted('=', 5, 5)

        kb_map_unshifted(';', 5, 3)
        kb_map_shifted('+', 5, 3)

        kb_map_unshifted(':', 5, 2)
        kb_map_shifted('*', 5, 2)

        kb_map_unshifted(',', 5, 4)
        kb_map_shifted('<', 5, 4)

        kb_map_unshifted('.', 5, 6)
        kb_map_shifted('>', 5, 6)

        kb_map_unshifted('/', 5, 7)
        kb_map_shifted('?', 5, 7)

        kb_map_unshifted('@', 0, 0)

        kb_map(SDLK_Z, 3, 2)
        kb_map(SDLK_Y, 3, 1)
        kb_map(SDLK_X, 3, 0)
        kb_map(SDLK_W, 2, 7)
        kb_map(SDLK_V, 2, 6)
        kb_map(SDLK_U, 2, 5)
        kb_map(SDLK_T, 2, 4)
        kb_map(SDLK_S, 2, 3)
        kb_map(SDLK_R, 2, 2)
        kb_map(SDLK_Q, 2, 1)
        kb_map(SDLK_P, 2, 0)
        kb_map(SDLK_O, 1, 7)
        kb_map(SDLK_N, 1, 6)
        kb_map(SDLK_M, 1, 5)
        kb_map(SDLK_L, 1, 4)
        kb_map(SDLK_K, 1, 3)
        kb_map(SDLK_J, 1, 2)
        kb_map(SDLK_I, 1, 1)
        kb_map(SDLK_H, 1, 0)
        kb_map(SDLK_G, 0, 7)
        kb_map(SDLK_F, 0, 6)
        kb_map(SDLK_E, 0, 5)
        kb_map(SDLK_D, 0, 4)
        kb_map(SDLK_C, 0, 3)
        kb_map(SDLK_B, 0, 2)
        kb_map(SDLK_A, 0, 1)

        kb_map(SDLK_RETURN, 6, 0)
        kb_map('\n', 6, 0)
        kb_map(SDLK_SPACE, 3, 7)
    }

    return ret;
}

#define kb_map_symbol(sym, unshifted, shifted) case sym: if (event->mod & SDL_KMOD_SHIFT) ret=keyboard_set_char(ks, shifted, is_pressed); else ret=keyboard_set_char(ks, unshifted, is_pressed); break;

int keyboard_set_key(struct keyboard_status *ks, SDL_KeyboardEvent *event, int is_pressed) {
    int sym = event->key;
    int ret = 0;

    switch(sym) {
        kb_map(SDLK_LSHIFT, 6, 7)
        kb_map(SDLK_RSHIFT, 6, 7)
        kb_map(SDLK_ESCAPE, 6, 2)
        // kb_map(SDLK_F10, 6, 2)    // break
        kb_map_shifted(SDLK_F2, 4, 0)    // Upper case
        kb_map(SDLK_F1, 6, 1)    // clr
        kb_map_symbol(SDLK_EQUALS, '=', '+')
        kb_map_symbol(SDLK_SLASH, '/', '?')
        kb_map_symbol(SDLK_PERIOD, '.', '>')
        kb_map_symbol(SDLK_COMMA, ',', '<')
        kb_map_symbol(SDLK_SEMICOLON, ';', ':')
        kb_map_symbol(SDLK_APOSTROPHE, '\'', '"')
        kb_map_symbol(SDLK_9, '9', '(')
        kb_map_symbol(SDLK_8, '8', '*')
        kb_map_symbol(SDLK_7, '7', '&')
        kb_map_symbol(SDLK_6, '6', '^')
        kb_map_symbol(SDLK_5, '5', '%')
        kb_map_symbol(SDLK_4, '4', '$')
        kb_map_symbol(SDLK_3, '3', '#')
        kb_map_symbol(SDLK_2, '2', '@')
        kb_map_symbol(SDLK_1, '1', '!')
        kb_map_symbol(SDLK_0, '0', ')')
        kb_map(SDLK_RIGHT, 3, 6)
        kb_map(SDLK_BACKSPACE, 3, 5)
        kb_map(SDLK_LEFT, 3, 5)
        kb_map(SDLK_DOWN, 3, 4)
        kb_map(SDLK_UP, 3, 3)
        default:
            ret = keyboard_set_char(ks, sym, is_pressed);
    }

    ks->columns_used = 0;
    if (ret) {
        // force updating the pia
        _keyboard_column_change_cb(ks->pia, 0, ks->last_columns_value, ks);
    }
    return ret;
}
