#ifndef __MACHINE__
#define __MACHINE__

#include <SDL3/SDL.h>
#include "processor_6809.h"
#include "keyboard.h"
#include "video.h"
#include "adc.h"
#include "disk_drive.h"


struct machine_status {
    SDL_Window* window;
    SDL_Renderer* renderer;

    struct processor_state p;
    struct sam_status *sam;
    struct keyboard_status *keyboard;
    struct video_status *video;
    struct adc_status *adc;
    struct disk_drive_status *disk_drive;
    int cart_sense;

    uint64_t _next_disk_drive_call;  // tracks the disk timing
    uint64_t _next_keyboard_poll_ns;

    int settings_page_is_open;

    int _joy_emulation[2];    // enable/disable keyboard/mouse joystick emulation
    SDL_Joystick *joysticks[2];
    SDL_JoystickID joystick_ids[2];
};


void machine_init(struct machine_status *machine);
void machine_reset(struct machine_status *machine);
int machine_process_frame(struct machine_status *machine);
void machine_handle_input_begin(struct machine_status *machine);
int machine_handle_input(struct machine_status *machine, SDL_Event *event);
void machine_send_key(uint32_t key_code);
void keyboard_buffer_reset();

#endif
