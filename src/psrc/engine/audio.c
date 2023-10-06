#include "audio.h"
#include "audiomixer.h"

#include "../aux/logging.h"
#include "../aux/string.h"

#include "../game/game.h"
#include "../game/time.h"

#include "../debug.h"

#include <inttypes.h>
#include <stdarg.h>
#include <math.h>

static void wrsamples(struct audiostate* a, int16_t* stream, int len) {
    int bufi = a->audbufindex;
    #if DEBUG(3)
    plog(LL_INFO | LF_DEBUG, "Playing %d...", bufi);
    #endif
    //uint64_t d = a->buftime;
    //d = (d > 10000) ? d - 10000 : 0;
    //uint64_t t;
    //if (d) t = altutime();
    //recheck:;
    int mixbufi = a->mixaudbufindex;
    if (mixbufi == bufi || mixbufi < 0) {
        //if (d) if (altutime() - t < d) goto recheck;
        signalCond(&a->mixcond);
        #if DEBUG(2)
        plog(LL_INFO | LF_DEBUG, "Mix thread is beind!");
        #endif
        memset(stream, 0, len * sizeof(*stream));
    } else {
        signalCond(&a->mixcond);
        int channels = a->channels;
        int samples = len / channels;
        #if DEBUG(1)
        if (a->audbuf.len != samples) {
            plog(LL_WARN | LF_DEBUG, "Mismatch between buffer length (%d) and requested samples (%d)", a->audbuf.len, samples);
        }
        #endif
        int* audbuf[2] = {a->audbuf.data[bufi][0], a->audbuf.data[bufi][1]}; // prevent an extra dereference on each write
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
        #if DEBUG(3)
        plog(LL_INFO | LF_DEBUG, "Finished playing %d", bufi);
        #endif
        a->audbufindex = (bufi + 1) % 2;
    }
}

static void callback(void* data, uint8_t* stream, int len) {
    if (((struct audiostate*)data)->valid) {
        wrsamples(data, (int16_t*)stream, len / sizeof(int16_t));
    } else {
        memset(stream, 0, len);
    }
}

static void* mixthread(struct thread_data* td) {
    struct audiostate* a = td->args;
    mutex_t m;
    createMutex(&m);
    lockMutex(&m);
    while (!td->shouldclose) {
        int mixbufi = a->mixaudbufindex;
        int bufi = a->audbufindex;
        bool mixbufilt0 = (mixbufi < 0);
        if (bufi == mixbufi || mixbufilt0) {
            mixbufi = (mixbufi + 1) % 2;
            #if DEBUG(3)
            plog(LL_INFO | LF_DEBUG, "Mixing %d...", mixbufi);
            #endif
            lockMutex(&a->voicelock);
            int* audbuf[2] = {a->audbuf.data[bufi][0], a->audbuf.data[bufi][1]};
            int samples = a->audbuf.len;
            memset(audbuf[0], 0, samples * sizeof(*audbuf[0]));
            memset(audbuf[1], 0, samples * sizeof(*audbuf[1]));
            mixsounds(a, samples, audbuf);
            unlockMutex(&a->voicelock);
            #if DEBUG(3)
            plog(LL_INFO | LF_DEBUG, "Finished mixing %d", mixbufi);
            #endif
            a->mixaudbufindex = mixbufi;
            if (!mixbufilt0) {
                if (td->shouldclose) break;
                waitOnCond(&a->mixcond, &m, 0);
            }
        }
    }
    unlockMutex(&m);
    destroyMutex(&m);
    return NULL;
}

static inline void stopSound_inline(struct audiosound* v) {
    v->id = -1;
    switch ((uint8_t)v->rc->format) {
        case RC_SOUND_FRMT_VORBIS: {
            stb_vorbis_close(v->vorbis);
            free(v->audbuf.data[0]);
            free(v->audbuf.data[1]);
        } break;
        case RC_SOUND_FRMT_MP3: {
            mp3dec_ex_close(v->mp3);
            free(v->mp3);
            free(v->audbuf.data_mp3);
        } break;
    }
    releaseResource(v->rc);
}

void stopSound(struct audiostate* a, int64_t id) {
    lockMutex(&a->lock);
    if (id >= 0) {
        int i = (int64_t)(id % (int64_t)a->voices);
        struct audiosound* v = &a->voicedata[i];
        if (v->id >= 0 && id == v->id) {
            lockMutex(&a->voicelock);
            stopSound_inline(v);
            unlockMutex(&a->voicelock);
        }
    }
    unlockMutex(&a->lock);
}

