#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "adc.h"

void _adc_process(struct adc_status *adc) {
    int compare = 0;

    switch(adc->switch_selection) {
        case 0: compare = adc->adc_level < adc->input_joy_0; break;
        case 1: compare = adc->adc_level < adc->input_joy_1; break;
        case 2: compare = adc->adc_level < adc->input_joy_2; break;
        case 3: compare = adc->adc_level < adc->input_joy_3; break;
    }

    mc6821_peripheral_input(adc->pia1, 0, compare ? 0x80 : 0, 0x80);
}

void _adc_level_change_cb(struct mc6821_status *pia, int peripheral_address, uint8_t value, void *data) {
    struct adc_status *adc = (struct adc_status *)data;

    adc->adc_level = 0;
    if (value & 0b10000000) adc->adc_level += 2.25;
    if (value & 0b01000000) adc->adc_level += 1.125;
    if (value & 0b00100000) adc->adc_level += 0.563;
    if (value & 0b00010000) adc->adc_level += 0.281;
    if (value & 0b00001000) adc->adc_level += 0.14;
    if (value & 0b00000100) adc->adc_level += 0.07;

    _adc_process(adc);
}

void _adc_source_a_change_cb(struct mc6821_status *pia, int peripheral_address, uint8_t value, void *data) {
    struct adc_status *adc = (struct adc_status *)data;

    adc->switch_selection &= 0b10;
    adc->switch_selection |= value ? 1 : 0;

    _adc_process(adc);
}

void _adc_source_b_change_cb(struct mc6821_status *pia, int peripheral_address, uint8_t value, void *data) {
    struct adc_status *adc = (struct adc_status *)data;

    adc->switch_selection &= 0b1;
    adc->switch_selection |= value ? 0b10 : 0;

   _adc_process(adc);
}

struct adc_status *adc_initialize(struct mc6821_status *pia1, struct mc6821_status *pia2) {
    struct adc_status *adc=malloc(sizeof(struct adc_status));
    memset(adc, 0, sizeof(struct adc_status));
    adc->pia1 = pia1;

    adc->input_joy_0 = 2.5;
    adc->input_joy_1 = 2.5;
    adc->input_joy_2 = 2.5;
    adc->input_joy_3 = 2.5;

    mc6821_register_c2_cb(pia1, 0, (mc6821_cb)_adc_source_a_change_cb, adc);
    mc6821_register_c2_cb(pia1, 1, (mc6821_cb)_adc_source_b_change_cb, adc);
    mc6821_register_cb(pia2, 0, (mc6821_cb)_adc_level_change_cb, adc);

    return adc;
}
