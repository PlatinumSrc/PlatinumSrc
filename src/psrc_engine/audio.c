#include "audio.h"

#include "../psrc_aux/logging.h"

#include <stdint.h>

static inline void mixsounds(struct audiostate* a, int samples, bool stereo) {
    
}

static void wrsamples(struct audiostate* a, int16_t* stream, int channels, int len) {
    int samples = len / channels;
    //plog(LL_PLAIN, "Asked for %d samples", samples);
    lockMutex(&a->lock);
    mixsounds(a, samples, (channels > 2));
    unlockMutex(&a->lock);
    for (int c = 0; c < channels && c < 2; ++c) {
        for (int i = c; i < len; i += channels) {
            //int sample = (int)tmp1 * (len - 1 - i) / len + (int)tmp2 * i / len;
            //stream[i] = sample;
        }
    }
}

static void callback(void* data, uint8_t* stream, int len) {
    memset(stream, 0, len);
    int channels = ((struct audiostate*)data)->channels;
    wrsamples(data, (int16_t*)stream, channels, len / sizeof(int16_t));
}

bool initAudio(struct audiostate* a) {
    memset(a, 0, sizeof(*a));
    if (!createMutex(&a->lock)) return false;
    if (SDL_Init(SDL_INIT_AUDIO)) {
        plog(LL_CRIT, "Failed to init audio: %s", SDL_GetError());
        return false;
    }
    return true;
}

bool startAudio(struct audiostate* a) {
    SDL_AudioSpec inspec;
    SDL_AudioSpec outspec;
    inspec.freq = 44100;
    inspec.format = AUDIO_S16SYS;
    inspec.channels = 2;
    inspec.samples = 1024;
    inspec.callback = callback;
    inspec.userdata = a;
    SDL_AudioDeviceID output = SDL_OpenAudioDevice(
        NULL, false, &inspec, &outspec,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE
    );
    if (output > 0) {
        if (outspec.channels > 2 || outspec.channels < 1) {
            plog(LL_WARN, "Invalid number of channels: %d", outspec.channels);
        }
        plog(LL_INFO, "Audio info (device id %d):", (int)output);
        plog(LL_INFO, "  Frequency: %d", outspec.freq);
        plog(LL_INFO, "  Channels: %d (%s)", outspec.channels, (outspec.channels == 1) ? "mono" : "stereo");
        plog(LL_INFO, "  Samples: %d", (int)outspec.samples);
        a->freq = outspec.freq;
        a->channels = outspec.channels;
        SDL_PauseAudioDevice(output, 0);
    } else {
        plog(LL_ERROR, "Failed to get audio info for default output device; audio disabled: %s", SDL_GetError());
        a->valid = false;
        return true;
    }
    a->valid = true;
    return true;
}
