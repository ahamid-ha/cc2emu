#include <inttypes.h>

#ifndef __MC6821__
#define __MC6821__

#define MC6821_MAX_CB_COUNT 8

struct mc6821_peripheral_status {
    union {
        struct {
            unsigned c1_enable:1;
            unsigned c1_transition:1;
            unsigned ddr_access:1;
            unsigned c2_enable:1;
            unsigned c2_transition:1;
            unsigned c2_output:1;  // 1 = CA2 (CB2) direction is output
            unsigned irq2:1;
            unsigned irq1:1;
        }__attribute__((packed));
        uint8_t cr;
    };
    uint8_t ddr;
    uint8_t pr;

    // track status
    uint8_t peripheral_c1;
    uint8_t peripheral_c2;

    int _dump_read;
};

struct mc6821_status;
typedef void (*mc6821_cb)(struct mc6821_status *pia, int peripheral_address, uint8_t out_value, void *data);

struct mc6821_status {
    int name;
    struct mc6821_peripheral_status a;
    struct mc6821_peripheral_status b;

    struct {
        int peripheral_address;
        void *data;
        mc6821_cb cb;
    } output_change_cb[MC6821_MAX_CB_COUNT];
};

#define PIA(p) ((struct mc6821_status*)p->data)

void mc6821_peripheral_input(struct mc6821_status *p, int peripheral_address, uint8_t value, uint8_t mask);
int mc6821_register_cb(struct mc6821_status *p, int peripheral_address, mc6821_cb cb, void *data);
void mc6821_interrupt_1_input(struct mc6821_status *p, int peripheral_address, uint8_t value);
void mc6821_interrupt_2_input(struct mc6821_status *p, int peripheral_address, uint8_t value);
int mc6821_interrupt_state(struct mc6821_status *p);

#endif
