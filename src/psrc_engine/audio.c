#include "audio.h"

#include "../psrc_aux/logging.h"

#include "../psrc_game/game.h"

#include <inttypes.h>
#include <stdarg.h>

static inline __attribute__((always_inline)) void getvorbisat_fillbuf(struct audiosound* s, struct audiosound_vorbisbuf* vb) {
    stb_vorbis_seek(s->vorbis, vb->off);
    int count = stb_vorbis_get_samples_short(s->vorbis, 2, vb->data, vb->len);
    for (int i = count; i < vb->len; ++i) {
        vb->data[0][i] = 0;
        vb->data[1][i] = 0;
    }
}

static inline __attribute__((always_inline)) void getvorbisat_prepbuf(struct audiosound* s, struct audiosound_vorbisbuf* vb, int pos, int len) {
    if (pos >= vb->off + vb->len) {
        int oldoff = vb->off;
        vb->off = pos;
        if (vb->off + vb->len >= len) {
            vb->off = len - vb->len;
            if (vb->off < 0) vb->off = 0;
        }
        if (vb->off != oldoff) {
            s->vorbisbuf.off = vb->off;
            getvorbisat_fillbuf(s, vb);
        }
    } else if (pos < vb->off) {
        int oldoff = vb->off;
        vb->off = pos - vb->len / 2 + 1;
        if (vb->off < 0) vb->off = 0;
        if (vb->off != oldoff) {
            s->vorbisbuf.off = vb->off;
            getvorbisat_fillbuf(s, vb);
        }
    }
}

static inline __attribute__((always_inline)) void getvorbisat(struct audiosound* s, struct audiosound_vorbisbuf* vb, int pos, int len, int* out_l, int* out_r) {
    if (pos < 0 || pos >= len) {
        *out_l = 0;
        *out_r = 0;
        return;
    }
    getvorbisat_prepbuf(s, vb, pos, len);
    //printf("<- [%d, %d] [%d]\n", s->vorbisbuf.off, s->vorbisbuf.len, pos);
    *out_l = vb->data[0][pos - vb->off];
    *out_r = vb->data[1][pos - vb->off];
}

static inline __attribute__((always_inline)) void getvorbisat_forcemono(struct audiosound* s, struct audiosound_vorbisbuf* vb, int pos, int len, int* out_l, int* out_r) {
    if (pos < 0 || pos >= len) {
        *out_l = 0;
        *out_r = 0;
        return;
    }
    getvorbisat_prepbuf(s, vb, pos, len);
    int tmp = ((int)vb->data[0][pos - vb->off] + (int)vb->data[1][pos - vb->off]) / 2;
    *out_l = tmp;
    *out_r = tmp;
}

static inline __attribute__((always_inline)) void interpfx(struct audiosound_fx* sfx, struct audiosound_fx* fx, int i, int ii, int samples) {
    int tmp = sfx[0].posoff * ii;
    if (tmp >= 0) tmp += samples - 1;
    else tmp -= samples - 1;
    fx->posoff = tmp / samples + sfx[1].posoff * i / samples;
    tmp = sfx[0].speedmul * ii;
    if (tmp >= 0) tmp += samples - 1;
    else tmp -= samples - 1;
    fx->speedmul = tmp / samples + sfx[1].speedmul * i / samples;
    fx->volmul[0] = (sfx[0].volmul[0] * ii + samples - 1) / samples + sfx[1].volmul[0] * i / samples;
    fx->volmul[1] = (sfx[0].volmul[1] * ii + samples - 1) / samples + sfx[1].volmul[1] * i / samples;
}

static inline __attribute__((always_inline)) int64_t calcpos(struct audiosound_fx* fx, int64_t offset, int64_t freq, int64_t outfreq) {
    return (offset * 1000 * (int64_t)fx->speedmul + (int64_t)fx->posoff * 256 * outfreq) * freq / 256000 / outfreq;
}

