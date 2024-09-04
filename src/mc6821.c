#include "bus.h"
#include "mc6821.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define peripheral(peripheral_address) (peripheral_address == 0 ? &p->a : &p->b)

uint8_t mc6821_peripheral_read_register(struct mc6821_peripheral_status *p, int address) {
    if (address) {
        return p->cr;
    }
    if (p->ddr_access) {
        p->irq1 = 0;
        p->irq2 = 0;
        return p->pr;
    }

    return p->ddr;
}

uint8_t mc6821_read_register(struct mc6821_status *p, int address) {
    int peripheral_address = (address & 0b10 ) >> 1;
    struct mc6821_peripheral_status *ps = peripheral(peripheral_address);

    return mc6821_peripheral_read_register(ps, address & 1);
}

void mc6821_peripheral_write_register(struct mc6821_peripheral_status *ps, uint16_t address, uint8_t value) {
    if (address) {
        // the ifq flags bits can't be controlled
        value = value & 0x3f;
        ps->cr = (ps->cr & 0xc0) | value; // keep the value of the irg flags
    }
    else if (ps->ddr_access) {
        value = value & ps->ddr;
        ps->pr = ps->pr & (~ps->ddr);
        ps->pr = ps->pr | value;
    }
    else {
        ps->ddr = value;
    }
}

void mc6821_write_register(struct mc6821_status *p, uint16_t address, uint8_t value) {
    int peripheral_address = (address & 0b10 ) >> 1;
    struct mc6821_peripheral_status *ps = peripheral(peripheral_address);
    uint8_t old_output_value = ps->ddr & ps->pr;
    uint8_t old_cr_c2 = ps->c2_output? ps->c2_enable : 1;

    mc6821_peripheral_write_register(ps, address & 1, value);

    if ((ps->ddr & ps->pr) != old_output_value) {
        uint8_t output_value = ps->ddr & ps->pr;
        // find a registered cb and call it
        for (int cb_pos=0; cb_pos < MC6821_MAX_CB_COUNT && p->output_change_cb[cb_pos].cb; cb_pos++){
            if (peripheral_address == p->output_change_cb[cb_pos].peripheral_address) {
                p->output_change_cb[cb_pos].cb(p, peripheral_address, output_value, p->output_change_cb[cb_pos].data);
            }
        }
    }

    uint8_t cr_c2 = ps->c2_output? ps->c2_enable : 1;
    if (p->c2_cb[peripheral_address] && old_cr_c2 != cr_c2) {
        p->c2_cb[peripheral_address](p, peripheral_address, cr_c2, p->c2_cb_data[peripheral_address]);
    }
}

void mc6821_peripheral_input(struct mc6821_status *p, int peripheral_address, uint8_t value, uint8_t mask) {
    struct mc6821_peripheral_status *ps = peripheral(peripheral_address);
    mask = (~ps->ddr) & mask;
    value = value & mask;
    ps->pr = ps->pr & (~mask);  // keep only the bits that aren't part of the change
    ps->pr = ps->pr | value;
}

void mc6821_interrupt_1_input(struct mc6821_status *p, int peripheral_address, uint8_t value) {
    struct mc6821_peripheral_status *ps = peripheral(peripheral_address);

    if (ps->c1_transition) {
        // low to high transition
        if (ps->peripheral_c1 == 0 && value) {
            ps->irq1 = 1;
        }
    } else {
        // high to low transition
        if (ps->peripheral_c1 == 1 && !value) {
            ps->irq1 = 1;
        }
    }
    ps->peripheral_c1 = value ? 1 : 0;
}

void mc6821_interrupt_2_input(struct mc6821_status *p, int peripheral_address, uint8_t value) {
    struct mc6821_peripheral_status *ps = peripheral(peripheral_address);

    if (ps->c2_output) return;  // output pin, don't care for input

    if (ps->c2_transition) {
        // low to high transition
        if (ps->peripheral_c2 == 0 && value) {
            ps->irq2 = 1;
        }
    } else {
        // high to low transition
        if (ps->peripheral_c2 == 1 && !value) {
            ps->irq2 = 1;
        }
    }
    ps->peripheral_c2 = value ? 1 : 0;
}

// Returns 1 if any irq is enabled and its state is active
int mc6821_interrupt_state(struct mc6821_status *p) {
    for (int peripheral_address=0; peripheral_address < 2; peripheral_address++) {
        struct mc6821_peripheral_status *ps = peripheral(peripheral_address);
        if(ps->c1_enable && ps->irq1) return 1;
        if(ps->c2_enable && ps->irq2) return 1;
    }

    return 0;
}

void mc6821_register_c2_cb(struct mc6821_status *p, int peripheral_address, mc6821_cb cb, void *data) {
    peripheral_address = peripheral_address & 1;
    p->c2_cb[peripheral_address] = cb;
    p->c2_cb_data[peripheral_address] = data;
}

int mc6821_register_cb(struct mc6821_status *p, int peripheral_address, mc6821_cb cb, void *data) {
    for (int cb_pos=0; cb_pos < MC6821_MAX_CB_COUNT; cb_pos++){
        if (!p->output_change_cb[cb_pos].cb) {
            p->output_change_cb[cb_pos].cb = cb;
            p->output_change_cb[cb_pos].peripheral_address = peripheral_address;
            p->output_change_cb[cb_pos].data = data;
            return 0;
        }
    }

    return 1;
}

uint8_t _bus_pia_read(struct bus_adaptor *p, uint16_t addr) {
    struct mc6821_status *data = (struct mc6821_status *)p->data;
    return mc6821_read_register(data, addr);
}

void _bus_pia_write(struct bus_adaptor *p, uint16_t addr, uint8_t value) {
    struct mc6821_status *data = (struct mc6821_status *)p->data;
    mc6821_write_register(data, addr, value);
}

struct bus_adaptor * bus_create_pia1() {
    struct bus_adaptor *adaptor = malloc(sizeof(struct bus_adaptor));
    struct mc6821_status *data = malloc(sizeof(struct mc6821_status));
    memset(data, 0, sizeof(struct mc6821_status));

    data->name = 1;

    adaptor->start = 0xFF00;
    adaptor->end = 0xFF1F;
    adaptor->data = data;
    adaptor->load_8 = _bus_pia_read;
    adaptor->store_8 = _bus_pia_write;

    printf("Created PIA 1 start=%04X end=%04X\n", adaptor->start, adaptor->end);

    return adaptor;
}

struct bus_adaptor * bus_create_pia2() {
    struct bus_adaptor *adaptor = malloc(sizeof(struct bus_adaptor));
    struct mc6821_status *data = malloc(sizeof(struct mc6821_status));
    memset(data, 0, sizeof(struct mc6821_status));

    data->name = 2;

    adaptor->start = 0xFF20;
    adaptor->end = 0xFF33;
    adaptor->data = data;
    adaptor->load_8 = _bus_pia_read;
    adaptor->store_8 = _bus_pia_write;

    printf("Created PIA 2 start=%04X end=%04X\n", adaptor->start, adaptor->end);

    return adaptor;
}
