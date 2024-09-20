#include "video.h"
#include "mc6821.h"
#include "sam.h"
#include <stdlib.h>
#include <stdio.h>

#define COLOR_GREEN 0x1cd510ff
#define COLOR_YELLOW 0xe2db0fff
#define COLOR_BLUE 0x0320ffff
#define COLOR_RED 0xe2200aff
#define COLOR_BUFF 0xcddbe0ff
#define COLOR_CYAN 0x16d0e2ff
#define COLOR_MAGENTA 0xcb39e2ff
#define COLOR_ORANGE 0xffbb44ff
#define COLOR_BLACK 0x101010ff
#define COLOR_DARK_GREEN 0x003400ff
#define COLOR_DARK_ORANGE 0x321400ff

uint32_t _text_render_colors[] = {
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_BLUE,
    COLOR_RED,
    COLOR_BUFF,
    COLOR_CYAN,
    COLOR_MAGENTA,
    COLOR_ORANGE
};


const unsigned char epd_bitmap_mc6847charset [] = {
	0x0e, 0x11, 0x10, 0x16, 0x15, 0x15, 0x0e, 0x04, 0x0a, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x0f, 0x12, 
	0x12, 0x0e, 0x12, 0x12, 0x0f, 0x0e, 0x11, 0x01, 0x01, 0x01, 0x11, 0x0e, 0x0f, 0x12, 0x12, 0x12, 
	0x12, 0x12, 0x0f, 0x1f, 0x01, 0x01, 0x0f, 0x01, 0x01, 0x1f, 0x1f, 0x01, 0x01, 0x0f, 0x01, 0x01, 
	0x01, 0x1e, 0x01, 0x01, 0x19, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11, 0x0e, 
	0x04, 0x04, 0x04, 0x04, 0x04, 0x0e, 0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x0e, 0x11, 0x09, 0x05, 
	0x03, 0x05, 0x09, 0x11, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1f, 0x11, 0x1b, 0x15, 0x15, 0x11, 
	0x11, 0x11, 0x11, 0x13, 0x15, 0x19, 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1f, 
	0x0f, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x01, 0x0e, 0x11, 0x11, 0x11, 0x15, 0x09, 0x16, 0x0f, 0x11, 
	0x11, 0x0f, 0x05, 0x09, 0x11, 0x0e, 0x11, 0x02, 0x04, 0x08, 0x11, 0x0e, 0x1f, 0x04, 0x04, 0x04, 
	0x04, 0x04, 0x04, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x11, 0x0a, 0x0a, 0x04, 
	0x04, 0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11, 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11, 0x11, 
	0x11, 0x0a, 0x04, 0x04, 0x04, 0x04, 0x1f, 0x10, 0x08, 0x04, 0x02, 0x01, 0x1f, 0x07, 0x01, 0x01, 
	0x01, 0x01, 0x01, 0x07, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10, 0x1c, 0x10, 0x10, 0x10, 0x10, 
	0x10, 0x1c, 0x04, 0x0e, 0x15, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x02, 0x1f, 0x02, 0x04, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x0a, 0x0a, 
	0x0a, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x0a, 0x1b, 0x00, 0x1b, 0x0a, 0x0a, 0x04, 0x1e, 0x01, 0x0e, 
	0x10, 0x0f, 0x04, 0x13, 0x13, 0x08, 0x04, 0x02, 0x19, 0x19, 0x02, 0x05, 0x05, 0x02, 0x15, 0x09, 
	0x16, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x04, 0x02, 0x01, 0x01, 0x01, 0x02, 0x04, 0x04, 
	0x08, 0x10, 0x10, 0x10, 0x08, 0x04, 0x00, 0x04, 0x0e, 0x1f, 0x0e, 0x04, 0x00, 0x00, 0x04, 0x04, 
	0x1f, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x02, 0x01, 0x00, 0x00, 0x00, 0x1f, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x10, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01, 
	0x06, 0x09, 0x09, 0x09, 0x09, 0x09, 0x06, 0x04, 0x06, 0x04, 0x04, 0x04, 0x04, 0x0e, 0x0e, 0x11, 
	0x10, 0x0e, 0x01, 0x01, 0x1f, 0x0e, 0x11, 0x10, 0x0c, 0x10, 0x11, 0x0e, 0x08, 0x0c, 0x0a, 0x1f, 
	0x08, 0x08, 0x08, 0x1f, 0x01, 0x0f, 0x10, 0x10, 0x11, 0x0e, 0x0e, 0x01, 0x01, 0x0f, 0x11, 0x11, 
	0x0e, 0x1f, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01, 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e, 0x0e, 
	0x11, 0x11, 0x1e, 0x10, 0x10, 0x0e, 0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00, 
	0x06, 0x06, 0x04, 0x02, 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08, 0x00, 0x00, 0x1f, 0x00, 0x1f, 
	0x00, 0x00, 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02, 0x06, 0x09, 0x08, 0x04, 0x04, 0x00, 0x04
};

