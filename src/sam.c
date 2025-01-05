#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "sam.h"


#define set_sam_bit(position, name) case position: data->name = is_set; break;

void _sam_register_write(struct sam_status *data, uint16_t addr, uint8_t value) {
    int is_set = addr & 0x1;
    int bit_pos = (addr >> 1) & 0xf;

    switch(bit_pos) {
        set_sam_bit(0, V0)
        set_sam_bit(1, V1)
        set_sam_bit(2, V2)
        set_sam_bit(3, F0)
        set_sam_bit(4, F1)
        set_sam_bit(5, F2)
        set_sam_bit(6, F3)
        set_sam_bit(7, F4)
        set_sam_bit(8, F5)
        set_sam_bit(9, F6)
        set_sam_bit(10, P1)
        set_sam_bit(11, R0)
        set_sam_bit(12, R1)
        set_sam_bit(13, M0)
        set_sam_bit(14, M1)
        set_sam_bit(15, TY)
    }

    switch(data->V) {
        case 0:
            data->_vdg_multiplier_x = 1;
            data->_vdg_multiplier_y = 12;
            break;
        case 1:
            data->_vdg_multiplier_x = 3;
            data->_vdg_multiplier_y = 1;
            break;
        case 2:
            data->_vdg_multiplier_x = 1;
            data->_vdg_multiplier_y = 3;
            break;
        case 3:
            data->_vdg_multiplier_x = 2;
            data->_vdg_multiplier_y = 1;
            break;
        case 4:
            data->_vdg_multiplier_x = 1;
            data->_vdg_multiplier_y = 2;
            break;
        case 5:
            data->_vdg_multiplier_x = 1;
            data->_vdg_multiplier_y = 1;
            break;
        case 6:
            data->_vdg_multiplier_x = 1;
            data->_vdg_multiplier_y = 1;
            break;
        case 7:
            data->_vdg_multiplier_x = 1;
            data->_vdg_multiplier_y = 1;
            break;
    }
}

void sam_reset(struct sam_status *sam) {
    sam->V = 0;
    sam->F = 0;
    sam->P = 0;
    sam->R = 0;
    sam->M = 0;
    sam->TY = 0;
    memset(sam->ram, 0, sizeof(sam->ram));
}

struct sam_status *bus_create_sam() {
    struct sam_status *sam = malloc(sizeof(struct sam_status));
    memset(sam, 0, sizeof(struct sam_status));

    return sam;
}

void sam_vdg_hs_reset(struct sam_status *sam) {
    if (sam->V == 7) return;
    sam->_vdg_address_0_3 &= 0xfff0;
    if (sam->V0 == 0) sam->_vdg_address_4 &= 0xffe0;
}

void sam_vdg_fs_reset(struct sam_status *sam) {
    sam->_vdg_address_0_3 = 0;
    sam->_vdg_address_4 = 0;
    sam->_vdg_address_5_15 = sam->F << 9;
}

uint8_t sam_get_vdg_data(struct sam_status *sam) {
    uint16_t addr = (sam->_vdg_address_0_3 & 0b1111) | (sam->_vdg_address_4 & 0b10000) | (sam->_vdg_address_5_15 & 0xffe0);
    return sam->ram[addr];
}

void sam_vdg_increment(struct sam_status *sam) {
    sam->_vdg_address_0_3++;
    if ((sam->_vdg_address_0_3 >> 4) >= sam->_vdg_multiplier_x) {
        sam->_vdg_address_4 += 1 << 4;
        sam->_vdg_address_0_3 = 0;
    }
    if ((sam->_vdg_address_4 >> 5) >= sam->_vdg_multiplier_y) {
        sam->_vdg_address_5_15 += 1 << 5;
        sam->_vdg_address_4 = 0;
    }
}

