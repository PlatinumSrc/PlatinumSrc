#include "audio.h"
#include "audiomixer.h"

#include "../psrc_aux/logging.h"

#include "../psrc_game/game.h"

#include <inttypes.h>
#include <stdarg.h>
#include <math.h>

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
    //#if PLATFORM == PLAT_XBOX
    //KFLOATING_SAVE fpstate;
    //KeSaveFloatingPointState(&fpstate);
    //#endif
    wrsamples(data, (int16_t*)stream, channels, len / sizeof(int16_t));
    //#if PLATFORM == PLAT_XBOX
    //KeRestoreFloatingPointState(&fpstate);
    //#endif
    unlockMutex(&((struct audiostate*)data)->lock);
}

static void* decodethread(struct thread_data* td) {
    struct audiostate* a = td->args;
    /*
    while (!td->shouldclose) {
        plog(LL_INFO | LF_DEBUG, "Testing 1 2 3");
    }
    */
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
            stopSound_inline(v);
        }
    }
    unlockMutex(&a->lock);
}

static inline void calcSoundFX(struct audiostate* a, struct audiosound* s) {
    s->fx[1].speedmul = roundf(s->speed * 1000.0);
    //s->fx[1].posoff = roundf(x * (float)s->rc->freq);
    if (s->flags & SOUNDFLAG_POSEFFECT) {
        (void)a;
    } else {
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

int64_t playSound(struct audiostate* a, struct rc_sound* rc, unsigned f, ...) {
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
                stopSound_inline(tmpv);
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
        switch ((uint8_t)rc->format) {
            case RC_SOUND_FRMT_VORBIS: {
                v->vorbis = stb_vorbis_open_memory(rc->data, rc->size, NULL, NULL);
                v->audbuf.off = 0;
                v->audbuf.len = 2048;
                v->audbuf.data[0] = malloc(v->audbuf.len * sizeof(*v->audbuf.data[0]));
                v->audbuf.data[1] = malloc(v->audbuf.len * sizeof(*v->audbuf.data[1]));
                stb_vorbis_get_samples_short(v->vorbis, 2, v->audbuf.data, v->audbuf.len);
            } break;
            case RC_SOUND_FRMT_MP3: {
                v->mp3 = malloc(sizeof(*v->mp3));
                mp3dec_ex_open_buf(v->mp3, rc->data, rc->size, MP3D_SEEK_TO_SAMPLE);
                v->audbuf.off = 0;
                v->audbuf.len = 2048;
                v->audbuf.data_mp3 = malloc(v->audbuf.len * v->rc->channels * sizeof(*v->audbuf.data_mp3));
                mp3dec_ex_read(v->mp3, v->audbuf.data_mp3, v->audbuf.len);
            } break;
        }
        v->offset = 0;
        v->flags = f;
        v->state.paused = 0;
        v->state.fxchanged = 0;
        v->vol[0] = 1.0;
        v->vol[1] = 1.0;
        v->speed = 1.0;
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
            }
        }
        va_end(args);
        calcSoundFX(a, v);
    }
    unlockMutex(&a->lock);
    return id;
}

bool initAudio(struct audiostate* a) {
    memset(a, 0, sizeof(*a));
    if (!createMutex(&a->lock)) return false;
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
        a->nextid = 0;
        createThread(&a->decodethread, "auddec", decodethread, a);
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
        unlockMutex(&a->lock);
        destroyThread(&a->decodethread, NULL);
        SDL_PauseAudioDevice(a->output, 1);
        lockMutex(&a->lock);
        SDL_CloseAudioDevice(a->output);
        for (int i = 0; i < a->voices; ++i) {
            struct audiosound* v = &a->voicedata[i];
            if (v->id >= 0) {
               stopSound_inline(v);
            }
        }
        free(a->voicedata);
        free(a->audbuf.data[0]);
        free(a->audbuf.data[1]);
    }
    unlockMutex(&a->lock);
}

void termAudio(struct audiostate* a) {
    destroyMutex(&a->lock);
}