#define draw_pixel(x, y, c) v->_pixels[(x) + ((y) * (v->_pitch))] = c
#define CLK_CYCLE_NS 279
#define H_HS_START_NS 2400
#define H_HS_END_NS (H_HS_START_NS + (16 * CLK_CYCLE_NS) + (CLK_CYCLE_NS >> 1))
#define H_LEFT_BORDER_START (H_HS_START_NS + 5100)
#define H_AV_START (H_LEFT_BORDER_START + (29 * CLK_CYCLE_NS))
#define H_AV_END (H_AV_START + (CLK_CYCLE_NS * 128))
#define H_SCAN_TIME_NS (228 * CLK_CYCLE_NS)

uint8_t old_sam=0;
uint8_t old_mode=0;

uint64_t video_start_field(struct video_status *v) {
    v->field_row_number = 0;
    v->_h_time_ns = H_HS_START_NS;
    v->signal_fs = 1;

    if(!SDL_LockTexture( v->texture, NULL, (void**)&v->_pixels, &v->_pitch )) {
        printf("SDL_LockTexture failed\n");
        exit(1);
    }
    v->_pitch = v->_pitch >> 2;

    return H_HS_START_NS;
}

void video_end_field(struct video_status *v) {
    SDL_FRect dest = {
        .h = 192 * 4,
        .w = 256 * 4,
        .x = 20,
        .y = 20,
    };

    SDL_UnlockTexture(v->texture);
    SDL_RenderTexture(v->renderer, v->texture, NULL, &dest);
}

