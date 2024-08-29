#include <inttypes.h>
#include "mc6821.h"


struct keyboard_status {
    uint8_t keyboard_keys_status[7][8];
};

void keyboard_set_key(struct keyboard_status *ks, SDL_KeyboardEvent *event, int is_pressed);
struct keyboard_status *keyboard_initialize(struct mc6821_status *pia);
