#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "processor_6809.h"

uint8_t processor_load_8(struct processor_state *p, uint16_t addr) {
    if (addr >= 0xFFF2) {
        // map the vectors to the rom
        addr -= 0xFFF2 - 0xBFF2;
    }
    // printf("processor_load_8 %04X\n", addr);
    for (int i=0; i < p->bus.count; i++) {
        struct bus_adaptor *adaptor = p->bus.adaptors[i];
        // printf(" checking adaptor start=%04X end=%04X\n", adaptor->start, adaptor->end);
        if (addr >= adaptor->start && addr <= adaptor->end) {
            if (adaptor->load_8) {
                // printf("  Found adaptor for address %04X\n", addr);
                return adaptor->load_8(adaptor, addr - adaptor->start);
            }
        }
    }
    return 0;
}

char display_chars[] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]  "
    " !\"#$%&'()*+,-./0123456789:;<=>?";
void display_text(struct processor_state *p) {
    int in_exit=0;
    for (int line=0; line < 16; line++) {
        for (int col=0; col < 32; col++) {
            uint8_t data = processor_load_8(p, 1024 + (line * 32) + col);
            if (1024 + (line * 32) + col == 1024 && data == 0xc6) in_exit += 1;
            if (1024 + (line * 32) + col == 1024 +1 && data == 0x4f) in_exit += 1;
            if (1024 + (line * 32) + col == 1024 +2 && data == 0xc1) in_exit += 1;
            if (1024 + (line * 32) + col == 1024 + 32 && data == 0xc6 && in_exit == 3) exit(0);

            uint8_t display_char = display_chars[data & 63];
            if (display_char >= 128) display_char = 32;
            printf("%02x%c ", data, display_char);
            // putc(data, stdout);
        }
        putc('\n', stdout);
    }
    putc('\n', stdout);
}

void processor_store_8(struct processor_state *p, uint16_t addr, uint8_t value) {
    for (int i=0; i < p->bus.count; i++) {
        struct bus_adaptor *adaptor = p->bus.adaptors[i];
        if (addr >= adaptor->start && addr <= adaptor->end) {
            if (adaptor->store_8) {
                // printf(" Store %04X:%02X\n", addr, value);
                adaptor->store_8(adaptor, addr - adaptor->start, value);
            }
        }
    }

    // if (addr >= 0x400 && addr < 0x600 && value != 0 && value != 0x60) display_text(p);
}

void processor_register_bus_adaptor(struct processor_state *p, struct bus_adaptor *adaptor) {
    p->bus.adaptors = realloc(p->bus.adaptors, sizeof(struct bus_adaptor *) * (p->bus.count + 1));
    p->bus.adaptors[p->bus.count] = adaptor;
    p->bus.count++;
}

uint16_t processor_load_16(struct processor_state *p, uint16_t addr) {
    uint16_t msb = (uint16_t)processor_load_8(p, addr);
    uint16_t lsb = (uint16_t)processor_load_8(p, addr + 1);

    return (msb << 8) | lsb;
}

void processor_store_16(struct processor_state *p, uint16_t addr, uint16_t data) {
    // printf(" Store16 %04X:%04X\n", addr, data);
    processor_store_8(p, addr, (data >> 8) & 0xFF);
    processor_store_8(p, addr + 1, data & 0xFF);
}

void processor_reset(struct processor_state *p) {
    p->DP = 0;
    p->F = 1;
    p->I = 1;
    p->PC = processor_load_16(p, 0xfffe);
}

void processor_init(struct processor_state *p) {
    memset(p, 0, sizeof(struct processor_state));
    processor_reset(p);
}

#define _bit(x) (x?'1':'0')
void processor_dump(struct processor_state *p) {
    printf("    Processor dump:\n");
    printf("      A:%02X, B:%02X\n", p->A, p->B);
    printf("      X:%04X, Y:%04X, U:%04X, S:%04X, PC:%04X, DP:%02X\n", p->X, p->Y, p->U, p->S, p->PC, p->DP);
    printf("      C:%c, V:%c, Z:%c, N:%c, I:%c, H:%c, F:%c, E:%c\n\n", _bit(p->C), _bit(p->V), _bit(p->Z), _bit(p->N), _bit(p->I), _bit(p->H), _bit(p->F), _bit(p->E));
}

uint16_t __get_address_direct(struct processor_state *p) {
    uint16_t addr_high = p->DP;
    uint16_t addr_low = processor_load_8(p, p->PC++);

    return addr_high << 8 | addr_low;
}

uint16_t __get_address_extended(struct processor_state *p) {
    uint16_t index = processor_load_16(p, p->PC++);
    p->PC++;
    return index;
}

uint16_t __get_address_immediate8(struct processor_state *p) {
    uint16_t index = p->PC;
    p->PC++;
    return index;
}

uint16_t __get_address_immediate16(struct processor_state *p) {
    uint16_t index = p->PC;
    p->PC += 2;
    return index;
}

uint16_t __get_address_relative8(struct processor_state *p) {
    uint8_t offset = processor_load_8(p, p->PC++);
    uint16_t index = p->PC;
    index = index + ((int8_t)offset);

    return index;
}

uint16_t __get_address_relative16(struct processor_state *p) {
    uint16_t offset = processor_load_16(p, p->PC++);
    p->PC++;
    uint16_t index = p->PC;
    index = index + ((int16_t)offset);
    return index;
}

