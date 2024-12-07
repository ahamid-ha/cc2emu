#include <SDL3/SDL.h>
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT

#include "external/nuklear/nuklear.h"

struct nk_context *nk_sdl_init(SDL_Window *win, SDL_Renderer *renderer);
int nk_sdl_handle_event(SDL_Event *evt);
void nk_sdl_render(enum nk_anti_aliasing);
void nk_sdl_update_renderer(SDL_Window *win, SDL_Renderer *renderer);