static inline void calcSoundFX(struct audiostate* a, struct audiosound* s) {
    s->fx[1].speedmul = roundf(s->speed * 1000.0);
    if (s->flags & SOUNDFLAG_POSEFFECT) {
        float vol[2] = {s->vol[0], s->vol[1]};
        float pos[3];
        if (s->flags & SOUNDFLAG_RELPOS) {
            pos[0] = s->pos[0];
            pos[1] = s->pos[1];
            pos[2] = s->pos[2];
        } else {
            pos[0] = s->pos[0] - a->campos[0];
            pos[1] = s->pos[1] - a->campos[1];
            pos[2] = s->pos[2] - a->campos[2];
        }
        float range = s->range;
        if (isnormal(range)) {
            float dist = sqrt(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
            if (isnormal(dist)) {
                if (vol[0] * range >= dist && vol[1] * range >= dist) {
                    pos[0] /= dist;
                    pos[1] /= dist;
                    pos[2] /= dist;
                    vol[0] *= 1.0 - (dist / range);
                    vol[1] *= 1.0 - (dist / range);
                    if (s->flags & SOUNDFLAG_NODOPPLER) {
                        s->fx[1].posoff = 0;
                    } else {
                        s->fx[1].posoff = roundf(dist * -0.002898 * (float)s->rc->freq);
                    }
                    if (!(s->flags & SOUNDFLAG_RELPOS)) {
                        float tmpsin[3];
                        tmpsin[0] = sin(a->camrot[0] / 180.0 * M_PI);
                        tmpsin[1] = sin(-a->camrot[1] / 180.0 * M_PI);
                        tmpsin[2] = sin(a->camrot[2] / 180.0 * M_PI);
                        float tmpcos[3];
                        tmpcos[0] = cos(a->camrot[0] / 180.0 * M_PI);
                        tmpcos[1] = cos(-a->camrot[1] / 180.0 * M_PI);
                        tmpcos[2] = cos(a->camrot[2] / 180.0 * M_PI);
                        float tmp[3][3];
                        tmp[0][0] = tmpcos[2] * tmpcos[1];
                        tmp[0][1] = tmpcos[2] * tmpsin[1] * tmpsin[0] - tmpsin[2] * tmpcos[0];
                        tmp[0][2] = tmpcos[2] * tmpsin[1] * tmpcos[0] + tmpsin[2] * tmpsin[0];
                        tmp[1][0] = tmpsin[2] * tmpcos[1];
                        tmp[1][1] = tmpsin[2] * tmpsin[1] * tmpsin[0] + tmpcos[2] * tmpcos[0];
                        tmp[1][2] = tmpsin[2] * tmpsin[1] * tmpcos[0] - tmpcos[2] * tmpsin[0];
                        tmp[2][0] = -tmpsin[1];
                        tmp[2][1] = tmpcos[1] * tmpsin[0];
                        tmp[2][2] = tmpcos[1] * tmpcos[0];
                        float out[3];
                        out[0] = tmp[0][0] * pos[0] + tmp[0][1] * pos[1] + tmp[0][2] * pos[2];
                        out[1] = tmp[1][0] * pos[0] + tmp[1][1] * pos[1] + tmp[1][2] * pos[2];
                        out[2] = tmp[2][0] * pos[0] + tmp[2][1] * pos[1] + tmp[2][2] * pos[2];
                        pos[0] = out[0];
                        pos[1] = out[1];
                        pos[2] = out[2];
                    }
                    if (pos[2] > 0.0) {
                        pos[0] *= 1.0 - 0.2 * pos[2];
                        vol[0] *= 1.0 + 0.175 * pos[2];
                        vol[1] *= 1.0 + 0.175 * pos[2];
                    } else if (pos[2] < 0.0) {
                        pos[0] *= 1.0 - 0.25 * -pos[2];
                        vol[0] *= 1.0 + 0.15 * pos[2];
                        vol[1] *= 1.0 + 0.15 * pos[2];
                    }
                    if (pos[1] > 0.0) {
                        vol[0] *= 1.0 - 0.1 * pos[1];
                        vol[1] *= 1.0 - 0.1 * pos[1];
                        pos[0] *= 1.0 - 0.2 * pos[1];
                    } else if (pos[1] > 0.0) {
                        pos[0] *= 1.0 - 0.2 * pos[1];
                    }
                    if (pos[0] > 0.0) {
                        float tmp = 1.0 - (dist / range) * 0.67;
                        vol[1] *= 1.0 + 0.33 * tmp * pos[0];
                        vol[0] *= 1.0 - 0.5 * tmp * 2.0 * pos[0];
                    } else if (pos[0] < 0.0) {
                        float tmp = 1.0 - (dist / range) * 0.67;
                        vol[0] *= 1.0 - 0.33 * tmp * pos[0];
                        vol[1] *= 1.0 - 0.5 * tmp * 2.0 * -pos[0];
                    }
                    s->fx[1].volmul[0] = roundf(vol[0] * 65536.0);
                    s->fx[1].volmul[1] = roundf(vol[1] * 65536.0);
                } else {
                    s->fx[1].volmul[0] = 0;
                    s->fx[1].volmul[1] = 0;
                }
            } else {
                s->fx[1].posoff = 0;
                s->fx[1].volmul[0] = roundf(vol[0] * 65536.0);
                s->fx[1].volmul[1] = roundf(vol[1] * 65536.0);
            }
        } else {
            s->fx[1].posoff = 0;
            s->fx[1].volmul[0] = 0;
            s->fx[1].volmul[1] = 0;
        }
    } else {
        s->fx[1].posoff = 0;
        s->fx[1].volmul[0] = roundf(s->vol[0] * 65536.0);
        s->fx[1].volmul[1] = roundf(s->vol[1] * 65536.0);
    }
}

void changeSoundFX(struct audiostate* a, int64_t id, int immediate, ...) {
    lockMutex(&a->lock);
    if (id >= 0) {
        int i = (int64_t)(id % (int64_t)a->voices);
        struct audiosound* v = &a->voicedata[i];
        if (v->id >= 0 && id == v->id) {
            lockMutex(&a->voicelock);
            if (!immediate && !v->state.fxchanged) {
                v->fx[0] = v->fx[1];
                v->state.fxchanged = 1;
            }
            va_list args;
            va_start(args, immediate);
            enum soundfx fx;
            while ((fx = va_arg(args, enum soundfx)) != SOUNDFX_END) {
                switch ((uint8_t)fx) {
                    case SOUNDFX_VOL:
                        v->vol[0] = va_arg(args, double);
                        v->vol[1] = va_arg(args, double);
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
            calcSoundFX(a, v);
            unlockMutex(&a->voicelock);
        }
    }
    unlockMutex(&a->lock);
}

void changeSoundFlags(struct audiostate* a, int64_t id, unsigned disable, unsigned enable) {
    lockMutex(&a->lock);
    if (id >= 0) {
        int i = (int64_t)(id % (int64_t)a->voices);
        struct audiosound* v = &a->voicedata[i];
        if (v->id >= 0 && id == v->id) {
            v->flags &= ~(uint8_t)disable;
            v->flags |= (uint8_t)enable;
        }
    }
    unlockMutex(&a->lock);
}

int64_t playSound(struct audiostate* a, bool paused, struct rc_sound* rc, unsigned f, ...) {
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
                lockMutex(&a->voicelock);
                stopSound_inline(tmpv);
                unlockMutex(&a->voicelock);
                id = vi;
                v = tmpv;
                break;
            }
        }
    }
    if (v) {
        lockMutex(&a->voicelock);
        grabResource(rc);
        v->id = id;
        v->rc = rc;
        switch ((uint8_t)rc->format) {
            case RC_SOUND_FRMT_VORBIS: {
                v->vorbis = stb_vorbis_open_memory(rc->data, rc->size, NULL, NULL);
                v->audbuf.off = 0;
                v->audbuf.len = a->audbuflen;
                v->audbuf.data[0] = malloc(v->audbuf.len * sizeof(*v->audbuf.data[0]));
                v->audbuf.data[1] = malloc(v->audbuf.len * sizeof(*v->audbuf.data[1]));
                stb_vorbis_get_samples_short(v->vorbis, 2, v->audbuf.data, v->audbuf.len);
            } break;
            case RC_SOUND_FRMT_MP3: {
                v->mp3 = malloc(sizeof(*v->mp3));
                mp3dec_ex_open_buf(v->mp3, rc->data, rc->size, MP3D_SEEK_TO_SAMPLE);
                v->audbuf.off = 0;
                v->audbuf.len = a->audbuflen;
                v->audbuf.data_mp3 = malloc(v->audbuf.len * v->rc->channels * sizeof(*v->audbuf.data_mp3));
                mp3dec_ex_read(v->mp3, v->audbuf.data_mp3, v->audbuf.len);
            } break;
        }
        v->offset = 0;
        v->flags = f;
        v->state.paused = paused;
        v->state.fxchanged = 0;
        v->vol[0] = 1.0;
        v->vol[1] = 1.0;
        v->speed = 1.0;
        v->pos[0] = 0.0;
        v->pos[1] = 0.0;
        v->pos[2] = 0.0;
        v->range = 15.0;
        v->fx[0].posoff = 0;
        v->fx[1].posoff = 0;
        va_list args;
        va_start(args, f);
        enum soundfx fx;
        while ((fx = va_arg(args, enum soundfx)) != SOUNDFX_END) {
            switch ((uint8_t)fx) {
                case SOUNDFX_VOL:
                    v->vol[0] = va_arg(args, double);
                    v->vol[1] = va_arg(args, double);
                    break;
                case SOUNDFX_SPEED:
                    v->speed = va_arg(args, double);
                    break;
                case SOUNDFX_POS:
                    v->pos[0] = va_arg(args, double);
                    v->pos[1] = va_arg(args, double);
                    v->pos[2] = va_arg(args, double);
                    break;
                case SOUNDFX_RANGE:
                    v->range = va_arg(args, double);
                    break;
            }
        }
        va_end(args);
        calcSoundFX(a, v);
        unlockMutex(&a->voicelock);
    }
    unlockMutex(&a->lock);
    return id;
}