uint16_t __get_address_indexed(struct processor_state *p) {
    uint8_t post_byte = processor_load_8(p, p->PC++);
    uint16_t index = 0;
    switch ((post_byte & 0b01100000) >> 5) {
        case 0b00:
            index = p->X;
            break;
        case 0b01:
            index = p->Y;
            break;
        case 0b10:
            index = p->U;
            break;
        case 0b11:
            index = p->S;
            break;
    }
    uint8_t offset_code = post_byte & 0b11111;
    if ((post_byte & 0b10000000) == 0) {
        if (offset_code & 0b10000) {
            offset_code = offset_code | 0b11100000;
        }
        return index + ((int8_t)offset_code);
    }

    uint8_t indirect = offset_code & 0b10000;

    switch(offset_code & 0b1111) {
        case 0b0100:
            // No offset
            index = index;
            break;
        case 0b1000:
            // 8-bit offset
            {
                uint8_t offset = processor_load_8(p, p->PC++);
                index = index + ((int8_t)offset);
            }
            break;
        case 0b1001:
            // 16-bit offset
            {
                uint16_t offset = processor_load_16(p, p->PC++);
                p->PC++;
                index = index + ((int16_t)offset);
            }
            break;
        case 0b0110:
            // A register offset
            index = index + ((int8_t)p->A);
            break;
        case 0b0101:
            // B register offset
            index = index + ((int8_t)p->B);
            break;
        case 0b1011:
            // D register offset
            index = index + ((int16_t)p->D);
            break;
        case 0b0000:
            // Increment by 1
            switch ((post_byte & 0b01100000) >> 5) {
                case 0b00:
                    p->X += 1;
                    break;
                case 0b01:
                    p->Y += 1;
                    break;
                case 0b10:
                    p->U += 1;
                    break;
                case 0b11:
                    p->S += 1;
                    break;
            }
            index = index;
            break;
        case 0b0001:
            // Increment by 2
            switch ((post_byte & 0b01100000) >> 5) {
                case 0b00:
                    p->X += 2;
                    break;
                case 0b01:
                    p->Y += 2;
                    break;
                case 0b10:
                    p->U += 2;
                    break;
                case 0b11:
                    p->S += 2;
                    break;
            }
            index = index;
            break;
        case 0b0010:
            // Decrement by 1
            switch ((post_byte & 0b01100000) >> 5) {
                case 0b00:
                    p->X -= 1;
                    break;
                case 0b01:
                    p->Y -= 1;
                    break;
                case 0b10:
                    p->U -= 1;
                    break;
                case 0b11:
                    p->S -= 1;
                    break;
            }
            index = index - 1;
            break;
        case 0b0011:
            // Decrement by 2
            switch ((post_byte & 0b01100000) >> 5) {
                case 0b00:
                    p->X -= 2;
                    break;
                case 0b01:
                    p->Y -= 2;
                    break;
                case 0b10:
                    p->U -= 2;
                    break;
                case 0b11:
                    p->S -= 2;
                    break;
            }
            index = index - 2;
            break;
        case 0b1100:
            // constant offset from PC - 8 bit offset
            {
                uint8_t offset = processor_load_8(p, p->PC++);
                index = p->PC;
                index = index + ((int8_t)offset);
            }
            break;
        case 0b1101:
            // constant offset from PC - 16 bit offset
            {
                uint16_t offset = processor_load_16(p, p->PC++);
                p->PC++;
                index = p->PC;
                index = index + ((int16_t)offset);
            }
            break;
        case 0b1111:
            index = processor_load_16(p, p->PC++);
            break;
    }
    if (indirect) {
        index = processor_load_16(p, index);
    }
    return index;
}

void __update_CC_data8(struct processor_state *p, uint8_t data) {
    p->N = data & 0x80 ? 1 : 0;
    p->Z = data == 0 ? 1 : 0;
}

void __update_CC_data16(struct processor_state *p, uint16_t data) {
    p->N = data & 0x8000 ? 1 : 0;
    p->Z = data == 0 ? 1 : 0;
}

void __opcode_adca(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    uint8_t result;
    p->V = __builtin_add_overflow(data, p->C, &result);
    p->C = (int8_t)result < (int8_t)data;
    p->V |= __builtin_add_overflow(result, p->A, &p->A);
    p->C |= (int8_t)p->A < (int8_t)result;
    __update_CC_data8(p, p->A);
}

void __opcode_adcb(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    uint8_t result;
    p->V = __builtin_add_overflow(data, p->C, &result);
    p->C = (int8_t)result < (int8_t)data;
    p->V |= __builtin_add_overflow(result, p->B, &p->B);
    p->C |= (int8_t)p->B < (int8_t)result;
    __update_CC_data8(p, p->B);
}

void __opcode_add8(struct processor_state *p, uint16_t address, uint8_t *reg) {
    uint8_t data = processor_load_8(p, address);
    p->V = __builtin_add_overflow(data, *reg, reg);
    p->C = (int8_t)*reg < (int8_t)data;
    __update_CC_data8(p, *reg);

}

void __opcode_add16(struct processor_state *p, uint16_t address, uint16_t *reg) {
    uint16_t data = processor_load_16(p, address);
    p->V = __builtin_add_overflow(data, *reg, reg);
    p->C = (int16_t)*reg < (int16_t)data;
    __update_CC_data16(p, *reg);
}

void __opcode_neg(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    data = 0 - data;
    processor_store_8(p, address, data);
    __update_CC_data8(p, data);
    p->V = data == 0x80 ? 1 : 0;
    p->C = data == 0x0 ? 0 : 1;
}

void __opcode_clr(struct processor_state *p, uint16_t address) {
    processor_store_8(p, address, 0);
    p->N = 0;
    p->Z = 1;
    p->V = 0;
    p->C = 0;
}

void __opcode_clr_reg(struct processor_state *p, uint8_t *reg) {
    *reg = 0;
    p->N = 0;
    p->Z = 1;
    p->V = 0;
    p->C = 0;
}

void __opcode_inc(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    p->V = data == 0x7F ? 1 : 0;
    data++;
    processor_store_8(p, address, 0);
    __update_CC_data8(p, data);
}

void __opcode_inc_reg(struct processor_state *p, uint8_t *reg) {
    p->V = *reg == 0x7F ? 1 : 0;
    (*reg)++;
    __update_CC_data8(p, *reg);
}

void __opcode_com(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    data = ~data;
    processor_store_8(p, address, data);
    __update_CC_data8(p, data);
    p->V = 0;
    p->C = 1;
}

void __opcode_com_reg(struct processor_state *p, uint8_t *reg) {
    *reg = ~(*reg);
    __update_CC_data8(p, *reg);
    p->V = 0;
    p->C = 1;
}

void __opcode_lda(struct processor_state *p, uint16_t address) {
    p->A = processor_load_8(p, address);
    __update_CC_data8(p, p->A);
    p->V = 0;
}

void __opcode_ldb(struct processor_state *p, uint16_t address) {
    p->B = processor_load_8(p, address);
    __update_CC_data8(p, p->B);
    p->V = 0;
}

void __opcode_ldd(struct processor_state *p, uint16_t address) {
    p->D = processor_load_16(p, address);
    __update_CC_data16(p, p->D);
    p->V = 0;
}

void __opcode_lds(struct processor_state *p, uint16_t address) {
    p->S = processor_load_16(p, address);
    __update_CC_data16(p, p->S);
    p->V = 0;
}

void __opcode_ldu(struct processor_state *p, uint16_t address) {
    p->U = processor_load_16(p, address);
    __update_CC_data16(p, p->U);
    p->V = 0;
}

void __opcode_ldx(struct processor_state *p, uint16_t address) {
    p->X = processor_load_16(p, address);
    __update_CC_data16(p, p->X);
    p->V = 0;
}

void __opcode_ldy(struct processor_state *p, uint16_t address) {
    p->Y = processor_load_16(p, address);
    __update_CC_data16(p, p->Y);
    p->V = 0;
}

void __opcode_sta(struct processor_state *p, uint16_t address) {
    processor_store_8(p, address, p->A);
    __update_CC_data8(p, p->A);
    p->V = 0;
}

