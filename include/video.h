#include <SDL2/SDL.h>
#include <inttypes.h>
#include "mc6821.h"
#include "sam.h"

struct video_status {
    struct sam_status *sam;
    uint8_t *memory;
    uint8_t mode;
    SDL_Renderer* renderer;
    SDL_Texture* texture;

    int mode_change_count;
};

struct video_status *video_initialize(struct sam_status *sam, struct mc6821_status *pia, uint8_t *memory, SDL_Renderer* renderer);
void video_render(struct video_status *v);