#define fs_start 13 + 25 + 192
#define fs_end fs_start + 32
uint64_t video_process_next(struct video_status *v) {
    if (v->_h_time_ns == H_HS_START_NS) {
        if(v->h_sync) {
            sam_vdg_hs_reset(v->sam);
            v->_x = 0;
            if (v->field_row_number >= 13 + 25 && v->field_row_number < 13 + 25 + 192) {
                // increment the char line number only during active area
                v->_char_row_number++;
                if (v->_char_row_number > 11) v->_char_row_number = 0;
            }
        }
        v->h_sync = 0;
        v->_h_time_ns = H_HS_END_NS;
        return H_HS_END_NS - H_HS_START_NS;
    }
    v->h_sync = 1;
    if (v->_h_time_ns == H_HS_END_NS) {
        v->_h_time_ns = H_AV_START;
        return H_AV_START - H_HS_END_NS;
    }
    if (v->_h_time_ns >= H_AV_END) {
        int old_h_time_ns = v->_h_time_ns;
        v->_h_time_ns = H_HS_START_NS;

        if (v->field_row_number == 13 + 25 + 192) {
            sam_vdg_fs_reset(v->sam);
            v->_char_row_number = -1;
        }
        if (v->field_row_number == fs_start) {
            v->signal_fs = 0;
        }
        if (v->field_row_number == fs_end || v->field_row_number < fs_start) {
            v->signal_fs = 1;
        }

        v->field_row_number++;
        if (v->field_row_number > 13 + 25 + 192 + 32) {
            return 0;
        }
        return H_SCAN_TIME_NS - old_h_time_ns + H_HS_START_NS;
    }

    if (v->field_row_number >= 13 + 25 && v->field_row_number < 13 + 25 + 192) {
        // active area 192 lines
        long byte_time;
        int pixels;
        int y = v->field_row_number - (13 + 25);
        uint8_t data = v->memory[sam_get_vdg_address(v->sam)];
        sam_vdg_increment(v->sam);
        if (v->enable_graphics) {
            // graphics
            switch(v->graphics_mode) {
                case 0:
                    byte_time = CLK_CYCLE_NS * 4 * 4 / 2;
                    pixels = 4;
                    break;
                case 1:
                    byte_time = CLK_CYCLE_NS * 8 * 3 / 2;
                    pixels = 3;
                    break;
                case 2:
                    byte_time = CLK_CYCLE_NS * 4 * 3 / 2;
                    pixels = 3;
                    break;
                case 3:
                    byte_time = CLK_CYCLE_NS * 8 * 2 / 2;
                    pixels = 2;
                    break;
                case 4:
                    byte_time = CLK_CYCLE_NS * 4 * 2 / 2;
                    pixels = 2;
                    break;
                case 5:
                    byte_time = CLK_CYCLE_NS * 8 * 2 / 2;
                    pixels = 2;
                    break;
                case 6:
                    byte_time = CLK_CYCLE_NS * 4 * 2 / 2;
                    pixels = 2;
                    break;
                case 7:
                    byte_time = CLK_CYCLE_NS * 8 * 1 / 2;
                    pixels = 1;
                    break;
            }
            int is_colors = ((v->graphics_mode & 1) == 0);
            if (is_colors) {
                for (int bit_pos=0; bit_pos < 8; bit_pos+=2) {
                    int color_index = (data & 0b11000000) >> 6 | (v->css ? 0b100 : 0);
                    uint32_t color = _text_render_colors[color_index];
                    data = data << 2;
                    for(int i=0; i < pixels; i++) {
                        draw_pixel(v->_x++, y, color);
                    }
                }
            } else {
                for (int bit_pos=0; bit_pos < 8; bit_pos++) {
                    int color_index = data & 0b10000000;
                    uint32_t color = COLOR_BLACK;
                    if (color_index) {
                        if (v->css) color = COLOR_BUFF;
                        else color = COLOR_GREEN;
                    }
                    data = data << 1;
                    for(int i=0; i < pixels; i++) {
                        draw_pixel(v->_x++, y, color);
                    }
                }

            }
        } else {
            // text
            byte_time = CLK_CYCLE_NS * 8 / 2;
            int sg6 = v->graphics_mode & 0b1;
            int sg4 = !sg6;
            uint32_t text_background = v->css ? COLOR_DARK_ORANGE : COLOR_DARK_GREEN;
            uint32_t text_foreground = v->css ? COLOR_ORANGE : COLOR_GREEN;

            if (data < 128 && sg4) {
                uint8_t inverted = 0xff;
                if (data >= 64) {
                    inverted = 0;
                }
                data = data & 63;

                uint8_t mask = 0;
                if (v->_char_row_number >= 3 && v->_char_row_number < 10) mask = epd_bitmap_mc6847charset[data * 7 + v->_char_row_number - 3] << 2;
                mask = mask ^ inverted;
                for (int bit = 0; bit < 8; bit++, v->_x++) {
                    if ( (1 << bit) & mask) {
                        draw_pixel(v->_x, y, text_background);
                    } else {
                        draw_pixel(v->_x, y, text_foreground);
                    }
                }
            } else {
                uint8_t color;
                uint8_t columns;
                if (sg4) {
                    color = (data & 0b1110000) >> 4;
                    uint8_t row = v->_char_row_number >= 6 ? 1 : 0;  // which half of the character, top or bottom
                    columns = data >> ((1 - row) * 2);
                } else {
                    color = (data & 0b11000000) >> 6;
                    if (v->css) color |= 0b100;
                    uint8_t row = v->_char_row_number >> 2;  // divide by 4 to get the row number, as each cell is 4 rows high
                    columns = data >> ((2 - row) * 2);
                }
                for (int rec_x = 0; rec_x < 4; rec_x++) {
                    if (columns & 0b10) {
                        draw_pixel(v->_x + rec_x, y, _text_render_colors[color]);
                    } else {
                        draw_pixel(v->_x + rec_x, y, COLOR_BLACK);
                    }
                    if (columns & 0b01) {
                        draw_pixel(v->_x + rec_x + 4, y, _text_render_colors[color]);
                    } else {
                        draw_pixel(v->_x + rec_x + 4, y, COLOR_BLACK);
                    }
                }

                v->_x += 8;
            }
        }

        v->_h_time_ns += byte_time;
        return byte_time;
    }

    // vertical blanking 13 H lines
    // top border 25 H lines
    // bottom border 26 lines
    // vertical retrace 6 h lines

    v->_h_time_ns = H_AV_END;
    return H_AV_END - H_AV_START;
}

void _video_mode_change_cb(struct mc6821_status *pia, int peripheral_address, uint8_t value, void *data) {
    struct video_status *v = (struct video_status *)data;
    v->vdg_op_mode = value >> 3;
    v->mode_change_count++;
}

struct video_status *video_initialize(struct sam_status *sam, struct mc6821_status *pia, uint8_t *memory, SDL_Renderer* renderer) {
    struct video_status *v=malloc(sizeof(struct video_status));
    memset(v, 0, sizeof(struct video_status));
    v->memory = memory;
    v->vdg_op_mode = 0;
    v->sam = sam;

    v->signal_fs = 1;
    v->h_sync = 1;

    v->renderer = renderer;
    v->texture = SDL_CreateTexture( renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 256, 192 );

    mc6821_register_cb(pia, 1, (mc6821_cb)_video_mode_change_cb, v);

    return v;
}