void __opcode_stb(struct processor_state *p, uint16_t address) {
    processor_store_8(p, address, p->B);
    __update_CC_data8(p, p->B);
    p->V = 0;
}

void __opcode_std(struct processor_state *p, uint16_t address) {
    processor_store_16(p, address, p->D);
    __update_CC_data16(p, p->D);
    p->V = 0;
}

void __opcode_sts(struct processor_state *p, uint16_t address) {
    processor_store_16(p, address, p->S);
    __update_CC_data16(p, p->S);
    p->V = 0;
}

void __opcode_stu(struct processor_state *p, uint16_t address) {
    processor_store_16(p, address, p->U);
    __update_CC_data16(p, p->U);
    p->V = 0;
}

void __opcode_stx(struct processor_state *p, uint16_t address) {
    processor_store_16(p, address, p->X);
    __update_CC_data16(p, p->X);
    p->V = 0;
}

void __opcode_sty(struct processor_state *p, uint16_t address) {
    processor_store_16(p, address, p->Y);
    __update_CC_data16(p, p->Y);
    p->V = 0;
}

void __opcode_dec(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    p->V = 1 ? data == 0x80 : 0;
    data--;
    processor_store_8(p, address, data);
    __update_CC_data8(p, data);
}

void __opcode_leas(struct processor_state *p, uint16_t address) {
    p->S = address;
}

void __opcode_leau(struct processor_state *p, uint16_t address) {
    p->U = address;
}

void __opcode_leax(struct processor_state *p, uint16_t address) {
    p->X = address;
    p->Z = address == 0 ? 0 : 1;
}

void __opcode_leay(struct processor_state *p, uint16_t address) {
    p->Y = address;
    p->Z = address == 0 ? 0 : 1;
}

void __opcode_jmp(struct processor_state *p, uint16_t address) {
    p->PC = address;
}

void __opcode_jsr(struct processor_state *p, uint16_t address) {
    p->S -= 2;
    processor_store_16(p, p->S, p->PC);
    p->PC = address;
}

void __opcode_rts(struct processor_state *p) {
    p->PC = processor_load_16(p, p->S);
    p->S += 2;
}

void __opcode_or(struct processor_state *p, uint16_t address, uint8_t *reg) {
    uint8_t data = processor_load_8(p, address);
    *reg = (*reg) | data;
    p->V = 0;
    __update_CC_data8(p, *reg);
}

void __opcode_orcc(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    p->CC = p->CC | data;
}

void __opcode_lsr8(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    p->C = data & 1;
    data = data >> 1;
    processor_store_8(p, address, data);
    __update_CC_data8(p, data);
}

void __opcode_bit8(struct processor_state *p, uint16_t address, uint8_t *reg) {
    uint8_t data = processor_load_8(p, address);
    uint8_t result = *reg & data;
    p->V = 0;
    __update_CC_data8(p, result);
}

void __opcode_sub8(struct processor_state *p, uint16_t address, uint8_t *reg, int compare_only) {
    uint8_t data = processor_load_8(p, address);
    uint8_t result;
    p->V = __builtin_sub_overflow(*reg, data, &result);
    p->C = data > *reg;
    __update_CC_data8(p, result);

    if (!compare_only) *reg = result;
}

void __opcode_sub16(struct processor_state *p, uint16_t address, uint16_t *reg, int compare_only) {
    uint16_t data = processor_load_16(p, address);
    uint16_t result;
    p->V = __builtin_sub_overflow(*reg, data, &result);
    p->C = data > *reg;
    __update_CC_data8(p, result);

    if (!compare_only) *reg = result;
}

uint16_t __opcode_exg_get_value(struct processor_state *p, uint8_t code, int r_pos) {
    switch(code & 0xf) {
        case 0b0000:
            return p->D;
        case 0b0001:
            return p->X;
        case 0b0010:
            return p->Y;
        case 0b0011:
            return p->U;
        case 0b0100:
            return p->S;
        case 0b0101:
            return p->PC;
        case 0b1000:
            return p->A | 0xff00;
        case 0b1001:
            return p->B | 0xff00;
        case 0b1010:
            if (!r_pos)
                return p->CC | (p->CC << 8);
            return p->CC | 0xff00;
        case 0b1011:
            if (!r_pos)
                return p->DP | (p->DP << 8);
            return p->DP | 0xff00;
    }
    return 0;
}
void __opcode_exg_set_value(struct processor_state *p, uint8_t code, uint16_t value) {
    switch(code & 0xf) {
        case 0b0000:
            p->D = value;
            break;
        case 0b0001:
            p->X = value;
            break;
        case 0b0010:
            p->Y = value;
            break;
        case 0b0011:
            p->U = value;
            break;
        case 0b0100:
            p->S = value;
            break;
        case 0b0101:
            p->PC = value;
            break;
        case 0b1000:
            p->A = value;
            break;
        case 0b1001:
            p->B = value;
            break;
        case 0b1010:
            p->CC = value;
            break;
        case 0b1011:
            p->DP = value;
    }
}
void __opcode_exg(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    if (data == 0b10000000) data = 0b10001001;  // covert EXG A,D to EXG A,B

    uint16_t r0 = __opcode_exg_get_value(p, data >> 4, 0);
    uint16_t r1 = __opcode_exg_get_value(p, data, 1);

    __opcode_exg_set_value(p, data, r0);
    __opcode_exg_set_value(p, data >> 4, r1);
}

void __opcode_tfr(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    if (data == 0b10000000) data = 0b10001001u;  // covert EXG A,D to EXG A,B

    uint16_t r0 = __opcode_exg_get_value(p, data >> 4, 0);
    __opcode_exg_set_value(p, data, r0);
}

void __opcode_daa(struct processor_state *p) {
    uint8_t h = p->A >> 4;
    uint8_t l = p->A & 0xf;

    h = (h + 6) & 0xf ? p->C != 0 || h > 9 || ( h > 8 && l > 9) : h;
    l = (l + 6) & 0xf ? p->H != 0 || l > 9 : l;

    p->C = 1 ? p->C != 0 || h > 9 || ( h > 8 && l > 9) : 0;

    p->A = (h << 4) | l;
    __update_CC_data8(p, p->A);
}

void __opcode_and(struct processor_state *p, uint16_t address, uint8_t *reg) {
    uint8_t data = processor_load_8(p, address);
    *reg = *reg & data;
    __update_CC_data8(p, *reg);
    p->V = 0;
}

void __opcode_eor(struct processor_state *p, uint16_t address, uint8_t *reg) {
    uint8_t data = processor_load_8(p, address);
    *reg = *reg ^ data;
    __update_CC_data8(p, *reg);
    p->V = 0;
}

