#include <SDL3/SDL.h>
#include <inttypes.h>
#include "mc6821.h"
#include "sam.h"

struct video_status {
    struct sam_status *sam;

    union {
        struct {
            unsigned css:1;
            unsigned graphics_mode:3;
            unsigned enable_graphics:1;
        };
        uint8_t vdg_op_mode;
    };
    SDL_Renderer* renderer;
    SDL_Texture* texture;

    int _h_time_ns;   // to track the time of current HS
    int signal_fs;    // field sync
    int h_sync;
    int field_row_number;
    int _char_row_number;
    int _x;
    uint32_t* _pixels;
    int _pitch;
};

struct video_status *video_initialize(struct sam_status *sam, struct mc6821_status *pia, SDL_Renderer* renderer);
void video_reset(struct video_status *v);

uint64_t video_start_field(struct video_status *v);
void video_end_field(struct video_status *v);
uint64_t video_process_next(struct video_status *v);
