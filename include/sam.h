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

    uint16_t _vdg_address_0_3;
    uint16_t _vdg_address_4;
    uint16_t _vdg_address_5_15;

    int _vdg_multiplier_x;
    int _vdg_multiplier_y;
};

#define SAM(p) (struct sam_status *)p->data

struct bus_adaptor * bus_create_sam();
void sam_reset(struct sam_status *sam);
void sam_vdg_hs_reset(struct sam_status *sam);
void sam_vdg_fs_reset(struct sam_status *sam);
uint16_t sam_get_vdg_address(struct sam_status *sam);
void sam_vdg_increment(struct sam_status *sam);

#endif
