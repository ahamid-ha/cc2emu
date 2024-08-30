#include <inttypes.h>

#ifndef __SAM_H__
#define __SAM_H__

struct sam_status {
    union {
        struct {
            unsigned V0:1;
            unsigned V1:1;
            unsigned V2:1;
        };
        uint8_t V;
    };

    union {
        struct {
            unsigned F0:1;
            unsigned F1:1;
            unsigned F2:1;
            unsigned F3:1;
            unsigned F4:1;
            unsigned F5:1;
            unsigned F6:1;
        };
        uint8_t F;
    };
    
    union {
        struct {
            unsigned P1:1;
        };
        uint8_t P;
    };
    
    union {
        struct {
            unsigned R0:1;
            unsigned R1:1;
        };
        uint8_t R;
    };
    
    union {
        struct {
            unsigned M0:1;
            unsigned M1:1;
        };
        uint8_t M;
    };

    unsigned TY;
};

#define SAM(p) (struct sam_status *)p->data

struct bus_adaptor * bus_create_sam();

#endif
