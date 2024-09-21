#include <inttypes.h>
#include "mc6821.h"


#define SOUND_BUFFER_SIZE 40000


struct adc_status {
    float adc_level;
    uint8_t switch_selection;

    float input_joy_0;
    float input_joy_1;
    float input_joy_2;
    float input_joy_3;

    struct mc6821_status *pia1;
    struct mc6821_status *pia2;

    uint8_t sound_enabled;  // 0: off, 1: on
    uint8_t sound_samples[SOUND_BUFFER_SIZE];
    int sound_samples_size;
    uint64_t next_sound_sample_time_ns;
    SDL_AudioStream *stream;

    uint8_t cassette_motor;  // 0: off, 1: on
    uint32_t cassette_audio_len;
    uint32_t cassette_audio_location;
    uint8_t *cassette_audio_buf;
    uint64_t next_cassette_sample_time_ns;
};

struct adc_status *adc_initialize(struct mc6821_status *pia1, struct mc6821_status *pia2);
void adc_load_cassette(struct adc_status *adc, const char *path);
void adc_process(struct adc_status *adc, uint64_t virtual_time_ns);
