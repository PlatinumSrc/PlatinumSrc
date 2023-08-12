#include "audio.h"

#include "../psrc_aux/logging.h"

#include <stdint.h>

static void callback(struct audiostate* a, uint8_t* stream, int l) {
    //printf("Asked for %d samples\n", l / 2);
    memset(stream, 0, l);
}

bool initAudio(struct audiostate* a) {
    memset(a, 0, sizeof(*a));
    if (SDL_Init(SDL_INIT_AUDIO)) {
        plog(LL_CRIT, "Failed to init audio: %s", SDL_GetError());
        return false;
    }
    return true;
}

bool startAudio(struct audiostate* a) {
    SDL_AudioSpec spec;
    spec.freq = 44100;
    spec.format = AUDIO_S16;
    spec.channels = 2;
    spec.samples = 1024;
    spec.callback = (SDL_AudioCallback)callback;
    spec.userdata = a;
    a->output = SDL_OpenAudioDevice(
        NULL, false, &spec, &a->outspec,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE
    );
    if (a->output) {
        plog(LL_INFO, "Audio info:");
        plog(LL_INFO, "  Frequency: %d", a->outspec.freq);
        plog(LL_INFO, "  Channels: %d (%s)", a->outspec.channels, (a->outspec.channels == 1) ? "mono" : "stereo");
        plog(LL_INFO, "  Samples: %d", (int)a->outspec.samples);
        SDL_PauseAudioDevice(a->output, 0);
    } else {
        plog(LL_WARN, "Failed to get audio info for default output device; audio disabled: %s", SDL_GetError());
        a->valid = false;
        return true;
    }
    a->valid = true;
    return true;
}
