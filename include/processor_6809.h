#include <inttypes.h>
#include <bus.h>


#define cycle_nano 1117
// #define cycle_nano 1042

#define fs_time_nano 16700000
#define hs_time_nano 63500

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

    uint64_t _virtual_time_nano;
    int _stopped;
    int _dump_execution;
    int _irq;
    int _firq;
    int _nmi;
    int _sync;
    int _cwai;
};

void processor_init(struct processor_state *p);
void processor_reset(struct processor_state *p);
void processor_next_opcode(struct processor_state *p);
