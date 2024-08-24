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

struct bus_adaptor * bus_create_rom(char *path, uint16_t start);
struct bus_adaptor * bus_create_ram(uint16_t size, uint16_t start);
struct bus_adaptor * bus_create_pia1();
struct bus_adaptor * bus_create_pia2();
