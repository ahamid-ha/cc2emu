#include <inttypes.h>

struct bus_adaptor {
    uint16_t start;
    uint16_t end;
    void *data;
    uint8_t (*load_8)(struct bus_adaptor *p, uint16_t addr);
    void (*store_8)(struct bus_adaptor *p, uint16_t addr, uint8_t value);
};

struct bus_register {
    struct bus_adaptor **adaptors;
    int count;
};

struct pia1_status {
    uint8_t pb;

    uint8_t keyboard_keys_status[7][8];
};

uint8_t bus_read_8(struct bus_register *b, uint16_t addr);
void bus_write_8(struct bus_register *b, uint16_t addr, uint8_t value);
void processor_register_bus_adaptor(struct bus_register *b, struct bus_adaptor *adaptor);

struct bus_adaptor * bus_create_rom(uint16_t start);
int bus_load_ram(struct bus_adaptor *ram, const char *path, int pos);
int bus_load_rom(struct bus_adaptor *rom, const char *path);
void bus_unload_rom(struct bus_adaptor *rom);
struct bus_adaptor * bus_create_ram(uint16_t size, uint16_t start);
void bus_ram_reset(struct bus_adaptor *ram);
struct bus_adaptor * bus_create_pia1();
struct bus_adaptor * bus_create_pia2();