static inline void mixsounds(struct audiostate* a, int samples) {
    if (a->audbuf.len < samples) {
        a->audbuf.len = samples;
        a->audbuf.data[0] = realloc(a->audbuf.data[0], samples * sizeof(*a->audbuf.data[0]));
        a->audbuf.data[1] = realloc(a->audbuf.data[1], samples * sizeof(*a->audbuf.data[1]));
    }
    memset(a->audbuf.data[0], 0, samples * sizeof(*a->audbuf.data[0]));
    memset(a->audbuf.data[1], 0, samples * sizeof(*a->audbuf.data[1]));
    int* audbuf[2] = {a->audbuf.data[0], a->audbuf.data[1]};
    int tmpbuf[2];
    int outfreq = a->freq;
    for (int si = 0; si < a->voices; ++si) {
        struct audiosound* s = &a->voicedata[si];
        if (s->id < 0 || s->state.paused) continue;
        struct audiosound_fx sfx[2] = {s->fx[0], s->fx[1]};
        struct audiosound_fx fx;
        int64_t offset = s->offset;
        int freq = s->rc->freq;
        int len = s->rc->len;
        switch (s->rc->format) {
            case RC_SOUND_FRMT_WAV: {
            } break;
            case RC_SOUND_FRMT_VORBIS: {
                struct audiosound_vorbisbuf vb = s->vorbisbuf;
                if (s->flags & SOUNDFLAG_FORCEMONO) {
                    if (s->flags & SOUNDFLAG_LOOP) {
                        for (int i = 0, ii = samples; i < samples; ++i) {
                            interpfx(sfx, &fx, i, ii, samples);
                            int64_t pos = calcpos(&fx, offset, freq, outfreq);
                            if (pos >= 0) pos %= len;
                            getvorbisat_forcemono(s, &vb, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            audbuf[0][i] += tmpbuf[0];
                            audbuf[1][i] += tmpbuf[1];
                            ++offset;
                            --ii;
                        }
                    } else {
                        for (int i = 0, ii = samples; i < samples; ++i) {
                            interpfx(sfx, &fx, i, ii, samples);
                            int64_t pos = calcpos(&fx, offset, freq, outfreq);
                            getvorbisat_forcemono(s, &vb, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            audbuf[0][i] += tmpbuf[0];
                            audbuf[1][i] += tmpbuf[1];
                            ++offset;
                            --ii;
                        }
                    }
                } else {
                    if (s->flags & SOUNDFLAG_LOOP) {
                        for (int i = 0, ii = samples; i < samples; ++i) {
                            interpfx(sfx, &fx, i, ii, samples);
                            int64_t pos = calcpos(&fx, offset, freq, outfreq);
                            if (pos >= 0) pos %= len;
                            //printf("[%"PRId64"]: [%d, %d, %d, %d]\n", pos, fx.posoff, fx.speedmul, fx.volmul[0], fx.volmul[1]);
                            getvorbisat(s, &vb, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            audbuf[0][i] += tmpbuf[0];
                            audbuf[1][i] += tmpbuf[1];
                            ++offset;
                            --ii;
                        }
                    } else {
                        for (int i = 0, ii = samples; i < samples; ++i) {
                            interpfx(sfx, &fx, i, ii, samples);
                            int64_t pos = calcpos(&fx, offset, freq, outfreq);
                            getvorbisat(s, &vb, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            audbuf[0][i] += tmpbuf[0];
                            audbuf[1][i] += tmpbuf[1];
                            ++offset;
                            --ii;
                        }
                    }
                }
            } break;
        }
        s->offset += samples;
        s->fx[0] = sfx[1];
    }
}

static void wrsamples(struct audiostate* a, int16_t* stream, int channels, int len) {
    int samples = len / channels;
    //plog(LL_PLAIN, "Asked for %d samples", samples);
    mixsounds(a, samples);
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
    lockMutex(&((struct audiostate*)data)->lock);
    memset(stream, 0, len);
    int channels = ((struct audiostate*)data)->channels;
    wrsamples(data, (int16_t*)stream, channels, len / sizeof(int16_t));
    unlockMutex(&((struct audiostate*)data)->lock);
}

static inline void stopSound_inline(struct audiostate* a, int64_t id) {
    if (id >= 0) {
        int i = (int64_t)(id % (int64_t)a->voices);
        struct audiosound* v = &a->voicedata[i];
        if (id == v->id) {
            v->id = -1;
            if (v->rc->format == RC_SOUND_FRMT_VORBIS) {
                stb_vorbis_close(v->vorbis);
            }
            releaseResource(v->rc);
        }
    }
}

#if 0
static void stopSound_internal(struct audiostate* a, int64_t id) {
    stopSound_inline(a, id);
}
#endif

void stopSound(struct audiostate* a, int64_t id) {
    lockMutex(&a->lock);
    stopSound_inline(a, id);
    unlockMutex(&a->lock);
}

int64_t playSound(struct audiostate* a, struct rc_sound* rc, uint8_t f, ...) {
    lockMutex(&a->lock);
    if (!a->valid) {
        unlockMutex(&a->lock);
        return -1;
    }
    int64_t nextid = a->nextid;
    int64_t stopid = nextid + a->voices;
    int voices = a->voices;
    int64_t id = -1;
    struct audiosound* v = NULL;
    for (int64_t vi = nextid; vi < stopid; ++vi) {
        int i = (int64_t)(vi % (int64_t)voices);
        struct audiosound* tmpv = &a->voicedata[i];
        if (tmpv->id < 0) {
            id = vi;
            v = tmpv;
            break;
        }
    }
    if (!v) {
        for (int64_t vi = nextid; vi < stopid; ++vi) {
            int i = (int64_t)(vi % (int64_t)voices);
            struct audiosound* tmpv = &a->voicedata[i];
            if (!(tmpv->flags & SOUNDFLAG_UNINTERRUPTIBLE)) {
                stopSound_inline(a, tmpv->id);
                id = vi;
                v = tmpv;
                break;
            }
        }
    }
    if (v) {
        grabResource(rc);
        v->id = id;
        v->rc = rc;
        if (rc->format == RC_SOUND_FRMT_VORBIS) {
            v->vorbis = stb_vorbis_open_memory(rc->data, rc->size, NULL, NULL);
            v->vorbisbuf.off = 0;
            v->vorbisbuf.len = 4096;
            v->vorbisbuf.data[0] = malloc(v->vorbisbuf.len * sizeof(*v->vorbisbuf.data[0]));
            v->vorbisbuf.data[1] = malloc(v->vorbisbuf.len * sizeof(*v->vorbisbuf.data[1]));
            getvorbisat_fillbuf(v, &v->vorbisbuf);
        }
        v->offset = 0;
        v->flags = f;
        v->state.paused = 0;
        v->vol = 1.0;
        v->speed = 1.0;
        v->fx[0].posoff = 0;
        v->fx[0].speedmul = 256;
        v->fx[0].volmul[0] = 65536;
        v->fx[0].volmul[1] = 65536;
        v->fx[1] = v->fx[0];
        va_list args;
        va_start(args, f);
        enum soundfx fx;
        while ((fx = va_arg(args, enum soundfx)) != SOUNDFX_END) {
            switch ((uint8_t)fx) {
                case SOUNDFX_VOL:
                    v->vol = va_arg(args, double);
                    break;
                case SOUNDFX_SPEED:
                    v->speed = va_arg(args, double);
                    break;
                case SOUNDFX_POS:
                    v->pos[0] = va_arg(args, double);
                    v->pos[1] = va_arg(args, double);
                    v->pos[2] = va_arg(args, double);
                    break;
            }
        }
        va_end(args);
    }
    unlockMutex(&a->lock);
    return id;
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
    lockMutex(&a->lock);
    SDL_AudioSpec inspec;
    SDL_AudioSpec outspec;
    inspec.format = AUDIO_S16SYS;
    inspec.channels = 2;
    char* tmp = cfg_getvar(config, "Sound", "freq");
    if (tmp) {
        inspec.freq = atoi(tmp);
        free(tmp);
    } else {
        inspec.freq = 44100;
    }
    tmp = cfg_getvar(config, "Sound", "buffer");
    if (tmp) {
        inspec.samples = atoi(tmp);
        free(tmp);
    } else {
        inspec.samples = 1024;
    }
    inspec.callback = callback;
    inspec.userdata = a;
    SDL_AudioDeviceID output = SDL_OpenAudioDevice(
        NULL, false, &inspec, &outspec,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE
    );
    if (output <= 0) {
        inspec.channels = 1;
        output = SDL_OpenAudioDevice(
            NULL, false, &inspec, &outspec,
            SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE
        );
    }
    if (output > 0) {
        a->output = output;
        plog(LL_INFO, "Audio info (device id %d):", (int)output);
        plog(LL_INFO, "  Frequency: %d", outspec.freq);
        plog(LL_INFO, "  Channels: %d (%s)", outspec.channels, (outspec.channels == 1) ? "mono" : "stereo");
        plog(LL_INFO, "  Samples: %d", (int)outspec.samples);
        a->freq = outspec.freq;
        a->channels = outspec.channels;
        int voices;
        tmp = cfg_getvar(config, "Sound", "voices");
        if (tmp) {
            a->voices = atoi(tmp);
            free(tmp);
        } else {
            voices = 256;
        }
        if (a->voices != voices) {
            a->voices = voices;
            a->voicedata = realloc(a->voicedata, voices * sizeof(*a->voicedata));
        }
        for (int i = 0; i < voices; ++i) {
            a->voicedata[i].id = -1;
        }
        a->nextid = 0;
        SDL_PauseAudioDevice(output, 0);
        a->valid = true;
    } else {
        plog(LL_ERROR, "Failed to get audio info for default output device; audio disabled: %s", SDL_GetError());
        a->valid = false;
    }
    unlockMutex(&a->lock);
    return true;
}

void stopAudio(struct audiostate* a) {
    lockMutex(&a->lock);
    if (a->valid) {
        a->valid = false;
        SDL_PauseAudioDevice(a->output, 1);
        SDL_CloseAudioDevice(a->output);
    }
    unlockMutex(&a->lock);
}

void termAudio(struct audiostate* a) {
    destroyMutex(&a->lock);
}
