#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "processor_6809.h"

#define add_cycles(t) p->_nano_time_passed += t * cycle_nano
#define processor_load_8(p, addr) bus_read_8(&p->bus, addr)
#define processor_store_8(p, addr, value) bus_write_8(&p->bus, addr, value)

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
    printf("    Processor dump:");
    printf("      A:%02X, B:%02X, D:%04X", p->A, p->B, p->D);
    printf("      X:%04X, Y:%04X, U:%04X, S:%04X, PC:%04X, DP:%02X", p->X, p->Y, p->U, p->S, p->PC, p->DP);
    printf("      C:%c, V:%c, Z:%c, N:%c, I:%c, H:%c, F:%c, E:%c\n\n", _bit(p->C), _bit(p->V), _bit(p->Z), _bit(p->N), _bit(p->I), _bit(p->H), _bit(p->F), _bit(p->E));
}

uint16_t __get_address_direct(struct processor_state *p) {
    uint16_t addr_high = p->DP;
    uint8_t addr_low = processor_load_8(p, p->PC++);

    return (addr_high << 8) | addr_low;
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
    uint16_t offset = processor_load_8(p, p->PC++);
    uint16_t index = p->PC;
    if (offset & 0x80) {
        offset = offset | 0xff00;
    }
    index = index + offset;

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

    if (post_byte == 0b10011111) {
        index = processor_load_16(p, p->PC++);
        p->PC++;
        index = processor_load_16(p, index);
        add_cycles(5);

        return index;
    }

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
    uint16_t offset_code = post_byte & 0b11111;
    if ((post_byte & 0b10000000) == 0) {
        if (offset_code & 0b10000) {
            offset_code = offset_code | 0xffe0;
        }
        add_cycles(1);
        return index + offset_code;
    }

    uint8_t indirect = offset_code & 0b10000;

    switch(offset_code & 0b1111) {
        case 0b0100:
            // No offset
            break;
        case 0b1000:
            // 8-bit offset
            {
                uint16_t offset = processor_load_8(p, p->PC++);
                if (offset & 0x80) {
                    offset = offset | 0xff00;
                }
                index = index + offset;
            }
            add_cycles(1);
            break;
        case 0b1001:
            // 16-bit offset
            {
                uint16_t offset = processor_load_16(p, p->PC++);
                p->PC++;
                index = index + ((int16_t)offset);
            }
            add_cycles(4);
            break;
        case 0b0110:
            // A register offset
            {
                uint16_t offset = p->A;
                if (offset & 0x80) {
                    offset = offset | 0xff00;
                }
                index = index + offset;
            }
            add_cycles(1);
            break;
        case 0b0101:
            // B register offset
            {
                uint16_t offset = p->B;
                if (offset & 0x80) {
                    offset = offset | 0xff00;
                }
                index = index + offset;
            }
            add_cycles(1);
            break;
        case 0b1011:
            // D register offset
            index = index + p->D;
            add_cycles(4);
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
            add_cycles(2);
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
            add_cycles(3);
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
            add_cycles(2);
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
            add_cycles(3);
            break;
        case 0b1100:
            // constant offset from PC - 8 bit offset
            {
                uint16_t offset = processor_load_8(p, p->PC++);
                if (offset & 0x80) {
                    offset = offset | 0xff00;
                }
                index = p->PC;
                index = index + offset;
            }
            add_cycles(1);
            break;
        case 0b1101:
            // constant offset from PC - 16 bit offset
            {
                uint16_t offset = processor_load_16(p, p->PC++);
                p->PC++;
                index = p->PC;
                index = index + offset;
            }
            add_cycles(5);
            break;
        default:
            printf("Invalid addressing %02x\n", post_byte);
            exit(0);
    }
    if (indirect) {
        index = processor_load_16(p, index);
        add_cycles(3);
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

void __opcode_adc(struct processor_state *p, uint16_t address, uint8_t *reg) {
    uint8_t data = processor_load_8(p, address);
    uint16_t result;

    p->H = ((data & 0xf) + (*reg & 0xf) + (p->C ? 1 : 0)) & 0b00010000;
    uint8_t b7c = ((data & 0x7f) + (*reg & 0x7f) + (p->C ? 1 : 0)) & 0b10000000;

    result = data + *reg + (p->C ? 1 : 0);
    p->C = (result & 0x0100) ? 1 : 0;
    p->V = (p->C ? 1 : 0) != (b7c ? 1 : 0);

    *reg = 0xff & result;
    __update_CC_data8(p, *reg);
}

void __opcode_add8(struct processor_state *p, uint16_t address, uint8_t *reg) {
    uint8_t data = processor_load_8(p, address);
    uint16_t result;

    p->H = ((data & 0xf) + (*reg & 0xf)) & 0b00010000;
    uint8_t b7c = ((data & 0x7f) + (*reg & 0x7f)) & 0b10000000;

    result = data + *reg;
    p->C = (result & 0x0100) ? 1 : 0;
    p->V = (p->C ? 1 : 0) != (b7c ? 1 : 0);

    *reg = 0xff & result;
    __update_CC_data8(p, *reg);
}

void __opcode_add16(struct processor_state *p, uint16_t address, uint16_t *reg) {
    uint16_t data = processor_load_16(p, address);
    uint32_t result;

    uint16_t b7c = ((data & 0x7fff) + (*reg & 0x7fff)) & 0x8000;

    result = data + *reg;
    p->C = (result & 0x010000) ? 1 : 0;
    p->V = (p->C ? 1 : 0) != (b7c ? 1 : 0);

    *reg = 0xffff & result;
    __update_CC_data16(p, *reg);
}

void __opcode_and(struct processor_state *p, uint16_t address, uint8_t *reg) {
    uint8_t data = processor_load_8(p, address);
    *reg = *reg & data;
    __update_CC_data8(p, *reg);
    p->V = 0;
}

void __opcode_andcc(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    p->CC = p->CC & data;
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

void __opcode_bit8(struct processor_state *p, uint16_t address, uint8_t *reg) {
    uint8_t data = processor_load_8(p, address);
    uint8_t result = *reg & data;
    p->V = 0;
    __update_CC_data8(p, result);
}

void __opcode_mul(struct processor_state *p) {
    p->D = p->A * p->B;
    p->C = p->B & 0x80 ? 1 : 0;
    p->Z = p->D == 0 ? 1 : 0;
}

void __opcode_neg(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    data = (~data) + 1;
    processor_store_8(p, address, data);
    __update_CC_data8(p, data);
    p->V = data == 0x80 ? 1 : 0;
    p->C = data == 0x0 ? 0 : 1;
}

void __opcode_neg_reg(struct processor_state *p, uint8_t *reg) {
    uint8_t data = *reg;
    data = (~data) + 1;
    *reg = data;
    __update_CC_data8(p, data);
    p->V = data == 0x80 ? 1 : 0;
    p->C = data == 0x0 ? 0 : 1;
}

void __opcode_inc(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    p->V = data == 0x7F ? 1 : 0;
    data += 1;
    processor_store_8(p, address, data);
    __update_CC_data8(p, data);
}

void __opcode_inc_reg(struct processor_state *p, uint8_t *reg) {
    p->V = *reg == 0x7F ? 1 : 0;
    *reg += 1;
    __update_CC_data8(p, *reg);
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
    p->V = data == 0x80 ? 1 : 0;
    data -= 1;
    processor_store_8(p, address, data);
    __update_CC_data8(p, data);
}

void __opcode_dec_reg(struct processor_state *p, uint8_t *reg) {
    p->V = *reg == 0x80 ? 1 : 0;
    *reg -= 1;
    __update_CC_data8(p, *reg);
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

void __opcode_rti(struct processor_state *p) {
    p->CC = processor_load_8(p, p->S);
    p->S++;
    if (p->E) {
        p->A = processor_load_8(p, p->S);
        p->S++;
        p->B = processor_load_8(p, p->S);
        p->S++;
        p->DP = processor_load_8(p, p->S);
        p->S++;
        p->X = processor_load_16(p, p->S);
        p->S += 2;
        p->Y = processor_load_16(p, p->S);
        p->S += 2;
        p->U = processor_load_16(p, p->S);
        p->S += 2;
        add_cycles(9);
    }
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

void __opcode_sub8(struct processor_state *p, uint16_t address, uint8_t *reg, int compare_only) {
    uint8_t data = processor_load_8(p, address);
    uint16_t result;

    result = (*reg) - data;

    p->C = result > 0xff ? 1 : 0;
    p->V = (((*reg ^ result) & 0x80) && ((*reg ^ data) & 0x80)) ? 1 :0;
    __update_CC_data8(p, result);

    if (!compare_only) *reg = result & 0xff;
}

void __opcode_sub16(struct processor_state *p, uint16_t address, uint16_t *reg, int compare_only) {
    uint16_t data = processor_load_16(p, address);
    uint32_t result;

    result = (*reg) - data;

    p->C = result > 0xffff ? 1 : 0;
    p->V = (((*reg ^ result) & 0x8000) && ((*reg ^ data) & 0x8000)) ? 1 :0;
    __update_CC_data16(p, result);

    if (!compare_only) *reg = result & 0xffff;
}

void __opcode_sbc(struct processor_state *p, uint16_t address, uint8_t *reg) {
    uint8_t data = processor_load_8(p, address);
    uint16_t result;

    result = (*reg) - data - (p->C ? 1 : 0);

    p->C = result > 0xff ? 1 : 0;
    p->V = (((*reg ^ result) & 0x80) && ((*reg ^ data) & 0x80)) ? 1 :0;
    __update_CC_data8(p, result);
    *reg = result & 0xff;
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
    return 0xffff;
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

    h = p->C != 0 || h > 9 || ( h > 8 && l > 9) ? (h + 6) & 0xf : h;
    l = p->H != 0 || l > 9 ? (l + 6) & 0xf : l;

    p->C = p->C != 0 || h > 9 || ( h > 8 && l > 9) ? 1 : 0;

    p->A = (h << 4) | l;
    __update_CC_data8(p, p->A);
}

void __opcode_eor(struct processor_state *p, uint16_t address, uint8_t *reg) {
    uint8_t data = processor_load_8(p, address);
    *reg = (*reg) ^ data;
    __update_CC_data8(p, *reg);
    p->V = 0;
}

void __opcode_nop(struct processor_state *p) {
}

void __opcode_abx(struct processor_state *p) {
    p->X += p->B;
}

void __opcode_rol(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    int new_c = data & 0x80 ? 1 : 0;
    int bit6 = data & 0x40 ? 1 : 0;
    int bit7 = data & 0x80 ? 1 : 0;
    p->V = bit6 ^ bit7;
    data = (data << 1) | (p->C ? 1 : 0);
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
    *reg = (data << 1) | (p->C ? 1 : 0);
    __update_CC_data8(p, *reg);
    p->C = new_c;
}

void __opcode_ror(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    int bit7 = p->C ? 0x80 : 0;
    p->C = data & 1;
    data = (data >> 1) | bit7;
    processor_store_8(p, address, data);
    __update_CC_data8(p, data);
}

void __opcode_ror_reg(struct processor_state *p, uint8_t *reg) {
    uint8_t data = *reg;
    int bit7 = p->C ? 0x80 : 0;
    p->C = data & 1;
    *reg = (data >> 1) | bit7;
    __update_CC_data8(p, *reg);
}

void __opcode_lsr_reg(struct processor_state *p, uint8_t *reg) {
    p->C = *reg & 1;
    *reg = *reg >> 1;
    __update_CC_data8(p, *reg);
}

void __opcode_tst(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    __update_CC_data8(p, data);
    p->V = 0;
}

void __opcode_tst_reg(struct processor_state *p, uint8_t *reg) {
    __update_CC_data8(p, *reg);
    p->V = 0;
}

void __opcode_pshs(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    if (data & 0b10000000) {
        p->S -= 2;
        processor_store_16(p, p->S, p->PC);
        add_cycles(2);
    }
    if (data & 0b1000000) {
        p->S -= 2;
        processor_store_16(p, p->S, p->U);
        add_cycles(2);
    }
    if (data & 0b100000) {
        p->S -= 2;
        processor_store_16(p, p->S, p->Y);
        add_cycles(2);
    }
    if (data & 0b10000) {
        p->S -= 2;
        processor_store_16(p, p->S, p->X);
        add_cycles(2);
    }
    if (data & 0b1000) {
        p->S--;
        processor_store_8(p, p->S, p->DP);
        add_cycles(1);
    }
    if (data & 0b100) {
        p->S--;
        processor_store_8(p, p->S, p->B);
        add_cycles(1);
    }
    if (data & 0b10) {
        p->S--;
        processor_store_8(p, p->S, p->A);
        add_cycles(1);
    }
    if (data & 0b1) {
        p->S--;
        processor_store_8(p, p->S, p->CC);
        add_cycles(1);
    }
}

void __opcode_pshu(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    if (data & 0b10000000) {
        p->U -= 2;
        processor_store_16(p, p->U, p->PC);
        add_cycles(2);
    }
    if (data & 0b1000000) {
        p->U -= 2;
        processor_store_16(p, p->U, p->S);
        add_cycles(2);
    }
    if (data & 0b100000) {
        p->U -= 2;
        processor_store_16(p, p->U, p->Y);
        add_cycles(2);
    }
    if (data & 0b10000) {
        p->U -= 2;
        processor_store_16(p, p->U, p->X);
        add_cycles(2);
    }
    if (data & 0b1000) {
        p->U--;
        processor_store_8(p, p->U, p->DP);
        add_cycles(1);
    }
    if (data & 0b100) {
        p->U--;
        processor_store_8(p, p->U, p->B);
        add_cycles(1);
    }
    if (data & 0b10) {
        p->U--;
        processor_store_8(p, p->U, p->A);
        add_cycles(1);
    }
    if (data & 0b1) {
        p->U--;
        processor_store_8(p, p->U, p->CC);
        add_cycles(1);
    }
}

void __opcode_puls(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    if (data & 0b1) {
        p->CC = processor_load_8(p, p->S);
        p->S++;
        add_cycles(1);
    }
    if (data & 0b10) {
        p->A = processor_load_8(p, p->S);
        p->S++;
        add_cycles(1);
    }
    if (data & 0b100) {
        p->B = processor_load_8(p, p->S);
        p->S++;
        add_cycles(1);
    }
    if (data & 0b1000) {
        p->DP = processor_load_8(p, p->S);
        p->S++;
        add_cycles(1);
    }
    if (data & 0b10000) {
        p->X = processor_load_16(p, p->S);
        p->S += 2;
        add_cycles(2);
    }
    if (data & 0b100000) {
        p->Y = processor_load_16(p, p->S);
        p->S += 2;
        add_cycles(2);
    }
    if (data & 0b1000000) {
        p->U = processor_load_16(p, p->S);
        p->S += 2;
        add_cycles(2);
    }
    if (data & 0b10000000) {
        p->PC = processor_load_16(p, p->S);
        p->S += 2;
        add_cycles(2);
    }
}

void __opcode_pulu(struct processor_state *p, uint16_t address) {
    uint8_t data = processor_load_8(p, address);
    if (data & 0b1) {
        p->CC = processor_load_8(p, p->U);
        p->U++;
        add_cycles(1);
    }
    if (data & 0b10) {
        p->A = processor_load_8(p, p->U);
        p->U++;
        add_cycles(1);
    }
    if (data & 0b100) {
        p->B = processor_load_8(p, p->U);
        p->U++;
        add_cycles(1);
    }
    if (data & 0b1000) {
        p->DP = processor_load_8(p, p->U);
        p->U++;
        add_cycles(1);
    }
    if (data & 0b10000) {
        p->X = processor_load_16(p, p->U);
        p->U += 2;
        add_cycles(2);
    }
    if (data & 0b100000) {
        p->Y = processor_load_16(p, p->U);
        p->U += 2;
        add_cycles(2);
    }
    if (data & 0b1000000) {
        p->S = processor_load_16(p, p->U);
        p->U += 2;
        add_cycles(2);
    }
    if (data & 0b10000000) {
        p->PC = processor_load_16(p, p->U);
        p->U += 2;
        add_cycles(2);
    }
}

#define bit_value(i) (i ? 1 : 0)
#define op_code(op, mnem, cycles, exe, ...) case op: add_cycles(cycles); exe(p, ##__VA_ARGS__); break;
#define op_code_direct(op, mnem, cycles, exe, ...) case op: add_cycles(cycles); exe(p, __get_address_direct(p), ##__VA_ARGS__); break;
#define op_code_relative16(op, mnem, cycles, exe) case op: add_cycles(cycles); exe(p, __get_address_relative16(p)); break;
#define op_code_relative8(op, mnem, cycles, exe) case op: add_cycles(cycles); exe(p, __get_address_relative8(p)); break;
#define op_code_immediate8(op, mnem, cycles, exe, ...) case op: add_cycles(cycles); exe(p, __get_address_immediate8(p), ##__VA_ARGS__); break;
#define op_code_immediate16(op, mnem, cycles, exe, ...) case op: add_cycles(cycles); exe(p, __get_address_immediate16(p), ##__VA_ARGS__); break;
#define op_code_indexed(op, mnem, cycles, exe, ...) case op: add_cycles(cycles); exe(p, __get_address_indexed(p), ##__VA_ARGS__); break;
#define op_code_extended(op, mnem, cycles, exe, ...) case op: add_cycles(cycles); exe(p, __get_address_extended(p), ##__VA_ARGS__); break;
#define op_code_branch8(op, mnem, cycles, exe) case op: add_cycles(cycles); {uint16_t jmp_address = __get_address_relative8(p); if (exe) __opcode_jmp(p, jmp_address);} break;
#define op_code_branch16(op, mnem, cycles, exe) case op: add_cycles(cycles); {uint16_t jmp_address = __get_address_relative16(p); if (exe) {__opcode_jmp(p, jmp_address); add_cycles(1);}} break;

void execute_opcode(struct processor_state *p, uint16_t opcode) {
    switch(opcode) {
        op_code_direct(0x00, 'NEG', 6, __opcode_neg)
        op_code_direct(0x03, 'COM', 6, __opcode_com)
        op_code_direct(0x04, 'LSR', 6, __opcode_lsr8)
        op_code_direct(0x06, 'ROR', 6, __opcode_ror)
        op_code_direct(0x07, 'ASR', 6, __opcode_asr)
        op_code_direct(0x08, 'ASL', 6, __opcode_asl)
        op_code_direct(0x09, 'ROL', 6, __opcode_rol)
        op_code_direct(0x0A, 'DEC', 6, __opcode_dec)
        op_code_direct(0x0C, 'INC', 6, __opcode_inc)
        op_code_direct(0x0D, 'TST', 6, __opcode_tst)
        op_code_direct(0x0E, 'JMP', 3, __opcode_jmp)
        op_code_direct(0x0F, 'CLR', 6, __opcode_clr)

        op_code(0x12, 'NOP', 2, __opcode_nop)
        op_code_relative16(0x16, 'LBRA', 5, __opcode_jmp)
        op_code_relative16(0x17, 'LBSR', 9, __opcode_jsr)
        op_code(0x19, 'DAA', 2, __opcode_daa)
        op_code_immediate8(0x1A, 'ORCC', 3, __opcode_orcc)
        op_code_immediate8(0x1C, 'ANDCC', 3, __opcode_andcc)
        op_code_immediate8(0x1E, 'EXG', 8, __opcode_exg)
        op_code_immediate8(0x1F, 'TFR', 6, __opcode_tfr)

        op_code_branch8(0x20, 'BRA', 3, 1)
        op_code_branch8(0x21, 'BRN', 3, 0)
        op_code_branch8(0x22, 'BHI', 3, p->Z == 0 && p->C == 0)
        op_code_branch8(0x23, 'BLS', 3, p->Z != 0 || p->C != 0)
        op_code_branch8(0x24, 'BHS', 3, p->C == 0)
        op_code_branch8(0x25, 'BLO', 3, p->C != 0)
        op_code_branch8(0x26, 'BNE', 3, p->Z == 0)
        op_code_branch8(0x27, 'BEQ', 3, p->Z != 0)
        op_code_branch8(0x28, 'BVC', 3, p->V == 0)
        op_code_branch8(0x29, 'BVS', 3, p->V != 0)
        op_code_branch8(0x2A, 'BPL', 3, p->N == 0)
        op_code_branch8(0x2B, 'BMI', 3, p->N != 0)
        op_code_branch8(0x2C, 'BGE', 3, bit_value(p->N) == bit_value(p->V))
        op_code_branch8(0x2D, 'BLT', 3, bit_value(p->N) != bit_value(p->V))
        op_code_branch8(0x2E, 'BGT', 3, bit_value(p->N) == bit_value(p->V) && p->Z == 0)
        op_code_branch8(0x2F, 'BLE', 3, (bit_value(p->N) != bit_value(p->V)) || p->Z != 0)

        op_code_indexed(0x30, 'LEAX', 4, __opcode_leax)
        op_code_indexed(0x31, 'LEAY', 4, __opcode_leay)
        op_code_indexed(0x32, 'LEAS', 4, __opcode_leas)
        op_code_indexed(0x33, 'LEAY', 4, __opcode_leau)
        op_code_immediate8(0x34, 'PSHS', 5, __opcode_pshs)
        op_code_immediate8(0x35, 'PULS', 5, __opcode_puls)
        op_code_immediate8(0x36, 'PSHU', 5, __opcode_pshu)
        op_code_immediate8(0x37, 'PULU', 5, __opcode_pulu)
        op_code(0x39, 'RTS', 5, __opcode_rts)
        op_code(0x3A, 'ABX', 3, __opcode_abx)
        op_code(0x3B, 'RTS', 6, __opcode_rti)
        op_code(0x3D, 'MUL', 11, __opcode_mul)

        op_code(0x40, 'NEGA', 2, __opcode_neg_reg, &p->A)
        op_code(0x43, 'COMA', 2, __opcode_com_reg, &p->A)
        op_code(0x44, 'LSRA', 2, __opcode_lsr_reg, &p->A)
        op_code(0x46, 'RORA', 2, __opcode_ror_reg, &p->A)
        op_code(0x47, 'ASRA', 2, __opcode_asr_reg, &p->A)
        op_code(0x48, 'ASLA', 2, __opcode_asl_reg, &p->A)
        op_code(0x49, 'ROLA', 2, __opcode_rol_reg, &p->A)
        op_code(0x4A, 'DECA', 2, __opcode_dec_reg, &p->A)
        op_code(0x4C, 'INCA', 2, __opcode_inc_reg, &p->A)
        op_code(0x4D, 'TSTA', 2, __opcode_tst_reg, &p->A)
        op_code(0x4F, 'CLRA', 2, __opcode_clr_reg, &p->A)

        op_code(0x50, 'NEGB', 2, __opcode_neg_reg, &p->B)
        op_code(0x53, 'COMB', 2, __opcode_com_reg, &p->B)
        op_code(0x54, 'LSRB', 2, __opcode_lsr_reg, &p->B)
        op_code(0x56, 'RORB', 2, __opcode_ror_reg, &p->B)
        op_code(0x57, 'ASRB', 2, __opcode_asr_reg, &p->B)
        op_code(0x58, 'ASLB', 2, __opcode_asl_reg, &p->B)
        op_code(0x59, 'ROLB', 2, __opcode_rol_reg, &p->B)
        op_code(0x5A, 'DECB', 2, __opcode_dec_reg, &p->B)
        op_code(0x5C, 'INCB', 2, __opcode_inc_reg, &p->B)
        op_code(0x5D, 'TSTB', 2, __opcode_tst_reg, &p->B)
        op_code(0x5F, 'CLRB', 2, __opcode_clr_reg, &p->B)

        op_code_indexed(0x60, 'NEG', 6, __opcode_neg)
        op_code_indexed(0x63, 'COM', 6, __opcode_com)
        op_code_indexed(0x64, 'LSR', 6, __opcode_lsr8)
        op_code_indexed(0x66, 'ROR', 6, __opcode_ror)
        op_code_indexed(0x67, 'ASR', 6, __opcode_asr)
        op_code_indexed(0x68, 'ASL', 6, __opcode_asl)
        op_code_indexed(0x69, 'ROL', 6, __opcode_rol)
        op_code_indexed(0x6A, 'DEC', 6, __opcode_dec)
        op_code_indexed(0x6C, 'INC', 6, __opcode_inc)
        op_code_indexed(0x6D, 'TST', 6, __opcode_tst)
        op_code_indexed(0x6E, 'JMP', 3, __opcode_jmp)
        op_code_indexed(0x6F, 'CLR', 6, __opcode_clr)

        op_code_extended(0x70, 'NEG', 7, __opcode_neg)
        op_code_extended(0x73, 'COM', 7, __opcode_com)
        op_code_extended(0x74, 'LSR', 7, __opcode_lsr8)
        op_code_extended(0x76, 'ROR', 7, __opcode_ror)
        op_code_extended(0x77, 'ASR', 7, __opcode_asr)
        op_code_extended(0x78, 'ASL', 7, __opcode_asl)
        op_code_extended(0x79, 'ROL', 7, __opcode_rol)
        op_code_extended(0x7A, 'DEC', 7, __opcode_dec)
        op_code_extended(0x7C, 'INC', 7, __opcode_inc)
        op_code_extended(0x7D, 'TST', 7, __opcode_tst)
        op_code_extended(0x7E, 'JMP', 4, __opcode_jmp)
        op_code_extended(0x7F, 'CLR', 7, __opcode_clr)

        op_code_immediate8(0x80, 'SUBA', 2, __opcode_sub8, &p->A, 0)
        op_code_immediate8(0x81, 'CMPA', 2, __opcode_sub8, &p->A, 1)
        op_code_immediate8(0x82, 'SBCA', 2, __opcode_sbc, &p->A)
        op_code_immediate16(0x83, 'SUBD', 4, __opcode_sub16, &p->D, 0);
        op_code_immediate8(0x84, 'ANDA', 2, __opcode_and, &p->A)
        op_code_immediate8(0x85, 'BITA', 2, __opcode_bit8, &p->A)
        op_code_immediate8(0x86, 'LDA', 2, __opcode_lda)
        op_code_immediate8(0x88, 'EORA', 2, __opcode_eor, &p->A)
        op_code_immediate8(0x89, 'ADCA', 2, __opcode_adc, &p->A)
        op_code_immediate8(0x8A, 'ORA', 2, __opcode_or, &p->A)
        op_code_immediate8(0x8B, 'ADDA', 2, __opcode_add8, &p->A)
        op_code_immediate16(0x8C, 'CMPX', 4, __opcode_sub16, &p->X, 1);
        op_code_relative8(0x8D, 'BSR', 7, __opcode_jsr)
        op_code_immediate16(0x8E, 'LDX', 4, __opcode_ldx);

        op_code_direct(0x90, 'SUBA', 4, __opcode_sub8, &p->A, 0)
        op_code_direct(0x91, 'CMPA', 4, __opcode_sub8, &p->A, 1)
        op_code_direct(0x92, 'SBCA', 4, __opcode_sbc, &p->A)
        op_code_direct(0x93, 'SUBD', 6, __opcode_sub16, &p->D, 0)
        op_code_direct(0x94, 'ANDA', 4, __opcode_and, &p->A)
        op_code_direct(0x95, 'BITA', 4, __opcode_bit8, &p->A)
        op_code_direct(0x96, 'LDA', 4, __opcode_lda)
        op_code_direct(0x97, 'STA', 4, __opcode_sta)
        op_code_direct(0x98, 'EORA', 4, __opcode_eor, &p->A)
        op_code_direct(0x99, 'ADCA', 4, __opcode_adc, &p->A)
        op_code_direct(0x9A, 'ORA', 4, __opcode_or, &p->A)
        op_code_direct(0x9B, 'ADDA', 4, __opcode_add8, &p->A)
        op_code_direct(0x9C, 'CMPX', 6, __opcode_sub16, &p->X, 1)
        op_code_direct(0x9D, 'JSR', 7, __opcode_jsr)
        op_code_direct(0x9E, 'LDX', 5, __opcode_ldx)
        op_code_direct(0x9F, 'STX', 5, __opcode_stx)

        op_code_indexed(0xA0, 'SUBA', 4, __opcode_sub8, &p->A, 0)
        op_code_indexed(0xA1, 'CMPA', 4, __opcode_sub8, &p->A, 1)
        op_code_indexed(0xA2, 'SBCA', 4, __opcode_sbc, &p->A)
        op_code_indexed(0xA3, 'SUBD', 6, __opcode_sub16, &p->D, 0)
        op_code_indexed(0xA4, 'ANDA', 4, __opcode_and, &p->A)
        op_code_indexed(0xA5, 'BITA', 4, __opcode_bit8, &p->A)
        op_code_indexed(0xA6, 'LDA', 4, __opcode_lda)
        op_code_indexed(0xA7, 'STA', 4, __opcode_sta)
        op_code_indexed(0xA8, 'EORA', 4, __opcode_eor, &p->A)
        op_code_indexed(0xA9, 'ADCA', 4, __opcode_adc, &p->A)
        op_code_indexed(0xAA, 'ORA', 4, __opcode_or, &p->A)
        op_code_indexed(0xAB, 'ADDA', 4, __opcode_add8, &p->A)
        op_code_indexed(0xAC, 'CMPX', 6, __opcode_sub16, &p->X, 1)
        op_code_indexed(0xAD, 'JSR', 7, __opcode_jsr)
        op_code_indexed(0xAE, 'LDX', 5, __opcode_ldx)
        op_code_indexed(0xAF, 'STX', 5, __opcode_stx)

        op_code_extended(0xB0, 'SUBA', 5, __opcode_sub8, &p->A, 0)
        op_code_extended(0xB1, 'CMPA', 5, __opcode_sub8, &p->A, 1)
        op_code_extended(0xB2, 'SBCA', 5, __opcode_sbc, &p->A)
        op_code_extended(0xB3, 'SUBD', 7, __opcode_sub16, &p->D, 0)
        op_code_extended(0xB4, 'ANDA', 5, __opcode_and, &p->A)
        op_code_extended(0xB5, 'BITA', 5, __opcode_bit8, &p->A)
        op_code_extended(0xB6, 'LDA', 5, __opcode_lda)
        op_code_extended(0xB7, 'STA', 5, __opcode_sta)
        op_code_extended(0xB8, 'EORA', 5, __opcode_eor, &p->A)
        op_code_extended(0xB9, 'ADCA', 5, __opcode_adc, &p->A)
        op_code_extended(0xBA, 'ORA', 5, __opcode_or, &p->A)
        op_code_extended(0xBB, 'ADDA', 5, __opcode_add8, &p->A)
        op_code_extended(0xBC, 'CMPX', 6, __opcode_sub16, &p->X, 1)
        op_code_extended(0xBD, 'JSR', 6, __opcode_jsr)
        op_code_extended(0xBE, 'LDX', 6, __opcode_ldx)
        op_code_extended(0xBF, 'STX', 6, __opcode_stx)

        op_code_immediate8(0xC0, 'SUBB', 2, __opcode_sub8, &p->B, 0)
        op_code_immediate8(0xC1, 'CMPB', 2, __opcode_sub8, &p->B, 1)
        op_code_immediate8(0xC2, 'SBCB', 2, __opcode_sbc, &p->B)
        op_code_immediate16(0xC3, 'ADDD', 4, __opcode_add16, &p->D)
        op_code_immediate8(0xC4, 'ANDB', 2, __opcode_and, &p->B)
        op_code_immediate8(0xC5, 'BITB', 2, __opcode_bit8, &p->B)
        op_code_immediate8(0xC6, 'LDB', 2, __opcode_ldb)
        op_code_immediate8(0xC8, 'EORB', 2, __opcode_eor, &p->B)
        op_code_immediate8(0xC9, 'ADCB', 2, __opcode_adc, &p->B)
        op_code_immediate8(0xCA, 'ORB', 2, __opcode_or, &p->B)
        op_code_immediate8(0xCB, 'ADDB', 2, __opcode_add8, &p->B)
        op_code_immediate16(0xCC, 'LDD', 3, __opcode_ldd)
        op_code_immediate16(0xCE, 'LDU', 3, __opcode_ldu)

        op_code_direct(0xD0, 'SUBB', 4, __opcode_sub8, &p->B, 0)
        op_code_direct(0xD1, 'CMPB', 4, __opcode_sub8, &p->B, 1)
        op_code_direct(0xD2, 'SBCB', 4, __opcode_sbc, &p->B)
        op_code_direct(0xD3, 'ADDD', 6, __opcode_add16, &p->D)
        op_code_direct(0xD4, 'ANDB', 4, __opcode_and, &p->B)
        op_code_direct(0xD5, 'BITB', 4, __opcode_bit8, &p->B)
        op_code_direct(0xD6, 'LDB', 4, __opcode_ldb)
        op_code_direct(0xD7, 'STB', 4, __opcode_stb)
        op_code_direct(0xD8, 'EORB', 4, __opcode_eor, &p->B)
        op_code_direct(0xD9, 'ADCB', 4, __opcode_adc, &p->B)
        op_code_direct(0xDA, 'ORB', 4, __opcode_or, &p->B)
        op_code_direct(0xDB, 'ADDB', 4, __opcode_add8, &p->B)
        op_code_direct(0xDC, 'LDD', 5, __opcode_ldd)
        op_code_direct(0xDD, 'STD', 5, __opcode_std)
        op_code_direct(0xDE, 'LDU', 5, __opcode_ldu)
        op_code_direct(0xDF, 'STU', 5, __opcode_stu)

        op_code_indexed(0xE0, 'SUBB', 4, __opcode_sub8, &p->B, 0)
        op_code_indexed(0xE1, 'CMPB', 4, __opcode_sub8, &p->B, 1)
        op_code_indexed(0xE2, 'SBCB', 4, __opcode_sbc, &p->B)
        op_code_indexed(0xE3, 'ADDD', 6, __opcode_add16, &p->D)
        op_code_indexed(0xE4, 'ANDB', 4, __opcode_and, &p->B)
        op_code_indexed(0xE5, 'BITB', 4, __opcode_bit8, &p->B)
        op_code_indexed(0xE6, 'LDB', 4, __opcode_ldb)
        op_code_indexed(0xE7, 'STB', 4, __opcode_stb)
        op_code_indexed(0xE8, 'EORB', 4, __opcode_eor, &p->B)
        op_code_indexed(0xE9, 'ADCB', 4, __opcode_adc, &p->B)
        op_code_indexed(0xEA, 'ORB', 4, __opcode_or, &p->B)
        op_code_indexed(0xEB, 'ADDB', 4, __opcode_add8, &p->B)
        op_code_indexed(0xEC, 'LDD', 5, __opcode_ldd)
        op_code_indexed(0xED, 'STD', 5, __opcode_std)
        op_code_indexed(0xEE, 'LDU', 5, __opcode_ldu)
        op_code_indexed(0xEF, 'STU', 5, __opcode_stu)

        op_code_extended(0xF0, 'SUBB', 5, __opcode_sub8, &p->B, 0)
        op_code_extended(0xF1, 'CMPB', 5, __opcode_sub8, &p->B, 1)
        op_code_extended(0xF2, 'SBCB', 5, __opcode_sbc, &p->B)
        op_code_extended(0xF3, 'ADDD', 7, __opcode_add16, &p->D)
        op_code_extended(0xF4, 'ANDB', 5, __opcode_and, &p->B)
        op_code_extended(0xF5, 'BITB', 5, __opcode_bit8, &p->B)
        op_code_extended(0xF6, 'LDB', 5, __opcode_ldb)
        op_code_extended(0xF7, 'STB', 5, __opcode_stb)
        op_code_extended(0xF8, 'EORB', 5, __opcode_eor, &p->B)
        op_code_extended(0xF9, 'ADCB', 5, __opcode_adc, &p->B)
        op_code_extended(0xFA, 'ORB', 5, __opcode_or, &p->B)
        op_code_extended(0xFB, 'ADDB', 5, __opcode_add8, &p->B)
        op_code_extended(0xFC, 'LDD', 6, __opcode_ldd)
        op_code_extended(0xFD, 'STD', 6, __opcode_std)
        op_code_extended(0xFE, 'LDU', 6, __opcode_ldu)
        op_code_extended(0xFF, 'STU', 6, __opcode_stu)

        op_code_branch16(0x1021, 'LBRN', 5, 0)
        op_code_branch16(0x1022, 'LBHI', 5, p->Z == 0 && p->C == 0)
        op_code_branch16(0x1023, 'LBLS', 5, p->Z != 0 || p->C != 0)
        op_code_branch16(0x1024, 'LBHS', 5, p->C == 0)
        op_code_branch16(0x1025, 'LBLO', 5, p->C != 0)
        op_code_branch16(0x1026, 'LBNE', 5, p->Z == 0)
        op_code_branch16(0x1027, 'LBEQ', 5, p->Z != 0)
        op_code_branch16(0x1028, 'LBVC', 5, p->V == 0)
        op_code_branch16(0x1029, 'LBVS', 5, p->V != 0)
        op_code_branch16(0x102A, 'LBPL', 5, p->N == 0)
        op_code_branch16(0x102B, 'LBMI', 5, p->N != 0)
        op_code_branch16(0x102C, 'LBGE', 5, bit_value(p->N) == bit_value(p->V))
        op_code_branch16(0x102D, 'LBLT', 5, bit_value(p->N) != bit_value(p->V))
        op_code_branch16(0x102E, 'LBGT', 5, bit_value(p->N) == bit_value(p->V) && p->Z == 0)
        op_code_branch16(0x102F, 'LBLE', 5, (bit_value(p->N) != bit_value(p->V)) || p->Z != 0)

        op_code_immediate16(0x1083, 'CMPD', 5, __opcode_sub16, &p->D, 1)
        op_code_immediate16(0x108C, 'CMPY', 5, __opcode_sub16, &p->Y, 1)
        op_code_immediate16(0x108E, 'LDY', 4, __opcode_ldy)

        op_code_direct(0x1093, 'CMPD', 7, __opcode_sub16, &p->D, 1)
        op_code_direct(0x109C, 'CMPY', 7, __opcode_sub16, &p->Y, 1)
        op_code_direct(0x109E, 'LDY', 6, __opcode_ldy)
        op_code_direct(0x109F, 'STY', 6, __opcode_sty)

        op_code_indexed(0x10A3, 'CMPD', 7, __opcode_sub16, &p->D, 1)
        op_code_indexed(0x10AC, 'CMPY', 7, __opcode_sub16, &p->Y, 1)
        op_code_indexed(0x10AE, 'LDY', 6, __opcode_ldy)
        op_code_indexed(0x10AF, 'STY', 6, __opcode_sty)

        op_code_extended(0x10B3, 'CMPD', 8, __opcode_sub16, &p->D, 1)
        op_code_extended(0x10BC, 'CMPY', 8, __opcode_sub16, &p->Y, 1)
        op_code_extended(0x10BE, 'LDY', 7, __opcode_ldy)
        op_code_extended(0x10BF, 'STY', 7, __opcode_sty)
        op_code_immediate16(0x10CE, '', 5, __opcode_lds)

        op_code_direct(0x10DE, 'LDS', 6, __opcode_lds)
        op_code_direct(0x10DF, 'STS', 6, __opcode_sts)

        op_code_indexed(0x10EE, 'LDS', 6, __opcode_lds)
        op_code_indexed(0x10EF, 'STS', 6, __opcode_sts)

        op_code_extended(0x10FE, 'LDS', 8, __opcode_lds)
        op_code_extended(0x10FF, 'STS', 8, __opcode_sts)

        op_code_immediate16(0x1183, 'CMPU', 5, __opcode_sub16, &p->U, 1)
        op_code_immediate16(0x118C, 'CMPS', 5, __opcode_sub16, &p->S, 1)

        op_code_direct(0x1193, 'CMPU', 7, __opcode_sub16, &p->U, 1)
        op_code_direct(0x119C, 'CMPS', 7, __opcode_sub16, &p->S, 1)

        op_code_indexed(0x11A3, 'CMPU', 7, __opcode_sub16, &p->U, 1)
        op_code_indexed(0x11AC, 'CMPS', 7, __opcode_sub16, &p->S, 1)

        op_code_extended(0x11B3, 'CMPU', 8, __opcode_sub16, &p->U, 1)
        op_code_extended(0x11BC, 'CMPS', 8, __opcode_sub16, &p->S, 1)

        default:
            printf("Unknown OPCODE %04X\n", opcode);
            exit(1);
    }
}

void processor_next_opcode(struct processor_state *p) {
    if (p->_stopped) {
        add_cycles(1);
        return;
    }

    if (p->_nmi) {
        p->E = 1;
        p->S -= 2;
        processor_store_16(p, p->S, p->PC);
        p->S -= 2;
        processor_store_16(p, p->S, p->U);
        p->S -= 2;
        processor_store_16(p, p->S, p->Y);
        p->S -= 2;
        processor_store_16(p, p->S, p->X);
        p->S--;
        processor_store_8(p, p->S, p->DP);
        p->S--;
        processor_store_8(p, p->S, p->B);
        p->S--;
        processor_store_8(p, p->S, p->A);
        p->S--;
        processor_store_8(p, p->S, p->CC);

        p->F = 1;
        p->I = 1;
        p->PC = processor_load_16(p, 0xfffc);
        add_cycles(18);
    }
    if (p->_firq && !p->F) {
        p->E = 0;
        p->S -= 2;
        processor_store_16(p, p->S, p->PC);
        p->S--;
        processor_store_8(p, p->S, p->CC);
        p->I = 1;
        p->F = 1;
        p->PC = processor_load_16(p, 0xfff6);
        add_cycles(9);
    }
    if (p->_irq && !p->I) {
        p->E = 1;
        p->S -= 2;
        processor_store_16(p, p->S, p->PC);
        p->S -= 2;
        processor_store_16(p, p->S, p->U);
        p->S -= 2;
        processor_store_16(p, p->S, p->Y);
        p->S -= 2;
        processor_store_16(p, p->S, p->X);
        p->S--;
        processor_store_8(p, p->S, p->DP);
        p->S--;
        processor_store_8(p, p->S, p->B);
        p->S--;
        processor_store_8(p, p->S, p->A);
        p->S--;
        processor_store_8(p, p->S, p->CC);

        p->I = 1;
        p->PC = processor_load_16(p, 0xfff8);
        add_cycles(18);
    }

    uint16_t org_address = p->PC;
    uint16_t opcode = processor_load_8(p, p->PC++);

    while (opcode == 0x10 || opcode == 0x11) {
        uint16_t opcode_high = opcode << 8;
        opcode = processor_load_8(p, p->PC++);
        if (!(opcode == 0x10 || opcode == 0x11)) opcode = opcode_high + opcode;
    }

    // if (org_address >= 0xA1C1 && org_address <= 0xA1F9)
    //     printf("Execuding %04X opcode %04X\n", org_address, opcode);
    if (p->_dump_execution) printf("Execuding %04X opcode %04X\n", org_address, opcode);
    execute_opcode(p, opcode);
    if (p->_dump_execution) processor_dump(p);
    // if (org_address >= 0xA1C1 && org_address <= 0xA1F9) {
    //     processor_dump(p);
    //     if (org_address == 0xA1F9)
    //         exit(0);
    // }

    // if (org_address == 0xA765) {
    //     exit(0);
    // }

    // if (org_address == 0xB99C) {
    //     printf("Printing: {");
    //     for (int i=1; ; i++){
    //         uint8_t ch = processor_load_8(p, p->X+i);
    //         if (ch == 0xd) ch = '\n';
    //         printf("%c", ch);
    //         if (ch ==0 || ch == '"') break;
    //     }
    //     printf("}");
    //     processor_dump(p);
    // }

}
