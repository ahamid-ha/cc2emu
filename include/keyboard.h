#include <inttypes.h>
#include "mc6821.h"


struct keyboard_status {
    uint8_t keyboard_keys_status[7][8];
    uint8_t last_columns_value;
    uint8_t columns_used;     // this marks which columns were scanned since last key press/release
    struct mc6821_status *pia;
};

int keyboard_set_key(struct keyboard_status *ks, SDL_KeyboardEvent *event, int is_pressed);
struct keyboard_status *keyboard_initialize(struct mc6821_status *pia);
