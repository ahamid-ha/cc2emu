#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SDL3/SDL_audio.h>
#include "adc.h"

void _adc_process(struct adc_status *adc) {
    int compare = 0;

    switch(adc->switch_selection) {
        case 0: compare = adc->adc_level < adc->input_joy_0; break;
        case 1: compare = adc->adc_level < adc->input_joy_1; break;
        case 2: compare = adc->adc_level < adc->input_joy_2; break;
        case 3: compare = adc->adc_level < adc->input_joy_3; break;
    }

    mc6821_peripheral_input(adc->pia1, 0, compare ? 0x80 : 0, 0x80);
}

void _adc_level_change_cb(struct mc6821_status *pia, int peripheral_address, uint8_t value, void *data) {
    struct adc_status *adc = (struct adc_status *)data;

    adc->adc_level = 0;
    if (value & 0b10000000) adc->adc_level += 2.25;
    if (value & 0b01000000) adc->adc_level += 1.125;
    if (value & 0b00100000) adc->adc_level += 0.563;
    if (value & 0b00010000) adc->adc_level += 0.281;
    if (value & 0b00001000) adc->adc_level += 0.14;
    if (value & 0b00000100) adc->adc_level += 0.07;

    _adc_process(adc);
}

void _adc_source_a_change_cb(struct mc6821_status *pia, int peripheral_address, uint8_t value, void *data) {
    struct adc_status *adc = (struct adc_status *)data;

    adc->switch_selection &= 0b10;
    adc->switch_selection |= value ? 1 : 0;

    _adc_process(adc);
}

void _adc_source_b_change_cb(struct mc6821_status *pia, int peripheral_address, uint8_t value, void *data) {
    struct adc_status *adc = (struct adc_status *)data;

    adc->switch_selection &= 0b1;
    adc->switch_selection |= value ? 0b10 : 0;

   _adc_process(adc);
}

void _adc_motor_cb(struct mc6821_status *pia, int peripheral_address, uint8_t value, void *data) {
    struct adc_status *adc = (struct adc_status *)data;

    adc->cassette_motor = value;
    if (value) printf("Cassette motor on\n");
    else printf("Cassette motor off\n");
}

void SDLCALL _adc_sound_sample_cb(void *data, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
    struct adc_status *adc = (struct adc_status *)data;
    if (additional_amount && adc->sound_samples_size) {
        if (adc->sound_samples_size > additional_amount) {
            SDL_PutAudioStreamData(stream, adc->sound_samples, additional_amount);
            adc->sound_samples_size -= additional_amount;
            memcpy(adc->sound_samples, adc->sound_samples + additional_amount, adc->sound_samples_size);
        } else {
            SDL_PutAudioStreamData(stream, adc->sound_samples, adc->sound_samples_size);
            adc->sound_samples_size = 0;
        }
    }
}

void _initialize_audio(struct adc_status *adc) {
    if (!adc->stream) {
        SDL_AudioSpec spec = {
            .format = SDL_AUDIO_U8,
            .channels = 1,
            .freq = 44100
        };
        adc->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, _adc_sound_sample_cb, adc);
        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(adc->stream));
    }
    adc->sound_samples_size = 0;
    adc->next_sound_sample_time_ns = 0;
}

void _adc_sound_cb(struct mc6821_status *pia, int peripheral_address, uint8_t value, void *data) {
    struct adc_status *adc = (struct adc_status *)data;

    adc->sound_enabled = value;
    if (value) {
        _initialize_audio(adc);
    }
    else {
        SDL_FlushAudioStream(adc->stream);
    }
}

void adc_reset(struct adc_status *adc) {
    if (adc->cassette_audio_buf) {
        SDL_free(adc->cassette_audio_buf);
        adc->cassette_audio_buf = NULL;
    }

    adc->input_joy_0 = 2.5;
    adc->input_joy_1 = 2.5;
    adc->input_joy_2 = 2.5;
    adc->input_joy_3 = 2.5;

    adc->adc_level = 0;
    adc->switch_selection = 0;

    adc->sound_enabled = 0;
    adc->sound_samples_size = 0;
    adc->next_sound_sample_time_ns = 0;

    adc->cassette_motor = 0;
    adc->cassette_audio_len = 0;
    adc->cassette_audio_location = 0;
    adc->next_cassette_sample_time_ns = 0;
}