void __opcode_rol(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    int new_c = data & 0x80 ? 1 : 0;
    int bit6 = data & 0x40 ? 1 : 0;
    int bit7 = data & 0x80 ? 1 : 0;
    p->V = bit6 ^ bit7;
    data = (data << 1) | p->C;
    processor_store_8(p, address, data);
    __update_CC_data8(p, data);
    p->C = new_c;
}

void __opcode_rol_reg(struct processor_state *p, uint8_t *reg) {
    uint8_t data = *reg;
    int new_c = data & 0x80 ? 1 : 0;
    int bit6 = data & 0x40 ? 1 : 0;
    int bit7 = data & 0x80 ? 1 : 0;
    p->V = bit6 ^ bit7;
    *reg = (data << 1) | p->C;
    __update_CC_data8(p, *reg);
    p->C = new_c;
}

void __opcode_asl(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    int new_c = data & 0x80 ? 1 : 0;
    int bit6 = data & 0x40 ? 1 : 0;
    int bit7 = data & 0x80 ? 1 : 0;
    p->V = bit6 ^ bit7;
    data = data << 1;
    processor_store_8(p, address, data);
    __update_CC_data8(p, data);
    p->C = new_c;
}

void __opcode_asl_reg(struct processor_state *p, uint8_t *reg) {
    uint8_t data = *reg;
    int new_c = data & 0x80 ? 1 : 0;
    int bit6 = data & 0x40 ? 1 : 0;
    int bit7 = data & 0x80 ? 1 : 0;
    p->V = bit6 ^ bit7;
    *reg = data << 1;
    __update_CC_data8(p, *reg);
    p->C = new_c;
}

void __opcode_asr(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    int bit7 = data & 0x80;
    p->C = data & 1;
    data = (data >> 1) | bit7;
    processor_store_8(p, address, data);
    __update_CC_data8(p, data);
}

void __opcode_asr_reg(struct processor_state *p, uint8_t *reg) {
    uint8_t data = *reg;
    int bit7 = data & 0x80;
    p->C = data & 1;
    *reg = (data >> 1) | bit7;
    __update_CC_data8(p, *reg);
}

void __opcode_ror(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    int bit7 = 0x80 ? p->C : 0;
    p->C = data & 1;
    data = (data >> 1) | bit7;
    processor_store_8(p, address, data);
    __update_CC_data8(p, data);
}

void __opcode_ror_reg(struct processor_state *p, uint8_t *reg) {
    uint8_t data = *reg;
    int bit7 = 0x80 ? p->C : 0;
    p->C = data & 1;
    *reg = (data >> 1) | bit7;
    __update_CC_data8(p, *reg);
}

void __opcode_andcc(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    p->CC = p->CC & data;
}

void __opcode_tst(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    __update_CC_data8(p, data);
    p->V = 0;
}

void __opcode_pshs(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    if (data & 0b10000000) {
        p->S -= 2;
        processor_store_16(p, p->S, p->PC);
    }
    if (data & 0b1000000) {
        p->S -= 2;
        processor_store_16(p, p->S, p->U);
    }
    if (data & 0b100000) {
        p->S -= 2;
        processor_store_16(p, p->S, p->Y);
    }
    if (data & 0b10000) {
        p->S -= 2;
        processor_store_16(p, p->S, p->X);
    }
    if (data & 0b1000) {
        p->S--;
        processor_store_8(p, p->S, p->DP);
    }
    if (data & 0b100) {
        p->S--;
        processor_store_8(p, p->S, p->B);
    }
    if (data & 0b10) {
        p->S--;
        processor_store_8(p, p->S, p->A);
    }
    if (data & 0b1) {
        p->S--;
        processor_store_8(p, p->S, p->CC);
    }
}

void __opcode_pshu(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    if (data & 0b10000000) {
        p->U -= 2;
        processor_store_16(p, p->U, p->PC);
    }
    if (data & 0b1000000) {
        p->U -= 2;
        processor_store_16(p, p->U, p->S);
    }
    if (data & 0b100000) {
        p->U -= 2;
        processor_store_16(p, p->U, p->Y);
    }
    if (data & 0b10000) {
        p->U -= 2;
        processor_store_16(p, p->U, p->X);
    }
    if (data & 0b1000) {
        p->U--;
        processor_store_8(p, p->U, p->DP);
    }
    if (data & 0b100) {
        p->U--;
        processor_store_8(p, p->U, p->B);
    }
    if (data & 0b10) {
        p->U--;
        processor_store_8(p, p->U, p->A);
    }
    if (data & 0b1) {
        p->U--;
        processor_store_8(p, p->U, p->CC);
    }
}

void __opcode_puls(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    if (data & 0b1) {
        p->CC = processor_load_8(p, p->S);
        p->S++;
    }
    if (data & 0b10) {
        p->A = processor_load_8(p, p->S);
        p->S++;
    }
    if (data & 0b100) {
        p->B = processor_load_8(p, p->S);
        p->S++;
    }
    if (data & 0b1000) {
        p->DP = processor_load_8(p, p->S);
        p->S++;
    }
    if (data & 0b10000) {
        p->X = processor_load_16(p, p->S);
        p->S += 2;
    }
    if (data & 0b100000) {
        p->Y = processor_load_16(p, p->S);
        p->S += 2;
    }
    if (data & 0b1000000) {
        p->U = processor_load_16(p, p->S);
        p->S += 2;
    }
    if (data & 0b10000000) {
        p->PC = processor_load_16(p, p->S);
        p->S -+ 2;
    }
}

void __opcode_pulu(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    if (data & 0b1) {
        p->CC = processor_load_8(p, p->U);
        p->U++;
    }
    if (data & 0b10) {
        p->A = processor_load_8(p, p->U);
        p->U++;
    }
    if (data & 0b100) {
        p->B = processor_load_8(p, p->U);
        p->U++;
    }
    if (data & 0b1000) {
        p->DP = processor_load_8(p, p->U);
        p->U++;
    }
    if (data & 0b10000) {
        p->X = processor_load_16(p, p->U);
        p->U += 2;
    }
    if (data & 0b100000) {
        p->Y = processor_load_16(p, p->U);
        p->U += 2;
    }
    if (data & 0b1000000) {
        p->S = processor_load_16(p, p->U);
        p->U += 2;
    }
    if (data & 0b10000000) {
        p->PC = processor_load_16(p, p->U);
        p->U += 2;
    }
}

