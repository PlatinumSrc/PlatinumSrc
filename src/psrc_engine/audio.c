#include "audio.h"

#include "../psrc_aux/logging.h"

#include <stdint.h>

static inline void getvorbisat(struct audiosound* s, int pos, bool stereo, int16_t* out_l, int16_t* out_r) {
    if (pos < 0 || pos >= s->rc->len) {
        *out_l = 0;
        if (stereo) *out_r = 0;
    }
    struct audiosound_vorbisbuf vb = s->vorbisbuf;
    if (pos >= vb.off + vb.len) {
        do {
            vb.off += vb.len;
        } while (pos >= vb.off + vb.len);
        int len = s->rc->len;
        if (vb.off + vb.len >= len) {
            vb.off = len - vb.len;
            if (vb.off < 0) vb.off = 0;
        }
        // TODO: seek to vb.off
        // TODO: decode samples into buffer
        // TODO: zero unset part of buffer
    } else if (pos < vb.off) {
        do {
            vb.off -= vb.len;
        } while (vb.off > 0 && pos < vb.off);
        if (vb.off < 0) vb.off = 0;
        // TODO: seek to vb.off
        // TODO: decode samples into buffer
        // TODO: zero unset part of buffer
    }
    s->vorbisbuf = vb;
    if (stereo) {
        if (s->rc->stereo) {
            if (s->forcemono) {
                uint16_t tmp = ((int)vb.data[0][pos - vb.off] + (int)vb.data[1][pos - vb.off]) / 2;
                *out_l = tmp;
                *out_r = tmp;
            } else {
                *out_l = vb.data[0][pos - vb.off];
                *out_r = vb.data[1][pos - vb.off];
            }
        } else {
            *out_l = vb.data[0][pos - vb.off];
            *out_r = vb.data[0][pos - vb.off];
        }
    } else {
        if (s->rc->stereo) {
            *out_l = ((int)vb.data[0][pos - vb.off] + (int)vb.data[1][pos - vb.off]) / 2;
        } else {
            *out_l = vb.data[0][pos - vb.off];
        }
    }
}

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
