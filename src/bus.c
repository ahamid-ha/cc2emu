#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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

struct bus_adaptor * bus_create_rom(char *path, uint16_t start) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "error reading rom from file %s: %s\n", path, strerror(errno));
        exit(1);
    }

    fseek(fp, 0L, SEEK_END);
    size_t size = ftell(fp);
    if (size + start > 0xff00) {
        fprintf(stderr, "file %s is too big %ld \n", path, size);
        size = 0xff00l - start;
        printf("Updated size %ld\n", size);
    }
    fseek(fp, 0L, SEEK_SET);

    uint8_t *rom_contents = malloc(size);

    size_t remaining = size;
    uint16_t pos = 0;
    while (remaining > 0) {
        size_t ret = fread(rom_contents + pos, 1, remaining > 1024 ? 1024: remaining, fp);
        if (ret <=0) {
            fprintf(stderr, "fread() failed: %zu\n", ret);
            exit(EXIT_FAILURE);
        }
        remaining -= ret;
        pos += ret;
    }
    fclose(fp);

    struct bus_adaptor *adaptor = malloc(sizeof(struct bus_adaptor));

    adaptor->start = start;
    adaptor->end = start + size -1;
    adaptor->data = rom_contents;
    adaptor->load_8 = _bus_memory_read;
    adaptor->store_8 = NULL;

    printf("Created rom %s start=%04X end=%04X\n", path, adaptor->start, adaptor->end);

    return adaptor;
}

struct bus_adaptor * bus_create_ram(uint16_t size, uint16_t start) {
    struct bus_adaptor *adaptor = malloc(sizeof(struct bus_adaptor));
    void *data = malloc(size);
    memset(data, 0, size);

    adaptor->start = start;
    adaptor->end = start + size -1;
    adaptor->data = data;
    adaptor->load_8 = _bus_memory_read;
    adaptor->store_8 = _bus_memory_write;

    printf("Created ram start=%04X end=%04X\n", adaptor->start, adaptor->end);

    return adaptor;
}
