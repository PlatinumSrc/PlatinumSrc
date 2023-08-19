#include "audio.h"

#include "../psrc_aux/logging.h"

#include <stdint.h>

static inline void getvorbisat_prepbuf(struct audiosound* s, struct audiosound_vorbisbuf vb, int pos) {
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
        // TODO: memcpy data[0] to data[1] if mono
        s->vorbisbuf = vb;
    } else if (pos < vb.off) {
        do {
            vb.off -= vb.len;
        } while (vb.off > 0 && pos < vb.off);
        if (vb.off < 0) vb.off = 0;
        // TODO: seek to vb.off
        // TODO: decode samples into buffer
        // TODO: zero unset part of buffer
        // TODO: memcpy data[0] to data[1] if mono
        s->vorbisbuf = vb;
    }
}

static inline void getvorbisat(struct audiosound* s, int pos, int* out_l, int* out_r) {
    if (pos < 0 || pos >= s->rc->len) {
        *out_l = 0;
        *out_r = 0;
        return;
    }
    struct audiosound_vorbisbuf vb = s->vorbisbuf;
    getvorbisat_prepbuf(s, vb, pos);
    *out_l = vb.data[0][pos - vb.off];
    *out_r = vb.data[1][pos - vb.off];
}

static inline void getvorbisat_forcemono(struct audiosound* s, int pos, int* out_l, int* out_r) {
    if (pos < 0 || pos >= s->rc->len) {
        *out_l = 0;
        *out_r = 0;
        return;
    }
    struct audiosound_vorbisbuf vb = s->vorbisbuf;
    getvorbisat_prepbuf(s, vb, pos);
    uint16_t tmp = ((int)vb.data[0][pos - vb.off] + (int)vb.data[1][pos - vb.off]) / 2;
    *out_l = tmp;
    *out_r = tmp;
}

static inline void mixsounds(struct audiostate* a, int samples) {
    if (a->audbuf.len != samples) {
        a->audbuf.len = samples;
        a->audbuf.data[0] = realloc(a->audbuf.data[0], samples * sizeof(*a->audbuf.data[0]));
        a->audbuf.data[1] = realloc(a->audbuf.data[1], samples * sizeof(*a->audbuf.data[1]));
    }
    memset(a->audbuf.data[0], 0, samples * sizeof(*a->audbuf.data[0]));
    memset(a->audbuf.data[1], 0, samples * sizeof(*a->audbuf.data[1]));
    for (int si = 0; si < a->sounds; ++si) {
        struct audiosound* s = &a->sounddata[si];
        if (!s->rc || s->state.done || s->state.paused) continue;
        switch (s->rc->format) {
            case RC_SOUND_FRMT_WAV: {
                for (int i = 0; i < samples; ++i) {
                }
            } break;
            case RC_SOUND_FRMT_VORBIS: {
                if (s->flags.forcemono) {
                    for (int i = 0; i < samples; ++i) {
                    }
                } else {
                    for (int i = 0; i < samples; ++i) {
                    }
                }
            } break;
        }
    }
}

static void wrsamples(struct audiostate* a, int16_t* stream, int channels, int len) {
    int samples = len / channels;
    //plog(LL_PLAIN, "Asked for %d samples", samples);
    lockMutex(&a->lock);
    mixsounds(a, samples);
    unlockMutex(&a->lock);
    int* audbuf[2] = {a->audbuf.data[0], a->audbuf.data[1]}; // prevent an extra dereference on each write
    if (channels < 2) {
        for (int i = 0; i < samples; ++i) {
            int sample = (audbuf[0][i] + audbuf[1][i]) / 2;
            if (sample > 32767) sample = 32767;
            else if (sample < -32768) sample = -32768;
            stream[i] = sample;
        }
    } else {
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < samples; ++i) {
                int sample = audbuf[c][i];
                if (sample > 32767) sample = 32767;
                else if (sample < -32768) sample = -32768;
                stream[i * channels + c] = sample;
            }
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
