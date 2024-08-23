#include <inttypes.h>

struct mc6821_peripheral_status {
    union {
        struct {
            unsigned c1:2;
            unsigned ddr_access:1;
            unsigned c2:3;
            unsigned irq2:1;
            unsigned irq1:1;
        };
        uint8_t cr;
    };
    uint8_t ddr;
    uint8_t pr;
};

struct mc6821_status {
    int name;
    struct mc6821_peripheral_status a;
    struct mc6821_peripheral_status b;
};
