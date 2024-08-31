#include "video.h"
#include "mc6821.h"
#include "sam.h"

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

#define draw_pixel(x, y, c) pixels[(x) + ((y) * (pitch))] = c

void render_graphics(struct video_status *v) {
    uint16_t start_pos = v->sam->F << 9;
    uint8_t *memory = v->memory;

    uint8_t css = (v->mode & 0x1) << 2;
    int color_bits_shift = v->mode & 0b10 ? 1 : 2;
    uint8_t color_bits_mask = v->mode & 0b10 ? 0b10000000 : 0b11000000;
    uint8_t mode = (v->mode >> 1) & 7;
    int w,h;

    switch(mode) {
        case 0:
            w = 64;
            h = 64;
            break;
        case 1:
        case 2:
            w = 128;
            h = 64;
            break;
        case 3:
        case 4:
            w = 128;
            h = 96;
            break;
        case 5:
        case 6:
            w = 128;
            h = 192;
            break;
        case 7:
            w = 256;
            h = 192;
            break;
    }

    uint32_t* pixels;
    int pitch;

    SDL_LockTexture( v->texture, NULL, (void**)&pixels, &pitch );
    pitch = pitch >> 2;

    for (int y=0; y<h; y++) {
        for (int x=0; x<w; ) {
            uint8_t data = memory[start_pos];
            for (int bit_pos=0; bit_pos < 8; bit_pos+=color_bits_shift) {
                int color_index = (data & color_bits_mask) >> (8 - color_bits_shift) | css;

                uint32_t color;
                if (color_bits_shift == 1) {
                    // two color
                    if (color_index & 1) {
                        color = _text_render_colors[color_index - 1];
                    } else {
                        color = COLOR_BLACK;
                    }
                } else {
                    color = _text_render_colors[color_index];
                }
                draw_pixel(x, y, color);

                x += 1;
                data = data << color_bits_shift;
            }
            start_pos++;
        }
    }

    SDL_Rect src = {
        .h = h,
        .w = w,
        .x = 0,
        .y = 0,
    };
    SDL_Rect dest = {
        .h = 192 * 4,
        .w = 256 * 4,
        .x = 20,
        .y = 20,
    };

    SDL_UnlockTexture(v->texture);
    SDL_RenderCopy(v->renderer, v->texture, &src, &dest);
}

void render_text(struct video_status *v) {
    uint16_t start_pos = v->sam->F << 9;
    uint8_t *memory = v->memory;

    uint32_t text_background = v->mode & 0x1 ? COLOR_DARK_ORANGE : COLOR_DARK_GREEN;
    uint32_t text_foreground = v->mode & 0x1 ? COLOR_ORANGE : COLOR_GREEN;
    int color_bits_shift = v->mode & 0b10 ? 5 : 4;
    int box_lines = v->mode & 0b10 ? 4 : 6;

    uint32_t* pixels;
    int pitch;

    SDL_LockTexture( v->texture, NULL, (void**)&pixels, &pitch );
    pitch = pitch >> 2;

    for (int line=0; line < 16; line++) {
        for (int col=0; col < 32; col++) {
            uint8_t data = memory[start_pos + (line * 32) + col];

            if (data < 128) {
                uint8_t inverted = 0xff;
                if (data >= 64) {
                    inverted = 0;
                }
                data = data & 63;
                for (int row = 0; row < 12; row++) {
                    uint8_t mask = 0;
                    if (row >= 3 && row < 10) mask = epd_bitmap_mc6847charset[data * 7 + row - 3] << 2;
                    mask = mask ^ inverted;
                    for (int bit = 0; bit < 8; bit++) {
                        if ( (1 << bit) & mask) {
                            draw_pixel(col * 8 + bit, line * 12 + row, text_background);
                        } else {
                            draw_pixel(col * 8 + bit, line * 12 + row, text_foreground);
                        }
                    }
                }
            } else {
                uint8_t color = (data >> color_bits_shift) & 0b111;
                if ((v->mode & 0x1) && color_bits_shift == 2) color |= 0b100;

                for (int box_row = 12 - box_lines; box_row >= 0;) {
                    for (int rec_x = 0; rec_x < 4; rec_x++) {
                        for (int rec_y = 0; rec_y < box_lines; rec_y++) {
                            if (data & 0b10) {
                                draw_pixel(col * 8 + rec_x, line * 12 + rec_y + box_row, _text_render_colors[color]);
                            } else {
                                draw_pixel(col * 8 + rec_x, line * 12 + rec_y + box_row, COLOR_BLACK);
                            }
                            if (data & 0b01) {
                                draw_pixel(col * 8 + rec_x + 4, line * 12 + rec_y + box_row, _text_render_colors[color]);
                            } else {
                                draw_pixel(col * 8 + rec_x + 4, line * 12 + rec_y + box_row, COLOR_BLACK);
                            }
                        }
                    }
                    box_row -= box_lines;
                    data = data >> 2;
                }
            }
        }
    }

    SDL_Rect dest = {
        .h = 192 * 4,
        .w = 256 * 4,
        .x = 20,
        .y = 20,
    };

    SDL_UnlockTexture(v->texture);
    SDL_RenderCopy(v->renderer, v->texture, NULL, &dest);
    // SDL_RenderCopy(v->renderer, v->texture, NULL, NULL);
}

void video_render(struct video_status *v) {
    if (v->mode & 0b10000) {
        render_graphics(v);
    } else {
        render_text(v);
    }
}

void _video_mode_change_cb(struct mc6821_status *pia, int peripheral_address, uint8_t value, void *data) {
    struct video_status *v = (struct video_status *)data;
    v->mode = value >> 3;
}

struct video_status *video_initialize(struct sam_status *sam, struct mc6821_status *pia, uint8_t *memory, SDL_Renderer* renderer) {
    struct video_status *v=malloc(sizeof(struct video_status));
    v->memory = memory;
    v->mode = 0;
    v->sam = sam;

    v->renderer = renderer;
    v->texture = SDL_CreateTexture( renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 256, 192 );

    mc6821_register_cb(pia, 1, (mc6821_cb)_video_mode_change_cb, v);

    return v;
}