void execute_opcode(struct processor_state *p, uint16_t opcode) {
    switch(opcode) {
        case 0x00:  // NEG direct
            __opcode_neg(p, __get_address_direct(p));
            break;
        case 0x03:
            __opcode_com(p, __get_address_direct(p));
            break;
        case 0x04:
            __opcode_lsr8(p, __get_address_direct(p));
            break;
        case 0x06:
            __opcode_ror(p, __get_address_direct(p));
            break;
        case 0x07:
            __opcode_asr(p, __get_address_direct(p));
            break;
        case 0x08:
            __opcode_asl(p, __get_address_direct(p));
            break;
        case 0x09:
            __opcode_rol(p, __get_address_direct(p));
            break;
        case 0x0A:
            __opcode_dec(p, __get_address_direct(p));
            break;
        case 0x0C:
            __opcode_inc(p, __get_address_direct(p));
            break;
        case 0x0D:
            __opcode_tst(p, __get_address_direct(p));
            break;
        case 0x0E:
            __opcode_jmp(p, __get_address_direct(p));
            break;
        case 0x0F:
            __opcode_clr(p, __get_address_direct(p));
            break;
        case 0x12:
            // NOP
            break;
        case 0x16:
            __opcode_jmp(p, __get_address_relative16(p));  // LBRA
            break;
        case 0x17:
            __opcode_jsr(p, __get_address_relative16(p));  // LBSR
            break;
        case 0x19:
            __opcode_daa(p);
            break;
        case 0x1A:
            __opcode_orcc(p, __get_address_immediate8(p));
            break;
        case 0x1C:
            __opcode_andcc(p, __get_address_immediate8(p));
            break;
        case 0x1E:
            __opcode_exg(p, __get_address_immediate8(p));
            break;
        case 0x1F:
            __opcode_tfr(p, __get_address_immediate8(p));
            break;
        case 0x20: // BRA
            {
                uint16_t jmp_address = __get_address_relative8(p);
                __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x21: // BRN
            {
                __get_address_relative8(p);
                // NOPE
            }
            break;
        case 0x22: // BHI
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if (p->Z == 0 && p->C == 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x23: // BLS
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if (p->Z != 0 || p->C != 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x24: // BHS, BCC
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if (p->C == 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x25: // BLO, BCS
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if (p->C != 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x26: // BNE
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if (p->Z == 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x27: // BEQ
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if (p->Z != 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x28: // BVC
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if (p->V == 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x29: // BVS
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if (p->V != 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x2A: // BPL
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if (p->N == 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x2B: // BMI
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if (p->N != 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x2C: // BGE
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if (p->N == p->V)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x2D: // BLT
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if (p->N != p->V)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x2E: // BGT
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if (p->N == p->V && p->Z == 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x2F: // BLE
            {
                uint16_t jmp_address = __get_address_relative8(p);
                if ((p->N != p->V) || p->Z != 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x30:
            __opcode_leax(p, __get_address_indexed(p));
            break;
        case 0x31:
            __opcode_leay(p, __get_address_indexed(p));
            break;
        case 0x32:
            __opcode_leas(p, __get_address_indexed(p));
            break;
        case 0x33:
            __opcode_leau(p, __get_address_indexed(p));
            break;
        case 0x34:
            __opcode_pshs(p, __get_address_immediate8(p));
            break;
        case 0x35:
            __opcode_puls(p, __get_address_immediate8(p));
            break;
        case 0x36:
            __opcode_pshu(p, __get_address_immediate8(p));
            break;
        case 0x37:
            __opcode_pulu(p, __get_address_immediate8(p));
            break;
        case 0x39:
            __opcode_rts(p);
            break;
        case 0x3A:  // ABX
            p->X += p->B;
            break;
        case 0x43:
            __opcode_com_reg(p, &p->A);
            break;
        case 0x44:  // LSRA
            p->C = p->A & 1;
            p->A = p->A >> 1;
            __update_CC_data8(p, p->A);
            break;
        case 0x46:
            __opcode_ror_reg(p, &p->A);
            break;
        case 0x47:
            __opcode_asr_reg(p, &p->A);
            break;
        case 0x48:
            __opcode_asl_reg(p, &p->A);
            break;
        case 0x49:
            __opcode_rol_reg(p, &p->A);
            break;
        case 0x4A:  // DECA
            p->V = 1 ? p->A == 0x80 : 0;
            p->A--;
            __update_CC_data8(p, p->A);
            break;
        case 0x4C:
            __opcode_inc_reg(p, &p->A);
            break;
        case 0x4D:
            __update_CC_data8(p, p->A);
            p->V = 0;
            break;
        case 0x4F:
            __opcode_clr_reg(p, &p->A);
            break;
        case 0x53:
            __opcode_com_reg(p, &p->B);
            break;
        case 0x54:  // LSRB
            p->C = p->B & 1;
            p->B = p->B >> 1;
            __update_CC_data8(p, p->B);
            break;
        case 0x56:
            __opcode_ror_reg(p, &p->B);
            break;
        case 0x57:
            __opcode_asr_reg(p, &p->B);
            break;
        case 0x58:
            __opcode_asl_reg(p, &p->B);
            break;
        case 0x59:
            __opcode_rol_reg(p, &p->B);
            break;
        case 0x5A:  // DECA
            p->V = 1 ? p->B == 0x80 : 0;
            p->B--;
            __update_CC_data8(p, p->B);
            break;
        case 0x5C:
            __opcode_inc_reg(p, &p->B);
            break;
        case 0x5D:
            __update_CC_data8(p, p->B);
            p->V = 0;
            break;
        case 0x5F:
            __opcode_clr_reg(p, &p->B);
            break;
        case 0x60:  // NEG indexed
            __opcode_neg(p, __get_address_indexed(p));
            break;
        case 0x63:
            __opcode_com(p, __get_address_indexed(p));
            break;
        case 0x64:
            __opcode_lsr8(p, __get_address_indexed(p));
            break;
        case 0x66:
            __opcode_ror(p, __get_address_indexed(p));
            break;
        case 0x67:
            __opcode_asr(p, __get_address_indexed(p));
            break;
        case 0x68:
            __opcode_asl(p, __get_address_indexed(p));
            break;
        case 0x69:
            __opcode_rol(p, __get_address_indexed(p));
            break;
        case 0x6A:
            __opcode_dec(p, __get_address_indexed(p));
            break;
        case 0x6C:
            __opcode_inc(p, __get_address_indexed(p));
            break;
        case 0x6D:
            __opcode_tst(p, __get_address_indexed(p));
            break;
        case 0x6E:
            __opcode_jmp(p, __get_address_indexed(p));
            break;
        case 0x6F:
            __opcode_clr(p, __get_address_indexed(p));
            break;
        case 0x70:  // NEG extended
            __opcode_neg(p, __get_address_extended(p));
            break;
        case 0x73:
            __opcode_com(p, __get_address_extended(p));
            break;
        case 0x74:
            __opcode_lsr8(p, __get_address_extended(p));
            break;
        case 0x76:
            __opcode_ror(p, __get_address_extended(p));
            break;
        case 0x77:
            __opcode_asr(p, __get_address_extended(p));
            break;
        case 0x78:
            __opcode_asl(p, __get_address_extended(p));
            break;
        case 0x79:
            __opcode_rol(p, __get_address_extended(p));
            break;
        case 0x7A:
            __opcode_dec(p, __get_address_extended(p));
            break;
        case 0x7C:
            __opcode_inc(p, __get_address_extended(p));
            break;
        case 0x7D:
            __opcode_tst(p, __get_address_extended(p));
            break;
        case 0x7E:
            __opcode_jmp(p, __get_address_extended(p));
            break;
        case 0x7F:
            __opcode_clr(p, __get_address_extended(p));
            break;
        case 0x80:
            __opcode_sub8(p, __get_address_immediate8(p), &p->A, 0);
            break;
        case 0x81:
            __opcode_sub8(p, __get_address_immediate8(p), &p->A, 1);
            break;
        case 0x83:
            __opcode_sub16(p, __get_address_immediate16(p), &p->D, 0);
            break;
        case 0x84:
            __opcode_and(p, __get_address_immediate8(p), &p->A);
            break;
        case 0x85:
            __opcode_bit8(p, __get_address_immediate8(p), &p->A);
            break;
        case 0x86:
            __opcode_lda(p, __get_address_immediate8(p));
            break;
        case 0x88:
            __opcode_eor(p, __get_address_immediate8(p), &p->A);
            break;
        case 0x89:
            __opcode_adca(p, __get_address_immediate8(p));
            break;
        case 0x8A:
            __opcode_or(p, __get_address_immediate8(p), &p->A);
            break;
        case 0x8B:
            __opcode_add8(p, __get_address_immediate8(p), &p->A);
            break;
        case 0x8C:
            __opcode_sub16(p, __get_address_immediate16(p), &p->X, 1);
            break;
        case 0x8D:
            __opcode_jsr(p, __get_address_relative8(p));  // BSR
            break;
        case 0x8E:
            __opcode_ldx(p, __get_address_immediate16(p));
            break;
        case 0x90:
            __opcode_sub8(p, __get_address_direct(p), &p->A, 0);
            break;
        case 0x91:
            __opcode_sub8(p, __get_address_direct(p), &p->A, 1);
            break;
        case 0x93:
            __opcode_sub16(p, __get_address_direct(p), &p->D, 0);
            break;
        case 0x94:
            __opcode_and(p, __get_address_direct(p), &p->A);
            break;
        case 0x95:
            __opcode_bit8(p, __get_address_direct(p), &p->A);
            break;
        case 0x96:
            __opcode_lda(p, __get_address_direct(p));
            break;
        case 0x97:
            __opcode_sta(p, __get_address_direct(p));
            break;
        case 0x98:
            __opcode_eor(p, __get_address_direct(p), &p->A);
            break;
        case 0x99:
            __opcode_adca(p, __get_address_direct(p));
            break;
        case 0x9A:
            __opcode_or(p, __get_address_direct(p), &p->A);
            break;
        case 0x9B:
            __opcode_add8(p, __get_address_direct(p), &p->A);
            break;
        case 0x9C:
            __opcode_sub16(p, __get_address_direct(p), &p->X, 1);
            break;
        case 0x9D:
            __opcode_jsr(p, __get_address_direct(p));
            break;
        case 0x9E:
            __opcode_ldx(p, __get_address_direct(p));
            break;
        case 0x9F:
            __opcode_stx(p, __get_address_direct(p));
            break;
        case 0xA0:
            __opcode_sub8(p, __get_address_indexed(p), &p->A, 0);
            break;
        case 0xA1:
            __opcode_sub8(p, __get_address_indexed(p), &p->A, 1);
            break;
        case 0xA3:
            __opcode_sub16(p, __get_address_indexed(p), &p->D, 0);
            break;
        case 0xA4:
            __opcode_and(p, __get_address_indexed(p), &p->A);
            break;
        case 0xA5:
            __opcode_bit8(p, __get_address_indexed(p), &p->A);
            break;
        case 0xA6:
            __opcode_lda(p, __get_address_indexed(p));
            break;
        case 0xA7:
            __opcode_sta(p, __get_address_indexed(p));
            break;
        case 0xA8:
            __opcode_eor(p, __get_address_indexed(p), &p->A);
            break;
        case 0xA9:
            __opcode_adca(p, __get_address_indexed(p));
            break;
        case 0xAA:
            __opcode_or(p, __get_address_indexed(p), &p->A);
            break;
        case 0xAB:
            __opcode_add8(p, __get_address_indexed(p), &p->A);
            break;
        case 0xAC:
            __opcode_sub16(p, __get_address_indexed(p), &p->X, 1);
            break;
        case 0xAD:
            __opcode_jsr(p, __get_address_indexed(p));
            break;
        case 0xAE:
            __opcode_ldx(p, __get_address_indexed(p));
            break;
        case 0xAF:
            __opcode_stx(p, __get_address_indexed(p));
            break;
        case 0xB0:
            __opcode_sub8(p, __get_address_extended(p), &p->A, 0);
            break;
        case 0xB1:
            __opcode_sub8(p, __get_address_extended(p), &p->A, 1);
            break;
        case 0xB3:
            __opcode_sub16(p, __get_address_extended(p), &p->D, 0);
            break;
        case 0xB4:
            __opcode_and(p, __get_address_extended(p), &p->A);
            break;
        case 0xB5:
            __opcode_bit8(p, __get_address_extended(p), &p->A);
            break;
        case 0xB6:
            __opcode_lda(p, __get_address_extended(p));
            break;
        case 0xB7:
            __opcode_sta(p, __get_address_extended(p));
            break;
        case 0xB8:
            __opcode_eor(p, __get_address_extended(p), &p->A);
            break;
        case 0xB9:
            __opcode_adca(p, __get_address_extended(p));
            break;
        case 0xBA:
            __opcode_or(p, __get_address_extended(p), &p->A);
            break;
        case 0xBB:
            __opcode_add8(p, __get_address_extended(p), &p->A);
            break;
        case 0xBC:
            __opcode_sub16(p, __get_address_extended(p), &p->X, 1);
            break;
        case 0xBD:
            __opcode_jsr(p, __get_address_extended(p));
            break;
        case 0xBE:
            __opcode_ldx(p, __get_address_extended(p));
            break;
        case 0xBF:
            __opcode_stx(p, __get_address_extended(p));
            break;
        case 0xC0:
            __opcode_sub8(p, __get_address_immediate8(p), &p->B, 0);
            break;
        case 0xC1:
            __opcode_sub8(p, __get_address_immediate8(p), &p->B, 1);
            break;
        case 0xC3:
            __opcode_add16(p, __get_address_immediate16(p), &p->D);
            break;
        case 0xC4:
            __opcode_and(p, __get_address_immediate8(p), &p->B);
            break;
        case 0xC5:
            __opcode_bit8(p, __get_address_immediate8(p), &p->B);
            break;
        case 0xC6:
            __opcode_ldb(p, __get_address_immediate8(p));
            break;
        case 0xC8:
            __opcode_eor(p, __get_address_immediate8(p), &p->B);
            break;
        case 0xC9:
            __opcode_adcb(p, __get_address_immediate8(p));
            break;
        case 0xCA:
            __opcode_or(p, __get_address_immediate8(p), &p->B);
            break;
        case 0xCB:
            __opcode_add8(p, __get_address_immediate8(p), &p->B);
            break;
        case 0xCC:
            __opcode_ldd(p, __get_address_immediate16(p));
            break;
        case 0xCE:
            __opcode_ldu(p, __get_address_immediate16(p));
            break;
        case 0xD0:
            __opcode_sub8(p, __get_address_direct(p), &p->B, 0);
            break;
        case 0xD1:
            __opcode_sub8(p, __get_address_direct(p), &p->B, 1);
            break;
        case 0xD3:
            __opcode_add16(p, __get_address_direct(p), &p->D);
            break;
        case 0xD4:
            __opcode_and(p, __get_address_direct(p), &p->B);
            break;
        case 0xD5:
            __opcode_bit8(p, __get_address_direct(p), &p->B);
            break;
        case 0xD6:
            __opcode_ldb(p, __get_address_direct(p));
            break;
        case 0xD7:
            __opcode_stb(p, __get_address_direct(p));
            break;
        case 0xD8:
            __opcode_eor(p, __get_address_direct(p), &p->B);
            break;
        case 0xD9:
            __opcode_adcb(p, __get_address_direct(p));
            break;
        case 0xDA:
            __opcode_or(p, __get_address_direct(p), &p->B);
            break;
        case 0xDB:
            __opcode_add8(p, __get_address_direct(p), &p->B);
            break;
        case 0xDC:
            __opcode_ldd(p, __get_address_direct(p));
            break;
        case 0xDD:
            __opcode_std(p, __get_address_direct(p));
            break;
        case 0xDE:
            __opcode_ldu(p, __get_address_direct(p));
            break;
        case 0xDF:
            __opcode_stu(p, __get_address_direct(p));
            break;
        case 0xE0:
            __opcode_sub8(p, __get_address_indexed(p), &p->B, 0);
            break;
        case 0xE1:
            __opcode_sub8(p, __get_address_indexed(p), &p->B, 1);
            break;
        case 0xE3:
            __opcode_add16(p, __get_address_indexed(p), &p->D);
            break;
        case 0xE4:
            __opcode_and(p, __get_address_indexed(p), &p->B);
            break;
        case 0xE5:
            __opcode_bit8(p, __get_address_indexed(p), &p->B);
            break;
        case 0xE6:
            __opcode_ldb(p, __get_address_indexed(p));
            break;
        case 0xE7:
            __opcode_stb(p, __get_address_indexed(p));
            break;
        case 0xE8:
            __opcode_eor(p, __get_address_indexed(p), &p->B);
            break;
        case 0xE9:
            __opcode_adcb(p, __get_address_indexed(p));
            break;
        case 0xEA:
            __opcode_or(p, __get_address_indexed(p), &p->B);
            break;
        case 0xEB:
            __opcode_add8(p, __get_address_indexed(p), &p->B);
            break;
        case 0xEC:
            __opcode_ldd(p, __get_address_indexed(p));
            break;
        case 0xED:
            __opcode_std(p, __get_address_indexed(p));
            break;
        case 0xEE:
            __opcode_ldu(p, __get_address_indexed(p));
            break;
        case 0xEF:
            __opcode_stu(p, __get_address_indexed(p));
            break;
        case 0xF0:
            __opcode_sub8(p, __get_address_extended(p), &p->B, 0);
            break;
        case 0xF1:
            __opcode_sub8(p, __get_address_extended(p), &p->B, 1);
            break;
        case 0xF3:
            __opcode_add16(p, __get_address_extended(p), &p->D);
            break;
        case 0xF4:
            __opcode_and(p, __get_address_extended(p), &p->B);
            break;
        case 0xF5:
            __opcode_bit8(p, __get_address_extended(p), &p->B);
            break;
        case 0xF6:
            __opcode_ldb(p, __get_address_extended(p));
            break;
        case 0xF7:
            __opcode_stb(p, __get_address_extended(p));
            break;
        case 0xF8:
            __opcode_eor(p, __get_address_extended(p), &p->B);
            break;
        case 0xF9:
            __opcode_adcb(p, __get_address_extended(p));
            break;
        case 0xFA:
            __opcode_or(p, __get_address_extended(p), &p->B);
            break;
        case 0xFB:
            __opcode_add8(p, __get_address_extended(p), &p->B);
            break;
        case 0xFC:
            __opcode_ldd(p, __get_address_extended(p));
            break;
        case 0xFD:
            __opcode_std(p, __get_address_extended(p));
            break;
        case 0xFE:
            __opcode_ldu(p, __get_address_extended(p));
            break;
        case 0xFF:
            __opcode_stu(p, __get_address_extended(p));
            break;
        case 0x1021: // LBRN
           {
                __get_address_relative16(p);
                // NOPE
            }
            break;
        case 0x1022: // LBHI
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if (p->Z == 0 && p->C == 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x1023: // LBLS
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if (p->Z != 0 || p->C != 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x1024: // BHS, LBCC
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if (p->C == 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x1025: // BLO, LBCS
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if (p->C != 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x1026: // LBNE
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if (p->Z == 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x1027: // LBEQ
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if (p->Z != 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x1028: // LBVC
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if (p->V == 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x1029: // LBVS
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if (p->V != 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x102A: // LBPL
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if (p->N == 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x102B: // LBMI
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if (p->N != 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x102C: // LBGE
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if (p->N == p->V)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x102D: // LBLT
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if (p->N != p->V)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x102E: // LBGT
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if (p->N == p->V && p->Z == 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x102F: // LBLE
            {
                uint16_t jmp_address = __get_address_relative16(p);
                if ((p->N != p->V) || p->Z != 0)
                    __opcode_jmp(p, jmp_address);
            }
            break;
        case 0x1083:
            __opcode_sub16(p, __get_address_immediate16(p), &p->D, 1);
            break;
        case 0x108C:
            __opcode_sub16(p, __get_address_immediate16(p), &p->Y, 1);
            break;
        case 0x108E:
            __opcode_ldy(p, __get_address_immediate16(p));
            break;
        case 0x1093:
            __opcode_sub16(p, __get_address_direct(p), &p->D, 1);
            break;
        case 0x109C:
            __opcode_sub16(p, __get_address_direct(p), &p->Y, 1);
            break;
        case 0x109E:
            __opcode_ldy(p, __get_address_direct(p));
            break;
        case 0x109F:
            __opcode_sty(p, __get_address_direct(p));
            break;
        case 0x10A3:
            __opcode_sub16(p, __get_address_indexed(p), &p->D, 1);
            break;
        case 0x10AC:
            __opcode_sub16(p, __get_address_indexed(p), &p->Y, 1);
            break;
        case 0x10AE:
            __opcode_ldy(p, __get_address_indexed(p));
            break;
        case 0x10AF:
            __opcode_sty(p, __get_address_indexed(p));
            break;
        case 0x10B3:
            __opcode_sub16(p, __get_address_extended(p), &p->D, 1);
            break;
        case 0x10BC:
            __opcode_sub16(p, __get_address_extended(p), &p->Y, 1);
            break;
        case 0x10BE:
            __opcode_ldy(p, __get_address_extended(p));
            break;
        case 0x10BF:
            __opcode_sty(p, __get_address_extended(p));
            break;
        case 0x10CE:
            __opcode_lds(p, __get_address_immediate16(p));
            break;
        case 0x10DE:
            __opcode_lds(p, __get_address_direct(p));
            break;
        case 0x10DF:
            __opcode_sts(p, __get_address_direct(p));
            break;
        case 0x10EE:
            __opcode_lds(p, __get_address_indexed(p));
            break;
        case 0x10EF:
            __opcode_sts(p, __get_address_indexed(p));
            break;
        case 0x10FE:
            __opcode_lds(p, __get_address_extended(p));
            break;
        case 0x10FF:
            __opcode_sts(p, __get_address_extended(p));
            break;
        case 0x1183:
            __opcode_sub16(p, __get_address_immediate16(p), &p->U, 1);
            break;
        case 0x118C:
            __opcode_sub16(p, __get_address_immediate16(p), &p->S, 1);
            break;
        case 0x1193:
            __opcode_sub16(p, __get_address_direct(p), &p->U, 1);
            break;
        case 0x119C:
            __opcode_sub16(p, __get_address_direct(p), &p->S, 1);
            break;
        case 0x11A3:
            __opcode_sub16(p, __get_address_indexed(p), &p->U, 1);
            break;
        case 0x11AC:
            __opcode_sub16(p, __get_address_indexed(p), &p->S, 1);
            break;
        case 0x11B3:
            __opcode_sub16(p, __get_address_extended(p), &p->U, 1);
            break;
        case 0x11BC:
            __opcode_sub16(p, __get_address_extended(p), &p->S, 1);
            break;
        default:
            printf("Unknown OPCODE %04X\n", opcode);
            exit(1);
    }
}

void processor_next_opcode(struct processor_state *p) {
    uint16_t org_address = p->PC;
    uint16_t opcode = processor_load_8(p, p->PC++);

    while (opcode == 0x10 || opcode == 0x11) {
        uint16_t opcode_high = opcode << 8;
        opcode = processor_load_8(p, p->PC++);
        if (!(opcode == 0x10 || opcode == 0x11)) opcode = opcode_high + opcode;
    }
    if (org_address == 0xA30A){
        // printf("Executing %04X opcode %04X\n", org_address, opcode);
        execute_opcode(p, opcode);
        // processor_dump(p);
    } else {
        // printf("Executing %04X opcode %04X\n", org_address, opcode);
        execute_opcode(p, opcode);
        // processor_dump(p);
    }
}

uint8_t _bus_memory_read(struct bus_adaptor *p, uint16_t addr) {
    uint8_t *data = (uint8_t *)p->data;
    return data[addr];
}

void _bus_memory_write(struct bus_adaptor *p, uint16_t addr, uint8_t value) {
    uint8_t *data = (uint8_t *)p->data;
    data[addr] = value;
}

struct bus_adaptor * bus_create_rom(uint8_t *data, uint16_t size, uint16_t start) {
    struct bus_adaptor *adaptor = malloc(sizeof(struct bus_adaptor));

    adaptor->start = start;
    adaptor->end = start + size -1;
    adaptor->data = data;
    adaptor->load_8 = _bus_memory_read;
    adaptor->store_8 = NULL;

    printf("Created rom start=%04X end=%04X\n", adaptor->start, adaptor->end);

    return adaptor;
}

struct bus_adaptor * bus_create_ram(uint16_t size, uint16_t start) {
    struct bus_adaptor *adaptor = malloc(sizeof(struct bus_adaptor));
    void *data = malloc(size);

    adaptor->start = start;
    adaptor->end = start + size -1;
    adaptor->data = data;
    adaptor->load_8 = _bus_memory_read;
    adaptor->store_8 = _bus_memory_write;

    printf("Created ram start=%04X end=%04X\n", adaptor->start, adaptor->end);

    return adaptor;
}

void load_trs80(void) {
    uint8_t *basic_rom = malloc(16 * 1024);
    FILE *fp = fopen("roms/BASIC.ROM", "rb");
    if (!fp) {
        perror("error reading basic rom");
        exit(1);
    }
    size_t remaining = 8 * 1024;
    uint16_t pos = 0;
    while (remaining > 0) {
        size_t ret = fread(basic_rom + pos, 1, 1024, fp);
        if (ret <=0) {
            fprintf(stderr, "fread() failed: %zu\n", ret);
            exit(EXIT_FAILURE);
        }
        remaining -= ret;
        pos += ret;
    }
    fclose(fp);

    struct processor_state p;
    processor_init(&p);

    struct bus_adaptor *rom = bus_create_rom(basic_rom, 8 * 1024, 0xA000);
    processor_register_bus_adaptor(&p, rom);

    struct bus_adaptor *ram = bus_create_ram(16 * 1024, 0x0000);
    processor_register_bus_adaptor(&p, ram);

    processor_reset(&p);

    while (1) {
        // processor_dump(&p);
        processor_next_opcode(&p);
    }

    processor_dump(&p);
}

int main(void) {
    load_trs80();
}

int _main(void) {
    struct processor_state p;
    processor_init(&p);

    uint8_t rom_data[] = {0x8E, 0x01, 0x07, 0xc6, 0xfe, 0xA6, 0x85, 0x10, 0x20};
    struct bus_adaptor *rom = bus_create_rom(rom_data, sizeof(rom_data), 0x100);
    processor_register_bus_adaptor(&p, rom);

    p.PC = 0x100;
    processor_next_opcode(&p);
    processor_next_opcode(&p);
    processor_next_opcode(&p);

    processor_dump(&p);
    return 0;
}