struct adc_status *adc_initialize(struct mc6821_status *pia1, struct mc6821_status *pia2) {
    struct adc_status *adc=malloc(sizeof(struct adc_status));
    memset(adc, 0, sizeof(struct adc_status));
    adc->pia1 = pia1;
    adc->pia2 = pia2;

    adc_reset(adc);

    mc6821_register_c2_cb(pia1, 0, (mc6821_cb)_adc_source_a_change_cb, adc);
    mc6821_register_c2_cb(pia1, 1, (mc6821_cb)_adc_source_b_change_cb, adc);
    mc6821_register_c2_cb(pia2, 0, (mc6821_cb)_adc_motor_cb, adc);
    mc6821_register_c2_cb(pia2, 1, (mc6821_cb)_adc_sound_cb, adc);
    mc6821_register_cb(pia2, 0, (mc6821_cb)_adc_level_change_cb, adc);

    return adc;
}

int adc_load_cassette(struct adc_status *adc, const char *path) {
    SDL_AudioSpec spec;

    if (adc->cassette_audio_buf) {
        SDL_free(adc->cassette_audio_buf);
        adc->cassette_audio_buf = NULL;
    }

    if (!path) {
        return 0;
    }

    if (!SDL_LoadWAV(path, &spec, &adc->cassette_audio_buf, &adc->cassette_audio_len)) {
        printf("Error cassette loading: %s: %s\n", path, SDL_GetError());
        return 1;
    }
    adc->next_cassette_sample_time_ns = 0;
    adc->cassette_audio_location = 0;

    if (spec.format != SDL_AUDIO_U8 || spec.channels != 1 || spec.freq != 9600) {
        printf("Converting from format=%04X, channels=%d, freq=%d\n", spec.format, spec.channels, spec.freq);

        SDL_AudioSpec dest_spec = {
            .channels  = 1,
            .format  = SDL_AUDIO_U8,
            .freq = 9600
        };
        Uint32 dest_audio_len;
        Uint8 *dest_audio_buf;

        if (!SDL_ConvertAudioSamples(&spec, adc->cassette_audio_buf, (int)adc->cassette_audio_len, &dest_spec, &dest_audio_buf, (int*)&dest_audio_len)) {
            printf("Format converting failed: %s\n", SDL_GetError());
            SDL_free(adc->cassette_audio_buf);
            adc->cassette_audio_buf = NULL;
            return 1;
        }
        SDL_free(adc->cassette_audio_buf);
        adc->cassette_audio_buf = dest_audio_buf;
        adc->cassette_audio_len = dest_audio_len;
    }
    printf("Loaded wav file %s %d\n", path, adc->cassette_audio_len);
    return 0;
}

#define CASSETTE_SAMPLE_NS 104170
#define SOUND_SAMPLE_NS 22675
void adc_process(struct adc_status *adc, uint64_t virtual_time_ns) {
    if (!adc->next_cassette_sample_time_ns) {
        adc->next_cassette_sample_time_ns = virtual_time_ns + CASSETTE_SAMPLE_NS;
    }

    if (virtual_time_ns >= adc->next_cassette_sample_time_ns) {
        if (adc->cassette_audio_location < adc->cassette_audio_len &&
                adc->cassette_audio_buf && adc->cassette_motor) {
            if (adc->cassette_audio_buf[adc->cassette_audio_location] > 127)
                mc6821_peripheral_input(adc->pia2, 0, 0, 1);
            else
                mc6821_peripheral_input(adc->pia2, 0, 1, 1);
            adc->cassette_audio_location++;
        }
        adc->next_cassette_sample_time_ns += CASSETTE_SAMPLE_NS;
    }

    if (!adc->next_sound_sample_time_ns) {
        adc->next_sound_sample_time_ns = virtual_time_ns + SOUND_SAMPLE_NS;
    }

    if (virtual_time_ns > adc->next_sound_sample_time_ns) {
        if (adc->sound_enabled) {
            uint8_t snd;
            switch (adc->switch_selection) {
                // just passthrough cassette noise
                case 0: snd = (int)(adc->adc_level * 255 / 5); break;
                case 1: if (adc->cassette_audio_location < adc->cassette_audio_len) snd = adc->cassette_audio_buf[adc->cassette_audio_location]; break;
                default: snd = 0;
            }
            if (adc->sound_samples_size < SOUND_BUFFER_SIZE) {
                adc->sound_samples[adc->sound_samples_size++] = snd;
            } else {
                printf("Sound buffer overflow\n");
            }
        }
        adc->next_sound_sample_time_ns += SOUND_SAMPLE_NS;
    }
}
