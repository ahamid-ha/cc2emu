#include <inttypes.h>


struct bus_adaptor {
    uint16_t start;
    uint16_t end;
    void *data;
    uint8_t (*load_8)(struct bus_adaptor *p, uint16_t addr);
    void (*store_8)(struct bus_adaptor *p, uint16_t addr, uint8_t value);
};

struct processor_state {
    union {
        struct {
            uint8_t B;
            uint8_t A;
        };
        uint16_t D;
    };

    uint16_t X;
    uint16_t Y;
    uint16_t U;
    uint16_t S;
    uint16_t PC;
    uint8_t DP;
    union {
        struct {
            int C:1;
            int V:1;
            int Z:1;
            int N:1;
            int I:1;
            int H:1;
            int F:1;
            int E:1;
        };
        uint8_t CC;
    };

    struct {
        struct bus_adaptor **adaptors;
        int count;
    }bus;
};
