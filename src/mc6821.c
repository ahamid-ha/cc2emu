#include "bus.h"
#include "mc6821.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


uint8_t mc6821_peripheral_read(struct mc6821_peripheral_status *p, int address) {
    if (address) {
        printf(" cr\n");
        return p->cr;
    }
    if (p->ddr_access) {
        printf(" pr\n");
        p->irq1 = 0;
        p->irq2 = 0;
        return p->pr;
    }
    printf(" ddr\n");
    return p->ddr;
}

uint8_t mc6821_read(struct mc6821_status *p, int address) {
    printf(" %d ", p->name);
    if ((address & 0b10) == 0) {
        printf("mc6821_read a");
        return mc6821_peripheral_read(&p->a, address & 1);
    }
    printf("mc6821_read b");
    return mc6821_peripheral_read(&p->b, address & 1);
}

void mc6821_peripheral_write(struct mc6821_peripheral_status *p, int address, uint8_t value) {
    if (address) {
        printf(" cr\n");
        p->cr = value;
    } else if (p->ddr_access) {
        printf(" pr\n");
        p->pr = value;
    }
    else {
        printf(" ddr\n");
        p->ddr = value;
    }
}

void mc6821_write(struct mc6821_status *p, int address, uint8_t value) {
    printf(" %d ", p->name);
    if ((address & 0b10 )== 0) {
        printf("mc6821_write a");
        return mc6821_peripheral_write(&p->a, address & 1, value);
    } else {
        printf("mc6821_write b");
        mc6821_peripheral_write(&p->b, address & 1, value);
    }
}


uint8_t _bus_pia1_read(struct bus_adaptor *p, uint16_t addr) {
    struct pia1_status *data = (struct pia1_status *)p->data;
    printf("pia 1 read %04X\n", addr);
    switch(addr) {
        case 0:
            {
                uint8_t value = 0x00;
                for (int row=0; row < 7; row++) {
                    for (int col=0; col < 8; col++) {
                        if (data->keyboard_keys_status[row][col]) {
                            value |= 1 << row;
                        }
                    }
                }

                printf(" pia 1 return %02X\n", ~value);
                return ~value;
            }
            break;
        default:
            printf("Unhandled pia 1 read %04X\n", addr);
    }

    return 0x00;
}

void _bus_pia1_write(struct bus_adaptor *p, uint16_t addr, uint8_t value) {
    printf(" pia 1 write %04X:%02X\n", addr, value);
    struct pia1_status *data = (struct pia1_status *)p->data;
    switch(addr) {
        case 2:
            data->pb = value;
            break;
        default:
            printf("Unhandled pia 1 write %04X:%02X\n", addr, value);
    }
}

uint8_t _bus_pia_read(struct bus_adaptor *p, uint16_t addr) {
    struct mc6821_status *data = (struct mc6821_status *)p->data;
    return mc6821_read(data, addr);
}

void _bus_pia_write(struct bus_adaptor *p, uint16_t addr, uint8_t value) {
    struct mc6821_status *data = (struct mc6821_status *)p->data;
    mc6821_write(data, addr, value);
}

struct bus_adaptor * bus_create_pia1() {
    struct bus_adaptor *adaptor = malloc(sizeof(struct bus_adaptor));
    struct mc6821_status *data = malloc(sizeof(struct mc6821_status));
    memset(data, 0, sizeof(struct mc6821_status));

    data->name = 1;

    adaptor->start = 0xFF00;
    adaptor->end = 0xFF03;
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
    adaptor->end = 0xFF23;
    adaptor->data = data;
    adaptor->load_8 = _bus_pia_read;
    adaptor->store_8 = _bus_pia_write;

    printf("Created PIA 2 start=%04X end=%04X\n", adaptor->start, adaptor->end);

    return adaptor;
}
