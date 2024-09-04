#include <inttypes.h>
#include "mc6821.h"


struct adc_status {
    float adc_level;
    uint8_t switch_selection;

    float input_joy_0;
    float input_joy_1;
    float input_joy_2;
    float input_joy_3;

    struct mc6821_status *pia1;
};

struct adc_status *adc_initialize(struct mc6821_status *pia1, struct mc6821_status *pia2);
