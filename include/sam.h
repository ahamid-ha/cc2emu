#include <inttypes.h>
#include <mc6821.h>

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

    uint8_t ram[0x10000];
    uint8_t rom0[0x2000];
    uint8_t rom1[0x2000];
    uint8_t rom2[0x3f00];
    uint8_t rom_dsk[0x3f00];
    int rom_load_status[4];

    struct mc6821_status *pia1;
    struct mc6821_status *pia2;

    void *pia_cartridge;
    uint8_t (*pia_cartridge_read)(void *p, uint16_t addr);
    void (*pia_cartridge_write)(void *p, uint16_t addr, uint8_t value);
};

struct sam_status * bus_create_sam();
void sam_reset(struct sam_status *sam);
uint8_t sam_read(struct sam_status *sam, uint16_t addr);
void sam_write(struct sam_status *sam, uint16_t addr, uint8_t data);
int sam_load_rom(struct sam_status *sam, int rom_no, const char *path);
void sam_unload_rom(struct sam_status *sam, int rom_no);
void sam_vdg_hs_reset(struct sam_status *sam);
void sam_vdg_fs_reset(struct sam_status *sam);
uint8_t sam_get_vdg_data(struct sam_status *sam);
void sam_vdg_increment(struct sam_status *sam);

#endif