uint8_t sam_read(struct sam_status *sam, uint16_t addr) {
    if (sam->TY == 0) {
        if (addr <= 0x7fff) {
            addr = addr | (0x8000 ? sam->P : 0);
            return sam->ram[addr];
        } else if (addr <= 0x9fff) {
            addr = addr & 0x1fff;
            if (!sam->rom_load_status[0]) return 0xff;
            return sam->rom0[addr];
        } else if (addr <= 0xbfff) {
            addr = addr & 0x1fff;
            if (!sam->rom_load_status[1]) return 0xff;
            return sam->rom1[addr];
        } else if (addr <= 0xfeff) {
            addr = addr & 0x3fff;
            if (sam->rom_load_status[2]) {
                return sam->rom2[addr];
            }
            if (sam->rom_load_status[3]) {
                return sam->rom_dsk[addr];
            }
            return 0xff;
        }
    } else {
        if (addr <= 0xfeff) {
            return sam->ram[addr];
        }
    }

    if (addr <= 0xff1f) {
        addr = addr & 0x1f;
        if (!sam->pia1) return 0xff;
        return mc6821_read_register(sam->pia1, addr);
    } else if (addr <= 0xff3f) {
        addr = addr & 0x1f;
        if (!sam->pia2) return 0xff;
        return mc6821_read_register(sam->pia2, addr);
    } else if (addr <= 0xff5f) {
        addr = addr & 0x1f;
        if (!sam->pia_cartridge || !sam->pia_cartridge_read) return 0xff;
        return sam->pia_cartridge_read(sam->pia_cartridge, addr);
    } else if (addr >= 0xffe0) {
        addr = addr & 0x1fff;
        if (!sam->rom_load_status[1]) return 0xff;
        return sam->rom1[addr];
    }

    return 0xff;
}

void sam_write(struct sam_status *sam, uint16_t addr, uint8_t data) {
    if (sam->TY == 0) {
        if (addr <= 0x7fff) {
            addr = addr | (sam->P ? 0x8000 : 0);
            sam->ram[addr] = data;
            return;
        } else if (addr <= 0xfeff) {
            return;
        }
    } else {
        if (addr <= 0xfeff) {
            sam->ram[addr] = data;
            return;
        }
    }
    if (addr <= 0xff1f) {
        addr = addr & 0x1f;
        if (!sam->pia1) return;
        mc6821_write_register(sam->pia1, addr, data);
    } else if (addr <= 0xff3f) {
        addr = addr & 0x1f;
        if (!sam->pia2) return;
        mc6821_write_register(sam->pia2, addr, data);
    } else if (addr <= 0xff5f) {
        addr = addr & 0x1f;
        if (!sam->pia_cartridge || !sam->pia_cartridge_write) return;
        sam->pia_cartridge_write(sam->pia_cartridge, addr, data);
    } else if (addr <= 0xffbf) {
        return;
    } else if (addr <= 0xffdf) {
        _sam_register_write(sam, addr, data);
    }
}

int sam_load_rom(struct sam_status *sam, int rom_no, const char *path) {
    sam->rom_load_status[rom_no] = 0;
    if (!path || !*path) {
        return 0;
    }

    size_t size = 0;
    FILE *fp = fopen(path, "rb");
    uint8_t *rom_contents = sam->rom0;
    size_t max_rom_size = sizeof(sam->rom0);

    if (rom_no == 1) {
        rom_contents = sam->rom1;
        max_rom_size = sizeof(sam->rom1);
    } else if (rom_no == 2) {
        rom_contents = sam->rom2;
        max_rom_size = sizeof(sam->rom2);
    } else if (rom_no == 3) {
        rom_contents = sam->rom_dsk;
        max_rom_size = sizeof(sam->rom_dsk);
    }

    if (!fp) {
        fprintf(stderr, "error reading rom from file %s: %s\n", path, strerror(errno));
        return -1;
    }

    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    if (size > max_rom_size) {
        fprintf(stderr, "file %s is too big %ld \n", path, size);
        size = max_rom_size;
        printf("Updated size %ld\n", size);
    }
    fseek(fp, 0L, SEEK_SET);

    size_t remaining = size;
    uint16_t pos = 0;
    while (remaining > 0) {
        size_t ret = fread(rom_contents + pos, 1, remaining > 1024 ? 1024: remaining, fp);
        if (ret <=0) {
            fprintf(stderr, "fread() failed: %zu\n", ret);
            fclose(fp);
            return -2;
        }
        remaining -= ret;
        pos += ret;
    }
    fclose(fp);

    sam->rom_load_status[rom_no] = 1;

    printf("Loaded rom%d %s\n", rom_no, path);

    return 0;
}

void sam_unload_rom(struct sam_status *sam, int rom_no) {
    sam->rom_load_status[rom_no] = 0;
}
