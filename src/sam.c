#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bus.h"
#include "sam.h"


#define set_sam_bit(position, name) case position: data->name = is_set; break;

void _bus_sam_write(struct bus_adaptor *p, uint16_t addr, uint8_t value) {
    struct sam_status *data = SAM(p);

    int is_set = addr & 0x1;
    int bit_pos = addr >> 1;

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
}


struct bus_adaptor * bus_create_sam() {
    struct bus_adaptor *adaptor = malloc(sizeof(struct bus_adaptor));
    struct sam_status *data = malloc(sizeof(struct sam_status));
    memset(data, 0, sizeof(struct sam_status));

    adaptor->start = 0xFFC0;
    adaptor->end = 0xFFDF;
    adaptor->data = data;
    adaptor->load_8 = NULL;
    adaptor->store_8 = _bus_sam_write;

    printf("Created SAM start=%04X end=%04X\n", adaptor->start, adaptor->end);

    return adaptor;
}
