#include "audio.h"

#include "../psrc_aux/logging.h"

#include <stdint.h>

static inline void mixsound(struct audiostate* a, int samples) {
    if (a->audbuflen != samples) {
        a->audbuflen = samples;
        a->audbuf[0] = realloc(a->audbuf[0], samples * sizeof(*a->audbuf[0]));
        a->audbuf16[0] = realloc(a->audbuf16[0], samples * sizeof(*a->audbuf16[0]));
        if (a->channels == 2) {
            a->audbuf[1] = realloc(a->audbuf[1], samples * sizeof(*a->audbuf[1]));
            a->audbuf16[1] = realloc(a->audbuf16[1], samples * sizeof(*a->audbuf16[1]));
        }
    }
    memset(a->audbuf[0], 0, samples * sizeof(*a->audbuf[0]));
    memset(a->audbuf16[0], 0, samples * sizeof(*a->audbuf16[0]));
    if (a->channels == 2) {
        memset(a->audbuf[1], 0, samples * sizeof(*a->audbuf[1]));
        memset(a->audbuf16[1], 0, samples * sizeof(*a->audbuf16[1]));
    }
    for (int i = 0; i < a->sounds; ++i) {
        struct audiosound* sound = &a->sounddata[i];
    }
}

static void wrsamples(struct audiostate* a, int16_t* stream, int channels, int len) {
    int samples = len / channels;
    //plog(LL_PLAIN, "Asked for %d samples", samples);
    lockMutex(&a->lock);
    mixsound(a, samples);
    for (int c = 0; c < channels; ++c) {
        for (int i = c; i < len; i += channels) {
            int sample = (int)tmp1 * (len - 1 - i) / len + (int)tmp2 * i / len;
            stream[i] = sample;
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
        plog(LL_INFO, "Audio info (device id %d):", (int)output);
        plog(LL_INFO, "  Frequency: %d", outspec.freq);
        plog(LL_INFO, "  Channels: %d (%s)", outspec.channels, (outspec.channels == 1) ? "mono" : "stereo");
        plog(LL_INFO, "  Samples: %d", (int)outspec.samples);
        a->freq = outspec.freq;
        a->channels = (outspec.channels == 1) ? 1 : 2;
        SDL_PauseAudioDevice(output, 0);
    } else {
        plog(LL_WARN, "Failed to get audio info for default output device; audio disabled: %s", SDL_GetError());
        a->valid = false;
        return true;
    }
    a->valid = true;
    return true;
}
