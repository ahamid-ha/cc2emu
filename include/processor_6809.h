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
            unsigned C:1;
            unsigned V:1;
            unsigned Z:1;
            unsigned N:1;
            unsigned I:1;
            unsigned H:1;
            unsigned F:1;
            unsigned E:1;
        };
        uint8_t CC;
    };

    struct bus_register bus;

    uint64_t _nano_time_passed;
    int _stopped;
};

void processor_init(struct processor_state *p);
void processor_reset(struct processor_state *p);
void processor_next_opcode(struct processor_state *p);
