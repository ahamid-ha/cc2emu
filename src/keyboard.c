#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "keyboard.h"


void _keyboard_column_change_cb(struct mc6821_status *pia, int peripheral_address, uint8_t columns, void *data) {
    struct keyboard_status *kb = (struct keyboard_status *)data;
    uint8_t value = 0x7f;
    for (int row=0; row < 7; row++) {
        for (int col=0; col < 8; col++) {
            if (((1 << col) & columns) == 0 && kb->keyboard_keys_status[row][col]) {
                value &= ~(1 << row);
            }
        }
    }
    // printf("Keyboard input %02X out %02X\n", columns, value);

    // send the row values
    mc6821_peripheral_input(pia, 0, value, 0x7f);
}

struct keyboard_status *keyboard_initialize(struct mc6821_status *pia) {
    struct keyboard_status *ks=malloc(sizeof(struct keyboard_status));
    memset(ks, 0, sizeof(struct keyboard_status));

    mc6821_register_cb(pia, 1, (mc6821_cb)_keyboard_column_change_cb, ks);

    return ks;
}


#define kb_map(sym, row, column) case sym: ks->keyboard_keys_status[row][column] = is_pressed; break;
#define kb_map_unshifted(sym, row, column) case sym: ks->keyboard_keys_status[row][column] = is_pressed; ks->keyboard_keys_status[6][7] = 0; break;
#define kb_map_shifted(sym, row, column) case sym: ks->keyboard_keys_status[row][column] = is_pressed; ks->keyboard_keys_status[6][7] = is_pressed; break;

void keyboard_set_char(struct keyboard_status *ks, int sym, int is_pressed) {
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

        kb_map(SDLK_z, 3, 2)
        kb_map(SDLK_y, 3, 1)
        kb_map(SDLK_x, 3, 0)
        kb_map(SDLK_w, 2, 7)
        kb_map(SDLK_v, 2, 6)
        kb_map(SDLK_u, 2, 5)
        kb_map(SDLK_t, 2, 4)
        kb_map(SDLK_s, 2, 3)
        kb_map(SDLK_r, 2, 2)
        kb_map(SDLK_q, 2, 1)
        kb_map(SDLK_p, 2, 0)
        kb_map(SDLK_o, 1, 7)
        kb_map(SDLK_n, 1, 6)
        kb_map(SDLK_m, 1, 5)
        kb_map(SDLK_l, 1, 4)
        kb_map(SDLK_k, 1, 3)
        kb_map(SDLK_j, 1, 2)
        kb_map(SDLK_i, 1, 1)
        kb_map(SDLK_h, 1, 0)
        kb_map(SDLK_g, 0, 7)
        kb_map(SDLK_f, 0, 6)
        kb_map(SDLK_e, 0, 5)
        kb_map(SDLK_d, 0, 4)
        kb_map(SDLK_c, 0, 3)
        kb_map(SDLK_b, 0, 2)
        kb_map(SDLK_a, 0, 1)

        kb_map(SDLK_RETURN, 6, 0)
        kb_map(SDLK_SPACE, 3, 7)
    }
}

#define kb_map_symbol(sym, unshifted, shifted) case sym: if (event->keysym.mod & KMOD_SHIFT) keyboard_set_char(ks, shifted, is_pressed); else keyboard_set_char(ks, unshifted, is_pressed); break;

void keyboard_set_key(struct keyboard_status *ks, SDL_KeyboardEvent *event, int is_pressed) {
    int sym = event->keysym.sym;

    switch(sym) {
        kb_map(SDLK_LSHIFT, 6, 7)
        kb_map(SDLK_RSHIFT, 6, 7)
        kb_map(SDLK_ESCAPE, 6, 2)
        kb_map(SDLK_F1, 6, 1)
        kb_map_symbol(SDLK_EQUALS, '=', '+')
        kb_map_symbol(SDLK_SLASH, '/', '?')
        kb_map_symbol(SDLK_PERIOD, '.', '>')
        // kb_map_symbol(SDLK_MINUS, '-', '_')
        kb_map_symbol(SDLK_COMMA, ',', '<')
        kb_map_symbol(SDLK_SEMICOLON, ';', ':')
        kb_map_symbol(SDLK_QUOTE, '\'', '"')
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
            keyboard_set_char(ks, sym, is_pressed);
    }
}
