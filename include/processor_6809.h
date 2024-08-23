#include <inttypes.h>
#include <bus.h>


#define cycle_nano 1000

// ntsc 23.976 frame / second
#define frame_time_nano 41708375

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

    struct bus_register bus;

    uint64_t _nano_time_passed;
};

void processor_init(struct processor_state *p);
void processor_reset(struct processor_state *p);
void processor_next_opcode(struct processor_state *p);
