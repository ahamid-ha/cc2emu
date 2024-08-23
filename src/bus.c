#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bus.h"

uint8_t bus_read_8(struct bus_register *b, uint16_t addr) {
    if (addr >= 0xFFF2) {
        // map the vectors to the rom
        addr -= 0xFFF2 - 0xBFF2;
    }
    // printf("bus_read_8 %04X\n", addr);
    for (int i=0; i < b->count; i++) {
        struct bus_adaptor *adaptor = b->adaptors[i];
        // printf(" checking adaptor start=%04X end=%04X\n", adaptor->start, adaptor->end);
        if (addr >= adaptor->start && addr <= adaptor->end) {
            if (adaptor->load_8) {
                // printf("  Found adaptor for address %04X\n", addr);
                return adaptor->load_8(adaptor, addr - adaptor->start);
            }
        }
    }
    return 0xff;
}

void bus_write_8(struct bus_register *b, uint16_t addr, uint8_t value) {
    for (int i=0; i < b->count; i++) {
        struct bus_adaptor *adaptor = b->adaptors[i];
        if (addr >= adaptor->start && addr <= adaptor->end) {
            if (adaptor->store_8) {
                // printf(" Store %04X:%02X\n", addr, value);
                adaptor->store_8(adaptor, addr - adaptor->start, value);
            }
        }
    }

    // if (addr >= 0x400 && addr < 0x600 && value != 0 && value != 0x60) display_text(p);
}

void processor_register_bus_adaptor(struct bus_register *b, struct bus_adaptor *adaptor) {
    b->adaptors = realloc(b->adaptors, sizeof(struct bus_adaptor *) * (b->count + 1));
    b->adaptors[b->count] = adaptor;
    b->count++;
}


uint8_t _bus_memory_read(struct bus_adaptor *p, uint16_t addr) {
    uint8_t *data = (uint8_t *)p->data;
    return data[addr];
}

void _bus_memory_write(struct bus_adaptor *p, uint16_t addr, uint8_t value) {
    uint8_t *data = (uint8_t *)p->data;
    data[addr] = value;
}

struct bus_adaptor * bus_create_rom(uint8_t *data, uint16_t size, uint16_t start) {
    struct bus_adaptor *adaptor = malloc(sizeof(struct bus_adaptor));

    adaptor->start = start;
    adaptor->end = start + size -1;
    adaptor->data = data;
    adaptor->load_8 = _bus_memory_read;
    adaptor->store_8 = NULL;

    printf("Created rom start=%04X end=%04X\n", adaptor->start, adaptor->end);

    return adaptor;
}

struct bus_adaptor * bus_create_ram(uint16_t size, uint16_t start) {
    struct bus_adaptor *adaptor = malloc(sizeof(struct bus_adaptor));
    void *data = malloc(size);

    adaptor->start = start;
    adaptor->end = start + size -1;
    adaptor->data = data;
    adaptor->load_8 = _bus_memory_read;
    adaptor->store_8 = _bus_memory_write;

    printf("Created ram start=%04X end=%04X\n", adaptor->start, adaptor->end);

    return adaptor;
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

struct bus_adaptor * bus_create_pia1() {
    struct bus_adaptor *adaptor = malloc(sizeof(struct bus_adaptor));
    struct pia1_status *data = malloc(sizeof(struct pia1_status));
    memset(data, 0, sizeof(struct pia1_status));

    adaptor->start = 0xFF00;
    adaptor->end = 0xFF03;
    adaptor->data = data;
    adaptor->load_8 = _bus_pia1_read;
    adaptor->store_8 = _bus_pia1_write;

    printf("Created PIA 1 start=%04X end=%04X\n", adaptor->start, adaptor->end);

    return adaptor;
}

uint8_t tmp=0;
uint8_t _bus_pia2_read(struct bus_adaptor *p, uint16_t addr) {
    printf("pia 2 read %04X\n", addr);

    switch(addr) {
        case 1:
            return 0b00110100;
        case 2:
            return 0x10;
    }
    tmp = ~tmp;
    return tmp;
}

void _bus_pia2_write(struct bus_adaptor *p, uint16_t addr, uint8_t value) {
    printf(" pia 2 write %04X:%02X\n", addr, value);
}

struct bus_adaptor * bus_create_pia2() {
    struct bus_adaptor *adaptor = malloc(sizeof(struct bus_adaptor));
    struct pia1_status *data = NULL;

    adaptor->start = 0xFF20;
    adaptor->end = 0xFF23;
    adaptor->data = data;
    adaptor->load_8 = _bus_pia2_read;
    adaptor->store_8 = _bus_pia2_write;

    printf("Created PIA 2 start=%04X end=%04X\n", adaptor->start, adaptor->end);

    return adaptor;
}