struct rc_sound* loadSound(struct audiostate* a, const char* p) {
    return loadResource(RC_SOUND, p, &a->soundrcopt).sound;
}

void freeSound(struct audiostate* a, struct rc_sound* s) {
    (void)a;
    freeResource(s);
}

bool initAudio(struct audiostate* a) {
    memset(a, 0, sizeof(*a));
    if (!createMutex(&a->lock)) return false;
    if (!createCond(&a->mixcond)) return false;
    if (SDL_Init(SDL_INIT_AUDIO)) {
        plog(LL_CRIT | LF_FUNCLN, "Failed to init audio: %s", SDL_GetError());
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
        #if PLATFORM == PLAT_XBOX
        inspec.freq = 22050;
        #else
        inspec.freq = 44100;
        #endif
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
    #if PLATFORM != PLAT_XBOX
    int flags = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE;
    #else
    int flags = 0;
    #endif
    SDL_AudioDeviceID output = SDL_OpenAudioDevice(NULL, false, &inspec, &outspec, flags);
    if (output <= 0) {
        inspec.channels = 1;
        output = SDL_OpenAudioDevice(NULL, false, &inspec, &outspec, flags);
    }
    if (output > 0) {
        a->output = output;
        plog(LL_INFO, "Audio info (device id %d):", (int)output);
        plog(LL_INFO, "  Frequency: %d", outspec.freq);
        plog(LL_INFO, "  Channels: %d (%s)", outspec.channels, (outspec.channels == 1) ? "mono" : "stereo");
        plog(LL_INFO, "  Samples: %d", (int)outspec.samples);
        a->freq = outspec.freq;
        a->channels = outspec.channels;
        a->audbuf.len = outspec.samples;
        a->audbuf.data[0][0] = malloc(outspec.samples * sizeof(*a->audbuf.data[0][0]));
        a->audbuf.data[0][1] = malloc(outspec.samples * sizeof(*a->audbuf.data[0][1]));
        a->audbuf.data[1][0] = malloc(outspec.samples * sizeof(*a->audbuf.data[1][0]));
        a->audbuf.data[1][1] = malloc(outspec.samples * sizeof(*a->audbuf.data[1][1]));
        createMutex(&a->voicelock);
        int voices;
        tmp = cfg_getvar(config, "Sound", "voices");
        if (tmp) {
            voices = atoi(tmp);
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
        tmp = cfg_getvar(config, "Sound", "decodebuf");
        if (tmp) {
            a->audbuflen = atoi(tmp);
            free(tmp);
        } else {
            a->audbuflen = 4096;
        }
        tmp = cfg_getvar(config, "Sound", "decodewhole");
        if (tmp) {
            a->soundrcopt.decodewhole = strbool(tmp, true);
            free(tmp);
        } else {
            #if PLATFORM != PLAT_XBOX
            a->soundrcopt.decodewhole = true;
            #else
            a->soundrcopt.decodewhole = false;
            #endif
        }
        a->nextid = 0;
        a->audbufindex = 0;
        a->mixaudbufindex = -1;
        a->buftime = outspec.samples * 1000000 / outspec.freq;
        createThread(&a->mixthread, "mix", mixthread, a);
        a->valid = true;
        SDL_PauseAudioDevice(output, 0);
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
        quitThread(&a->mixthread);
        broadcastCond(&a->mixcond);
        destroyThread(&a->mixthread, NULL);
        destroyMutex(&a->voicelock);
        for (int i = 0; i < a->voices; ++i) {
            struct audiosound* v = &a->voicedata[i];
            if (v->id >= 0) {
               stopSound_inline(v);
            }
        }
        free(a->voicedata);
        free(a->audbuf.data[0][0]);
        free(a->audbuf.data[0][1]);
        free(a->audbuf.data[1][0]);
        free(a->audbuf.data[1][1]);
    }
    unlockMutex(&a->lock);
}

void termAudio(struct audiostate* a) {
    destroyMutex(&a->lock);
}
