#include "../rcmgralloc.h"

#include "audio.h"

#undef newAudioEmitter
#undef editAudioEmitter
#undef playSound
#undef editSoundEnv

#include "../common/logging.h"
#include "../common/string.h"
#include "../common/time.h"

#include "../common.h"
#include "../debug.h"

#include <inttypes.h>
#include <stdarg.h>
#include <math.h>

struct audiostate audiostate;

static ALWAYSINLINE void getvorbisat_fillbuf(struct audiosound* s, int off, int len) {
    stb_vorbis_seek(s->vorbis, off);
    stb_vorbis_get_samples_short(s->vorbis, 2, s->audbuf.data, len);
}
static int getvorbisat_prepbuf(struct audiosound* s, int pos) {
    if (!s->audbuf.data[0]) {
        s->audbuf.off = pos;
        s->audbuf.data[0] = malloc(s->audbuf.len * sizeof(**s->audbuf.data));
        s->audbuf.data[1] = malloc(s->audbuf.len * sizeof(**s->audbuf.data));
        getvorbisat_fillbuf(s, pos, s->audbuf.len);
        return 0;
    }
    int off = s->audbuf.off;
    int len = s->audbuf.len;
    int next = off + len;
    if (pos >= next) {
        off = pos;
        s->audbuf.off = off;
        getvorbisat_fillbuf(s, off, len);
        return 0;
    } else if (pos < off) {
        off = pos - len / 2;
        if (off < 0) off = 0;
        s->audbuf.off = off;
        getvorbisat_fillbuf(s, off, len);
    }
    return pos - off;
}
static ALWAYSINLINE void getvorbisat(struct audiosound* s, int pos, int* out_l, int* out_r) {
    int i = getvorbisat_prepbuf(s, pos);
    *out_l = s->audbuf.data[0][i];
    *out_r = s->audbuf.data[1][i];
}

static ALWAYSINLINE void getvorbisat_mono_fillbuf(struct audiosound* s, bool stereo, int off, int len) {
    stb_vorbis_seek(s->vorbis, off);
    stb_vorbis_get_samples_short(s->vorbis, 1 + stereo, s->audbuf.data, len);
    int16_t* d[2] = {s->audbuf.data[0], s->audbuf.data[1]};
    if (stereo) {
        for (int i = 0; i < len; ++i) {
            d[0][i] = (d[0][i] + d[1][i]) / 2;
        }
    }
}
static int getvorbisat_mono_prepbuf(struct audiosound* s, int pos, bool stereo) {
    if (!s->audbuf.data[0]) {
        s->audbuf.off = pos;
        s->audbuf.data[0] = malloc(s->audbuf.len * sizeof(**s->audbuf.data));
        if (stereo) s->audbuf.data[1] = malloc(s->audbuf.len * sizeof(**s->audbuf.data));
        getvorbisat_mono_fillbuf(s, stereo, pos, s->audbuf.len);
        return 0;
    }
    int off = s->audbuf.off;
    int len = s->audbuf.len;
    int next = off + len;
    if (pos >= next) {
        off = pos;
        s->audbuf.off = off;
        getvorbisat_mono_fillbuf(s, stereo, off, len);
        return 0;
    } else if (pos < off) {
        off = pos - len / 2;
        if (off < 0) off = 0;
        s->audbuf.off = off;
        getvorbisat_mono_fillbuf(s, stereo, off, len);
    }
    return pos - off;
}
static ALWAYSINLINE void getvorbisat_mono(struct audiosound* s, int pos, bool stereo, int* out) {
    int i = getvorbisat_mono_prepbuf(s, pos, stereo);
    *out = s->audbuf.data[0][i];
}

#ifdef PSRC_USEMINIMP3

static ALWAYSINLINE void getmp3at_fillbuf(struct audiosound* s, int off, int len, int ch) {
    mp3dec_ex_seek(s->mp3, off * ch);
    mp3dec_ex_read(s->mp3, s->audbuf.data_mp3, len * ch);
}
static int getmp3at_prepbuf(struct audiosound* s, int pos, int ch) {
    if (!s->audbuf.data[0]) {
        s->audbuf.off = pos;
        s->audbuf.data_mp3 = malloc(s->audbuf.len * ch * sizeof(*s->audbuf.data_mp3));
        getmp3at_fillbuf(s, pos, s->audbuf.len, ch);
        return 0;
    }
    int off = s->audbuf.off;
    int len = s->audbuf.len;
    int next = off + len;
    if (pos >= next) {
        off = pos;
        s->audbuf.off = off;
        getmp3at_fillbuf(s, off, len, ch);
        return 0;
    } else if (pos < off) {
        off = pos - len / 2;
        if (off < 0) off = 0;
        s->audbuf.off = off;
        getmp3at_fillbuf(s, off, len, ch);
    }
    return pos - off;
}
static ALWAYSINLINE void getmp3at(struct audiosound* s, int pos, bool stereo, int* out_l, int* out_r) {
    int channels = s->rc->channels;
    int i = getmp3at_prepbuf(s, pos, channels) * channels;
    *out_l = s->audbuf.data_mp3[i];
    *out_r = s->audbuf.data_mp3[i + stereo];
}

static ALWAYSINLINE void getmp3at_mono_fillbuf(struct audiosound* s, int off, int len, int ch) {
    mp3d_sample_t* d = s->audbuf.data_mp3;
    mp3dec_ex_seek(s->mp3, off * ch);
    mp3dec_ex_read(s->mp3, d, len * ch);
    if (ch > 1) {
        for (int i = 0; i < len; ++i) {
            d[i] = (d[i * ch] + d[i * ch + 1]) / 2;
        }
    }
}
static int getmp3at_mono_prepbuf(struct audiosound* s, int pos, int ch) {
    if (!s->audbuf.data[0]) {
        s->audbuf.off = pos;
        s->audbuf.data_mp3 = malloc(s->audbuf.len * ch * sizeof(*s->audbuf.data_mp3));
        getmp3at_mono_fillbuf(s, pos, s->audbuf.len, ch);
        return 0;
    }
    int off = s->audbuf.off;
    int len = s->audbuf.len;
    int next = off + len;
    if (pos >= next) {
        off = pos;
        s->audbuf.off = off;
        getmp3at_mono_fillbuf(s, off, len, ch);
        return 0;
    } else if (pos < off) {
        off = pos - len / 2;
        if (off < 0) off = 0;
        s->audbuf.off = off;
        getmp3at_mono_fillbuf(s, off, len, ch);
    }
    return pos - off;
}
static ALWAYSINLINE void getmp3at_mono(struct audiosound* s, int pos, int* out) {
    int channels = s->rc->channels;
    int i = getmp3at_mono_prepbuf(s, pos, channels);
    *out = s->audbuf.data_mp3[i];
}

#endif

static void calc3DSoundFx(struct audiosound_3d* s) {
    struct audioemitter* e = &audiostate.emitters.data[s->emitter];
    s->fx[1].speedmul = roundf(e->speed * s->speed * 32.0f);
    float vol[2] = {s->vol[0] * e->vol[0], s->vol[1] * e->vol[1]};
    float pos[3];
    pos[0] = e->pos[0] + s->pos[0] - audiostate.cam.pos[0];
    pos[1] = e->pos[1] + s->pos[1] - audiostate.cam.pos[1];
    pos[2] = e->pos[2] + s->pos[2] - audiostate.cam.pos[2];
    float range = e->range * s->range;
    if (isnormal(range)) {
        float dist = sqrtf(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
        if (isnormal(dist)) {
            if (dist < range) {
                float loudness = 1.0f - (dist / range);
                loudness *= loudness;
                loudness *= loudness;
                loudness *= loudness;
                pos[0] /= dist;
                pos[1] /= dist;
                pos[2] /= dist;
                vol[0] *= loudness;
                vol[1] *= loudness;
                if (s->flags & SOUNDFLAG_NODOPPLER) {
                    s->fx[1].posoff = 0;
                } else {
                    s->fx[1].posoff = roundf(dist * -0.0025f * (float)audiostate.freq);
                }
                float tmp[3];
                float mul[3][3];
                mul[0][0] = audiostate.cam.sinx * audiostate.cam.siny * audiostate.cam.sinz + audiostate.cam.cosy * audiostate.cam.cosz;
                mul[0][1] = audiostate.cam.cosx * -audiostate.cam.sinz;
                mul[0][2] = audiostate.cam.sinx * audiostate.cam.cosy * audiostate.cam.sinz + audiostate.cam.siny * audiostate.cam.cosz;
                mul[1][0] = audiostate.cam.sinx * audiostate.cam.siny * audiostate.cam.cosz + audiostate.cam.cosy * audiostate.cam.sinz;
                mul[1][1] = audiostate.cam.cosx * audiostate.cam.cosz;
                mul[1][2] = -audiostate.cam.sinx * audiostate.cam.cosy * audiostate.cam.cosz + audiostate.cam.siny * audiostate.cam.sinz;
                mul[2][0] = audiostate.cam.cosx * -audiostate.cam.siny;
                mul[2][1] = audiostate.cam.sinx;
                mul[2][2] = audiostate.cam.cosx * audiostate.cam.cosy;
                tmp[0] = pos[0] * mul[0][0] + pos[1] * mul[0][1] + pos[2] * mul[0][2];
                tmp[1] = pos[0] * mul[1][0] + pos[1] * mul[1][1] + pos[2] * mul[1][2];
                tmp[2] = pos[0] * mul[2][0] + pos[1] * mul[2][1] + pos[2] * mul[2][2];
                //printf("ANGLE: {%+.03f, %+.03f, %+.03f} -> ", pos[0], pos[1], pos[2]);
                //printf("{%+.03f, %+.03f, %+.03f}\n", tmp[0], tmp[1], tmp[2]);
                pos[0] = tmp[0];
                pos[1] = tmp[1];
                pos[2] = tmp[2];
                if (pos[2] > 0.0f) {
                    pos[0] *= 1.0f - 0.2f * pos[2];
                } else if (pos[2] < 0.0f) {
                    pos[0] *= 1.0f - 0.25f * pos[2];
                    vol[0] *= 1.0f + 0.25f * pos[2];
                    vol[1] *= 1.0f + 0.25f * pos[2];
                }
                if (pos[1] > 0.0f) {
                    pos[0] *= 1.0f - 0.2f * pos[1];
                    vol[0] *= 1.0f - 0.1f * pos[1];
                    vol[1] *= 1.0f - 0.1f * pos[1];
                } else if (pos[1] < 0.0f) {
                    pos[0] *= 1.0f - 0.2f * pos[1];
                }
                if (pos[0] > 0.0f) {
                    vol[0] *= 1.0f - pos[0] * 0.75f;
                } else if (pos[0] < 0.0f) {
                    vol[1] *= 1.0f + pos[0] * 0.75f;
                }
                s->fx[1].volmul[0] = roundf(vol[0] * 32768.0f);
                s->fx[1].volmul[1] = roundf(vol[1] * 32768.0f);
                s->fx[1].volmul[2] = (s->fx[1].volmul[1] > s->fx[1].volmul[0]) ? s->fx[1].volmul[1] : s->fx[1].volmul[0];
            } else {
                s->fx[1].volmul[0] = 0;
                s->fx[1].volmul[1] = 0;
                s->fx[1].volmul[2] = 0;
            }
        } else {
            s->fx[1].posoff = 0;
            s->fx[1].volmul[0] = roundf(vol[0] * 32768.0f);
            s->fx[1].volmul[1] = roundf(vol[1] * 32768.0f);
            s->fx[1].volmul[2] = (s->fx[1].volmul[1] > s->fx[1].volmul[0]) ? s->fx[1].volmul[1] : s->fx[1].volmul[0];
        }
    } else {
        s->fx[1].posoff = 0;
        s->fx[1].volmul[0] = 0;
        s->fx[1].volmul[1] = 0;
        s->fx[1].volmul[2] = 0;
    }
    s->maxvol = (s->fx[0].volmul[2] > s->fx[1].volmul[2]) ? s->fx[0].volmul[2] : s->fx[1].volmul[2];
    register float tmp = 1.0f - (1.0f - s->lpfilt) * (1.0f - e->lpfilt);
    if (tmp > 0.0f) {
        tmp = 1.0f - tmp;
        s->fx[1].lpfiltmul = (int)roundf(tmp * tmp * audiostate.freq);
    } else {
        s->fx[1].lpfiltmul = audiostate.freq;
    }
    tmp = 1.0f - (1.0f - s->hpfilt) * (1.0f - e->hpfilt);
    if (tmp > 0.0f) {
        s->fx[1].hpfiltmul = audiostate.freq - (int)roundf(tmp * tmp * audiostate.freq);
    } else {
        s->fx[1].hpfiltmul = audiostate.freq;
    }
}

static inline void stopSound_inline(struct audiosound* s) {
    switch ((uint8_t)s->rc->format) {
        case RC_SOUND_FRMT_VORBIS: {
            stb_vorbis_close(s->vorbis);
            free(s->audbuf.data[0]);
            free(s->audbuf.data[1]);
        } break;
        #ifdef PSRC_USEMINIMP3
        case RC_SOUND_FRMT_MP3: {
            mp3dec_ex_close(s->mp3);
            free(s->mp3);
            free(s->audbuf.data_mp3);
        } break;
        #endif
    }
    unlockRc(s->rc);
    s->rc = NULL;
}
static inline void stop3DSound_inline(struct audiosound_3d* s) {
    stopSound_inline(&s->data);
    --audiostate.emitters.data[s->emitter].uses;
}

static ALWAYSINLINE void interpfx(struct audiosound_fx* sfx, struct audiosound_fx* fx, int i, int ii, int samples) {
    fx->posoff = (sfx[0].posoff * ii + sfx[1].posoff * i) / samples;
    fx->speedmul = (sfx[0].speedmul * ii + sfx[1].speedmul * i) / samples;
    fx->volmul[0] = (sfx[0].volmul[0] * ii + sfx[1].volmul[0] * i) / samples;
    fx->volmul[1] = (sfx[0].volmul[1] * ii + sfx[1].volmul[1] * i) / samples;
    fx->lpfiltmul = (sfx[0].lpfiltmul * ii + sfx[1].lpfiltmul * i) / samples;
    fx->hpfiltmul = (sfx[0].hpfiltmul * ii + sfx[1].hpfiltmul * i) / samples;
}
#define MIXSOUND3D_CALCPOS() do {\
    register int mul = ((fx.posoff - fxoff + 1) * freq) * fx.speedmul;\
    fxoff = fx.posoff;\
    offset += mul / div;\
    frac += mul % div;\
    offset += frac / div;\
    frac %= div;\
    pos = offset;\
} while (0)
#define MIXSOUND3D_LOOP_COMMON(wp, wp2, poob, p2oob) do {\
    if (!(poob)) {\
        wp;\
        MIXSOUND_GETSAMPLE(pos, o1);\
        if (frac) {\
            pos2 = pos + 1;\
            if (!(p2oob)) {\
                wp2;\
                MIXSOUND_GETSAMPLE(pos2, o2);\
                uint8_t tmpfrac = frac / outfreq;\
                uint8_t ifrac = 32 - tmpfrac;\
                o1 = (o1 * ifrac + o2 * tmpfrac) / 32;\
            }\
        }\
        if (filterfx) {\
            hplastout = (hplastout + o1 - hplastin) * fx.hpfiltmul / audiostate.freq;\
            hplastin = o1;\
            o1 = lplastout = lplastout + (hplastout - lplastout) * fx.lpfiltmul / audiostate.freq;\
        }\
        audbuf[0][i] += o1 * fx.volmul[0] / 32768;\
        audbuf[1][i] += o1 * fx.volmul[1] / 32768;\
    }\
} while (0)
#define MIXSOUND3D_LOOP(wp, wp2, poob, p2oob) do {\
    if (s->fxchanged) {\
        register int i = 0, ii = audiostate.audbuf.len;\
        while (1) {\
            interpfx(sfx, &fx, i, ii, audiostate.audbuf.len);\
            MIXSOUND3D_CALCPOS();\
            MIXSOUND3D_LOOP_COMMON(wp, wp2, poob, p2oob);\
            ++i;\
            if (i == audiostate.audbuf.len) {if (poob) return false; break;}\
            --ii;\
        }\
    } else {\
        register int i = 0;\
        while (1) {\
            MIXSOUND3D_CALCPOS();\
            MIXSOUND3D_LOOP_COMMON(wp, wp2, poob, p2oob);\
            ++i;\
            if (i == audiostate.audbuf.len) {if (poob) return false; break;}\
        }\
    }\
} while (0)
#define MIXSOUND3D_BODY() do {\
    if (flags & SOUNDFLAG_LOOP) {\
        if (flags & SOUNDFLAG_WRAP) MIXSOUND3D_LOOP(pos = ((pos % len) + len) % len, pos2 = ((pos2 % len) + len) % len, 0, 0);\
        else MIXSOUND3D_LOOP(if (pos >= 0) pos %= len, if (pos2 >= 0) pos2 %= len, (pos < 0), 0);\
    } else {\
        MIXSOUND3D_LOOP(,,(pos >= len || pos < 0), (pos2 >= len));\
    }\
} while (0)
static bool mixsound_3d(struct audiosound_3d* s, int** audbuf) {
    struct rc_sound* rc = s->data.rc;
    if (!rc) return true;
    int len = rc->len;
    if (!len) return true;
    int freq = rc->freq;
    struct audioemitter* e = &audiostate.emitters.data[s->emitter];
    if (e->paused) return true;
    uint8_t flags = s->flags;
    struct audiosound_fx fx, sfx[2];
    long offset = s->data.offset;
    int frac = s->data.frac;
    int fxoff = s->fxoff;
    bool filterfx;
    #if 0
    int16_t lplastout;
    int16_t hplastout;
    int16_t hplastin;
    #endif
    int16_t lplastout = s->lplastout;
    int16_t hplastout = s->hplastout;
    int16_t hplastin = s->hplastin;
    if (s->fxchanged) {
        //puts("fxchanged");
        sfx[0] = s->fx[0];
        sfx[1] = s->fx[1];
        sfx[0].volmul[0] = sfx[0].volmul[0] * audiostate.vol.master / 100;
        sfx[0].volmul[1] = sfx[0].volmul[1] * audiostate.vol.master / 100;
        sfx[1].volmul[0] = sfx[1].volmul[0] * audiostate.vol.master / 100;
        sfx[1].volmul[1] = sfx[1].volmul[1] * audiostate.vol.master / 100;
        if (sfx[0].lpfiltmul != audiostate.freq || sfx[0].hpfiltmul != audiostate.freq ||
            sfx[1].lpfiltmul != audiostate.freq || sfx[1].lpfiltmul != audiostate.freq) {
            filterfx = true;
            #if 0
            lplastout = s->lplastout;
            hplastout = s->hplastout;
            hplastin = s->hplastin;
            #endif
        } else {
            filterfx = false;
        }
    } else {
        fx = s->fx[1];
        fx.volmul[0] = fx.volmul[0] * audiostate.vol.master / 100;
        fx.volmul[1] = fx.volmul[1] * audiostate.vol.master / 100;
        if (fx.lpfiltmul != audiostate.freq || fx.hpfiltmul != audiostate.freq) {
            filterfx = true;
            #if 0
            lplastout = s->lplastout;
            hplastout = s->hplastout;
            hplastin = s->hplastin;
            #endif
        } else {
            filterfx = false;
        }
    }
    int outfreq = audiostate.freq;
    {
        int outfreq2 = outfreq, gcd = freq;
        while (outfreq2) {
            int tmp = gcd % outfreq2;
            gcd = outfreq2;
            outfreq2 = tmp;
        }
        outfreq /= gcd;
        freq /= gcd;
    }
    int div = outfreq * 32;
    long pos;
    long pos2;
    bool stereo = rc->stereo;
    int o1, o2;
    switch (rc->format) {
        case RC_SOUND_FRMT_WAV: {
            union {
                uint8_t* ptr;
                uint8_t* i8;
                int16_t* i16;
            } data;
            data.ptr = rc->data;
            bool is8bit = rc->is8bit;
            int channels = rc->channels;
            if (stereo) {
                if (is8bit) {
                    #define MIXSOUND_GETSAMPLE(p, o) do {\
                        o = data.i8[p * channels] - 128;\
                        o = o * 256 + (o + 128);\
                        register int tmpo = data.i8[p * channels + 1] - 128;\
                        tmpo = tmpo * 256 + (tmpo + 128);\
                        o = (o + tmpo) / 2;\
                    } while (0)
                    MIXSOUND3D_BODY();
                    #undef MIXSOUND_GETSAMPLE
                } else {
                    #define MIXSOUND_GETSAMPLE(p, o) o = (data.i16[p * channels] + data.i16[p * channels + 1]) / 2
                    MIXSOUND3D_BODY();
                    #undef MIXSOUND_GETSAMPLE
                }
            } else {
                if (is8bit) {
                    #define MIXSOUND_GETSAMPLE(p, o) do {\
                        o = data.i8[p] - 128;\
                        o = o * 256 + (o + 128);\
                    } while (0)
                    MIXSOUND3D_BODY();
                    #undef MIXSOUND_GETSAMPLE
                } else {
                    #define MIXSOUND_GETSAMPLE(p, o) o = data.i16[p]
                    MIXSOUND3D_BODY();
                    #undef MIXSOUND_GETSAMPLE
                }
            }
        } break;
        case RC_SOUND_FRMT_VORBIS: {
            #define MIXSOUND_GETSAMPLE(p, o) getvorbisat_mono(&s->data, p, stereo, &o)
            MIXSOUND3D_BODY();
            #undef MIXSOUND_GETSAMPLE
        } break;
        #ifdef PSRC_USEMINIMP3
        case RC_SOUND_FRMT_MP3: {
            #define MIXSOUND_GETSAMPLE(p, o) getmp3at_mono(&s->data, p, &o)
            MIXSOUND3D_BODY();
            #undef MIXSOUND_GETSAMPLE
        } break;
        #endif
    }
    if (s->fxchanged) s->fxchanged = false;
    s->data.offset = offset;
    s->data.frac = frac;
    s->fxoff = fxoff;
    s->lplastout = lplastout;
    s->hplastout = hplastout;
    s->hplastin = hplastin;
    return true;
}
#undef MIXSOUND3D_LOOP_COMMON
#undef MIXSOUND3D_LOOP
#undef MIXSOUND3D_BODY
#define MIXSOUND3DFAKE_BODY(c) {\
    if (s->fxchanged) {\
        register int i = 0, ii = audiostate.audbuf.len;\
        while (1) {\
            interpfx(sfx, &fx, i, ii, audiostate.audbuf.len);\
            MIXSOUND3D_CALCPOS();\
            c\
            ++i;\
            if (i == audiostate.audbuf.len) {if (MIXSOUND_POSCHECK) return false; break;}\
            --ii;\
        }\
    } else {\
        register int i = 0;\
        while (1) {\
            MIXSOUND3D_CALCPOS();\
            c\
            ++i;\
            if (i == audiostate.audbuf.len) {if (MIXSOUND_POSCHECK) return false; break;}\
        }\
    }\
}
static bool mixsound_3d_fake(struct audiosound_3d* s) {
    struct rc_sound* rc = s->data.rc;
    if (!rc) return true;
    int len = rc->len;
    if (!len) return true;
    int freq = rc->freq;
    struct audioemitter* e = &audiostate.emitters.data[s->emitter];
    if (e->paused) return true;
    uint8_t flags = s->flags;
    struct audiosound_fx fx, sfx[2];
    long offset = s->data.offset;
    int fxoff = s->fxoff;
    int frac = s->data.frac;
    if (s->fxchanged) {
        //puts("fxchanged");
        sfx[0] = s->fx[0];
        sfx[1] = s->fx[1];
    } else {
        fx = s->fx[1];
    }
    int outfreq = audiostate.freq;
    {
        int outfreq2 = outfreq, gcd = freq;
        while (outfreq2) {
            int tmp = gcd % outfreq2;
            gcd = outfreq2;
            outfreq2 = tmp;
        }
        outfreq /= gcd;
        freq /= gcd;
    }
    int div = outfreq * 32;
    long pos;
    if (flags & SOUNDFLAG_LOOP) {
        if (flags & SOUNDFLAG_WRAP) {
            #define MIXSOUND_POSCHECK 0
            MIXSOUND3DFAKE_BODY (
                pos = ((pos % len) + len) % len;
            )
            #undef MIXSOUND_POSCHECK
        } else {
            #define MIXSOUND_POSCHECK (pos < 0)
            MIXSOUND3DFAKE_BODY (
                if (pos >= 0) pos %= len;
            )
            #undef MIXSOUND_POSCHECK
        }
    } else {
        #define MIXSOUND_POSCHECK (pos >= len || pos < 0)
        MIXSOUND3DFAKE_BODY ()
        #undef MIXSOUND_POSCHECK
    }
    if (s->fxchanged) s->fxchanged = false;
    s->data.offset = offset;
    s->data.frac = frac;
    s->fxoff = fxoff;
    return true;
}
#undef MIXSOUND3DFAKE_BODY
#undef MIXSOUND3D_CALCPOS

#define MIXSOUND2D_CALCPOS() do {\
    offset += freq / outfreq;\
    frac += freq % outfreq;\
    offset += frac / outfreq;\
    frac %= outfreq;\
    pos = offset;\
} while (0)
#define MIXSOUND2D_LOOP_COMMON(wp, wp2, poob, p2oob, v) do {\
    if (!(poob)) {\
        wp;\
        MIXSOUND_GETSAMPLE(pos, l1, r1);\
        if (frac) {\
            pos2 = pos + 1;\
            if (!(p2oob)) {\
                wp2;\
                MIXSOUND_GETSAMPLE(pos2, l2, r2);\
                uint8_t tmpfrac = frac * 32 / outfreq;\
                uint8_t ifrac = 32 - tmpfrac;\
                l1 = (l1 * ifrac + l2 * tmpfrac) / 32;\
                r1 = (r1 * ifrac + r2 * tmpfrac) / 32;\
            }\
        }\
        audbuf[0][i] += l1 * v / 32768;\
        audbuf[1][i] += r1 * v / 32768;\
    }\
} while (0)
#define MIXSOUND2D_LOOP(wp, wp2, poob, p2oob) do {\
    if (oldvol != newvol) {\
        register int i = 0, ii = audiostate.audbuf.len;\
        while (1) {\
            vol = (oldvol * ii + newvol * i) / audiostate.audbuf.len;\
            MIXSOUND2D_CALCPOS();\
            MIXSOUND2D_LOOP_COMMON(wp, wp2, poob, p2oob, vol);\
            ++i;\
            if (i == audiostate.audbuf.len) {if (poob) return false; break;}\
            --ii;\
        }\
    } else {\
        register int i = 0;\
        while (1) {\
            MIXSOUND2D_CALCPOS();\
            MIXSOUND2D_LOOP_COMMON(wp, wp2, poob, p2oob, newvol);\
            ++i;\
            if (i == audiostate.audbuf.len) {if (poob) return false; break;}\
        }\
    }\
} while (0)
#define MIXSOUND2D_BODY() do {\
    if (flags & SOUNDFLAG_LOOP) MIXSOUND2D_LOOP(if (pos >= 0) pos %= len, if (pos2 >= 0) pos2 %= len, (pos < 0), 0);\
    else MIXSOUND2D_LOOP(,,(pos >= len || pos < 0), (pos2 >= len));\
} while (0)
static bool mixsound_2d(struct audiosound* s, int** audbuf, uint8_t flags, int oldvol, int newvol, int volmul) {
    struct rc_sound* rc = s->rc;
    if (!rc) return true;
    int len = rc->len;
    if (!len) return true;
    int freq = rc->freq;
    long offset = s->offset;
    int frac = s->frac;
    int outfreq = audiostate.freq;
    {
        int outfreq2 = outfreq, gcd = freq;
        while (outfreq2) {
            int tmp = gcd % outfreq2;
            gcd = outfreq2;
            outfreq2 = tmp;
        }
        outfreq /= gcd;
        freq /= gcd;
    }
    long pos;
    long pos2;
    bool stereo = rc->stereo;
    int l1, r1;
    int l2, r2;
    oldvol = oldvol * volmul * audiostate.vol.master / 10000;
    newvol = newvol * volmul * audiostate.vol.master / 10000;
    int vol;
    switch (rc->format) {
        case RC_SOUND_FRMT_WAV: {
            union {
                uint8_t* ptr;
                uint8_t* i8;
                int16_t* i16;
            } data;
            data.ptr = rc->data;
            bool is8bit = rc->is8bit;
            int channels = rc->channels;
            if (stereo) {
                if (is8bit) {
                    #define MIXSOUND_GETSAMPLE(p, l, r) do {\
                        l = data.i8[p * channels] - 128;\
                        l = l * 256 + (l + 128);\
                        r = data.i8[p * channels + 1] - 128;\
                        r = r * 256 + (r + 128);\
                    } while (0)
                    MIXSOUND2D_BODY();
                    #undef MIXSOUND_GETSAMPLE
                } else {
                    #define MIXSOUND_GETSAMPLE(p, l, r) do {\
                        l = data.i16[p * channels];\
                        r = data.i16[p * channels + 1];\
                    } while (0)
                    MIXSOUND2D_BODY();
                    #undef MIXSOUND_GETSAMPLE
                }
            } else {
                if (is8bit) {
                    #define MIXSOUND_GETSAMPLE(p, l, r) do {\
                        l = data.i8[p] - 128;\
                        l = l * 256 + (l + 128);\
                        r = l;\
                    } while (0)
                    MIXSOUND2D_BODY();
                    #undef MIXSOUND_GETSAMPLE
                } else {
                    #define MIXSOUND_GETSAMPLE(p, l, r) r = l = data.i16[p]
                    MIXSOUND2D_BODY();
                    #undef MIXSOUND_GETSAMPLE
                }
            }
        } break;
        case RC_SOUND_FRMT_VORBIS: {
            #define MIXSOUND_GETSAMPLE(p, l, r) getvorbisat(s, p, &l, &r)
            MIXSOUND2D_BODY();
            #undef MIXSOUND_GETSAMPLE
        } break;
        #ifdef PSRC_USEMINIMP3
        case RC_SOUND_FRMT_MP3: {
            #define MIXSOUND_GETSAMPLE(p, l, r) getmp3at(s, p, stereo, &l, &r)
            MIXSOUND2D_BODY();
            #undef MIXSOUND_GETSAMPLE
        } break;
        #endif
    }
    s->offset = offset;
    s->frac = frac;
    return true;
}
#undef MIXSOUND2D_LOOP_COMMON
#undef MIXSOUND2D_LOOP
#undef MIXSOUND2D_BODY
#define MIXSOUND2DFAKE_BODY(c) {\
    register int i = 0;\
    while (1) {\
        MIXSOUND2D_CALCPOS();\
        c\
        ++i;\
        if (i == audiostate.audbuf.len) {if (MIXSOUND_POSCHECK) return false; break;}\
    }\
}
static bool mixsound_2d_fake(struct audiosound* s, uint8_t flags) {
    struct rc_sound* rc = s->rc;
    if (!rc) return true;
    int len = rc->len;
    if (!len) return true;
    int freq = rc->freq;
    long offset = s->offset;
    int frac = s->frac;
    int outfreq = audiostate.freq;
    {
        int outfreq2 = outfreq, gcd = freq;
        while (outfreq2) {
            int tmp = gcd % outfreq2;
            gcd = outfreq2;
            outfreq2 = tmp;
        }
        outfreq /= gcd;
        freq /= gcd;
    }
    long pos;
    if (flags & SOUNDFLAG_LOOP) {
        #define MIXSOUND_POSCHECK 0
        MIXSOUND2DFAKE_BODY (
            if (pos >= 0) pos %= len;
        )
        #undef MIXSOUND_POSCHECK
    } else {
        #define MIXSOUND_POSCHECK (pos >= len)
        MIXSOUND2DFAKE_BODY ()
        #undef MIXSOUND_POSCHECK
    }
    s->offset = offset;
    s->frac = frac;
    return true;
}
#undef MIXSOUND2DFAKE_BODY
#undef MIXSOUND2D_CALCPOS

static void doReverb(unsigned len, int* outl, int* outr) {
    int mix = roundf(audiostate.env.reverb.mix[1] * 256.0f);
    if (!mix) return;
    int feedback = roundf(audiostate.env.reverb.feedback[1] * 256.0f);
    if (!audiostate.env.reverb.size) {
        register unsigned size = len - 1;
        size |= size >> 1;
        size |= size >> 2;
        size |= size >> 4;
        size |= size >> 8;
        size |= size >> 16;
        ++size;
        audiostate.env.reverb.size = size;
        audiostate.env.reverb.buf[0] = calloc(size, sizeof(**audiostate.env.reverb.buf));
        audiostate.env.reverb.buf[1] = calloc(size, sizeof(**audiostate.env.reverb.buf));
    }
    register unsigned head = audiostate.env.reverb.head;
    register unsigned tail = (head + (audiostate.env.reverb.size - len)) % audiostate.env.reverb.size;
    register float filtamount = 1.0f - audiostate.env.reverb.lpfilt.amount[1];
    int lpmul = (int)roundf(filtamount * filtamount * audiostate.freq);
    filtamount = audiostate.env.reverb.hpfilt.amount[1];
    int hpmul = audiostate.freq - (int)roundf(filtamount * filtamount * audiostate.freq);
    int lplastoutl = audiostate.env.reverb.lpfilt.lastout[0];
    int lplastoutr = audiostate.env.reverb.lpfilt.lastout[1];
    int hplastoutl = audiostate.env.reverb.hpfilt.lastout[0];
    int hplastoutr = audiostate.env.reverb.hpfilt.lastout[1];
    int hplastinl = audiostate.env.reverb.hpfilt.lastin[0];
    int hplastinr = audiostate.env.reverb.hpfilt.lastin[1];
    for (register int i = 0; i < audiostate.audbuf.len; ++i) {
        int in[2] = {outl[i], outr[i]};
        int out[2] = {audiostate.env.reverb.buf[0][tail], audiostate.env.reverb.buf[1][tail]};
        hplastoutl = (hplastoutl + out[0] - hplastinl) * hpmul / audiostate.freq;
        hplastoutr = (hplastoutr + out[1] - hplastinr) * hpmul / audiostate.freq;
        hplastinl = out[0];
        hplastinr = out[1];
        out[0] = lplastoutl = lplastoutl + (hplastoutl - lplastoutl) * lpmul / audiostate.freq;
        out[1] = lplastoutr = lplastoutr + (hplastoutr - lplastoutr) * lpmul / audiostate.freq;
        outl[i] += out[0] * mix / 256;
        outr[i] += out[1] * mix / 256;
        in[0] += out[0] * feedback / 256;
        in[1] += out[1] * feedback / 256;
        if (in[0] < -32768) in[0] = -32768;
        else if (in[0] > 32767) in[0] = 32767;
        if (in[1] < -32768) in[1] = -32768;
        else if (in[1] > 32767) in[1] = 32767;
        audiostate.env.reverb.buf[0][head] = in[0];
        audiostate.env.reverb.buf[1][head] = in[1];
        head = (head + 1) % audiostate.env.reverb.size;
        tail = (tail + 1) % audiostate.env.reverb.size;
    }
    audiostate.env.reverb.head = head;
    if (lplastoutl < -32768) lplastoutl = -32768;
    else if (lplastoutl > 32767) lplastoutl = 32767;
    if (lplastoutr < -32768) lplastoutr = -32768;
    else if (lplastoutr > 32767) lplastoutr = 32767;
    if (hplastoutl < -32768) hplastoutl = -32768;
    else if (hplastoutl > 32767) hplastoutl = 32767;
    if (hplastoutr < -32768) hplastoutr = -32768;
    else if (hplastoutr > 32767) hplastoutr = 32767;
    if (hplastinl < -32768) hplastinl = -32768;
    else if (hplastinl > 32767) hplastinl = 32767;
    if (hplastinr < -32768) hplastinr = -32768;
    else if (hplastinr > 32767) hplastinr = 32767;
    audiostate.env.reverb.lpfilt.lastout[0] = lplastoutl;
    audiostate.env.reverb.lpfilt.lastout[1] = lplastoutr;
    audiostate.env.reverb.hpfilt.lastout[0] = hplastoutl;
    audiostate.env.reverb.hpfilt.lastout[1] = hplastoutr;
    audiostate.env.reverb.hpfilt.lastin[0] = hplastinl;
    audiostate.env.reverb.hpfilt.lastin[1] = hplastinr;
}
static void doGradReverb(unsigned curlen, unsigned nextlen, int* outl, int* outr) {
    int curmix = roundf(audiostate.env.reverb.mix[0] * 256.0f);
    int nextmix = roundf(audiostate.env.reverb.mix[1] * 256.0f);
    if (!curmix && !nextmix) return;
    int curfeedback = roundf(audiostate.env.reverb.feedback[0] * 256.0f);
    int nextfeedback = roundf(audiostate.env.reverb.feedback[1] * 256.0f);
    register unsigned head;
    unsigned maxlen = (nextlen >= curlen) ? nextlen : curlen;
    if (audiostate.env.reverb.size < maxlen) {
        free(audiostate.env.reverb.buf[0]);
        free(audiostate.env.reverb.buf[1]);
        register unsigned size = maxlen - 1;
        size |= size >> 1;
        size |= size >> 2;
        size |= size >> 4;
        size |= size >> 8;
        size |= size >> 16;
        ++size;
        audiostate.env.reverb.size = size;
        audiostate.env.reverb.buf[0] = calloc(size, sizeof(**audiostate.env.reverb.buf));
        audiostate.env.reverb.buf[1] = calloc(size, sizeof(**audiostate.env.reverb.buf));
        head = 0;
    } else {
        head = audiostate.env.reverb.head;
    }
    register unsigned tail = (head + (audiostate.env.reverb.size - nextlen)) % audiostate.env.reverb.size;
    register float curfiltamount = 1.0f - audiostate.env.reverb.lpfilt.amount[0];
    register float nextfiltamount = 1.0f - audiostate.env.reverb.lpfilt.amount[1];
    int curlpmul = (int)roundf(curfiltamount * curfiltamount * audiostate.freq);
    int nextlpmul = (int)roundf(nextfiltamount * nextfiltamount * audiostate.freq);
    curfiltamount = audiostate.env.reverb.hpfilt.amount[0];
    nextfiltamount = audiostate.env.reverb.hpfilt.amount[1];
    int curhpmul = audiostate.freq - (int)roundf(curfiltamount * curfiltamount * audiostate.freq);
    int nexthpmul = audiostate.freq - (int)roundf(nextfiltamount * nextfiltamount * audiostate.freq);
    int lplastoutl = audiostate.env.reverb.lpfilt.lastout[0];
    int lplastoutr = audiostate.env.reverb.lpfilt.lastout[1];
    int hplastoutl = audiostate.env.reverb.hpfilt.lastout[0];
    int hplastoutr = audiostate.env.reverb.hpfilt.lastout[1];
    int hplastinl = audiostate.env.reverb.hpfilt.lastin[0];
    int hplastinr = audiostate.env.reverb.hpfilt.lastin[1];
    for (register int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
        int mix = (curmix * ii + nextmix * i) / audiostate.audbuf.len;
        int feedback = (curfeedback * ii + nextfeedback * i) / audiostate.audbuf.len;
        int lpmul = (curlpmul * ii + nextlpmul * i) / audiostate.audbuf.len;
        int hpmul = (curhpmul * ii + nexthpmul * i) / audiostate.audbuf.len;
        int in[2] = {outl[i], outr[i]};
        int out[2] = {audiostate.env.reverb.buf[0][tail], audiostate.env.reverb.buf[1][tail]};
        hplastoutl = (hplastoutl + out[0] - hplastinl) * hpmul / audiostate.freq;
        hplastoutr = (hplastoutr + out[1] - hplastinr) * hpmul / audiostate.freq;
        hplastinl = out[0];
        hplastinr = out[1];
        out[0] = lplastoutl = lplastoutl + (hplastoutl - lplastoutl) * lpmul / audiostate.freq;
        out[1] = lplastoutr = lplastoutr + (hplastoutr - lplastoutr) * lpmul / audiostate.freq;
        outl[i] += out[0] * mix / 256;
        outr[i] += out[1] * mix / 256;
        in[0] += out[0] * feedback / 256;
        in[1] += out[1] * feedback / 256;
        if (in[0] < -32768) in[0] = -32768;
        else if (in[0] > 32767) in[0] = 32767;
        if (in[1] < -32768) in[1] = -32768;
        else if (in[1] > 32767) in[1] = 32767;
        audiostate.env.reverb.buf[0][head] = in[0];
        audiostate.env.reverb.buf[1][head] = in[1];
        head = (head + 1) % audiostate.env.reverb.size;
        tail = (tail + 1) % audiostate.env.reverb.size;
    }
    audiostate.env.reverb.head = head;
    if (lplastoutl < -32768) lplastoutl = -32768;
    else if (lplastoutl > 32767) lplastoutl = 32767;
    if (lplastoutr < -32768) lplastoutr = -32768;
    else if (lplastoutr > 32767) lplastoutr = 32767;
    if (hplastoutl < -32768) hplastoutl = -32768;
    else if (hplastoutl > 32767) hplastoutl = 32767;
    if (hplastoutr < -32768) hplastoutr = -32768;
    else if (hplastoutr > 32767) hplastoutr = 32767;
    if (hplastinl < -32768) hplastinl = -32768;
    else if (hplastinl > 32767) hplastinl = 32767;
    if (hplastinr < -32768) hplastinr = -32768;
    else if (hplastinr > 32767) hplastinr = 32767;
    audiostate.env.reverb.lpfilt.lastout[0] = lplastoutl;
    audiostate.env.reverb.lpfilt.lastout[1] = lplastoutr;
    audiostate.env.reverb.hpfilt.lastout[0] = hplastoutl;
    audiostate.env.reverb.hpfilt.lastout[1] = hplastoutr;
    audiostate.env.reverb.hpfilt.lastin[0] = hplastinl;
    audiostate.env.reverb.hpfilt.lastin[1] = hplastinr;
    if (!nextlen) {
        free(audiostate.env.reverb.buf[0]);
        free(audiostate.env.reverb.buf[1]);
    }
}
static void mixsounds(int buf) {
    int* audbuf[2] = {audiostate.audbuf.data[buf][0], audiostate.audbuf.data[buf][1]};
    memset(audbuf[0], 0, audiostate.audbuf.len * sizeof(**audbuf));
    memset(audbuf[1], 0, audiostate.audbuf.len * sizeof(**audbuf));
    // 3D mixing
    {
        int playcount = audiostate.voices.world.playcount;
        if (playcount > audiostate.voices.world.len) {
            playcount = audiostate.voices.world.len;
        } else if (playcount < audiostate.voices.world.len) {
            int si = playcount;
            do {
                struct audiosound_3d* s = &audiostate.voices.world.data[audiostate.voices.world.sortdata[si++]];
                if (!mixsound_3d_fake(s)) stop3DSound_inline(s);
            } while (si < audiostate.voices.world.len);
        }
        for (int si = 0; si < playcount; ++si) {
            struct audiosound_3d* s = &audiostate.voices.world.data[audiostate.voices.world.sortdata[si]];
            if (!((s->maxvol) ? mixsound_3d(s, audbuf) : mixsound_3d_fake(s))) stop3DSound_inline(s);
        }
        playcount = audiostate.voices.worldbg.playcount;
        if (playcount > audiostate.voices.worldbg.len) {
            playcount = audiostate.voices.worldbg.len;
        } else if (playcount < audiostate.voices.worldbg.len) {
            int si = playcount;
            do {
                struct audiosound_3d* s = &audiostate.voices.worldbg.data[audiostate.voices.worldbg.sortdata[si++]];
                if (!mixsound_3d_fake(s)) stop3DSound_inline(s);
            } while (si < audiostate.voices.worldbg.len);
        }
        for (int si = 0; si < playcount; ++si) {
            struct audiosound_3d* s = &audiostate.voices.worldbg.data[audiostate.voices.worldbg.sortdata[si]];
            if (!((s->maxvol) ? mixsound_3d(s, audbuf) : mixsound_3d_fake(s))) stop3DSound_inline(s);
        }
    }
    if (!audiostate.env.reverbchanged) {
        unsigned len = roundf(audiostate.env.reverb.delay[1] * audiostate.freq);
        if (len) doReverb(len, audbuf[0], audbuf[1]);
    } else {
        unsigned curlen = roundf(audiostate.env.reverb.delay[0] * audiostate.freq);
        unsigned nextlen = roundf(audiostate.env.reverb.delay[1] * audiostate.freq);
        if (curlen && nextlen) doGradReverb(curlen, nextlen, audbuf[0], audbuf[1]);
        audiostate.env.reverb.delay[0] = audiostate.env.reverb.delay[1];
        audiostate.env.reverb.feedback[0] = audiostate.env.reverb.feedback[1];
        audiostate.env.reverb.mix[0] = audiostate.env.reverb.mix[1];
        audiostate.env.reverb.lpfilt.amount[0] = audiostate.env.reverb.lpfilt.amount[1];
        audiostate.env.reverb.hpfilt.amount[0] = audiostate.env.reverb.hpfilt.amount[1];
        audiostate.env.reverbchanged = 0;
    }
    // moved ambience up so that filter effects apply
    {
        int oldfade = roundf(audiostate.voices.ambience.oldfade) * 32768.0f;
        int fade = roundf(audiostate.voices.ambience.fade) * 32768.0f;
        if (fade == 32768 && oldfade == 32768) {
            if (!mixsound_2d_fake(&audiostate.voices.ambience.data[0], SOUNDFLAG_LOOP)) stopSound_inline(&audiostate.voices.ambience.data[0]);
        } else if (!mixsound_2d(&audiostate.voices.ambience.data[0], audbuf, SOUNDFLAG_LOOP, 32768 - oldfade, 32768 - fade, audiostate.vol.ambience)) {
            stopSound_inline(&audiostate.voices.ambience.data[0]);
        }
        if (fade == 0 && oldfade == 0) {
            if (!mixsound_2d_fake(&audiostate.voices.ambience.data[1], SOUNDFLAG_LOOP)) stopSound_inline(&audiostate.voices.ambience.data[1]);
        } else if (!mixsound_2d(&audiostate.voices.ambience.data[1], audbuf, SOUNDFLAG_LOOP, oldfade, fade, audiostate.vol.ambience)) {
            stopSound_inline(&audiostate.voices.ambience.data[1]);
        }
    }
    // TODO: proximity voice chat
    if (!audiostate.env.filterchanged) {
        register float amount = audiostate.env.hpfilt.amount[1];
        if (amount > 0.0f) {
            int hpmul = audiostate.freq - (int)roundf(amount * amount * audiostate.freq);
            if (hpmul != audiostate.freq) {
                register int lastoutl = audiostate.env.hpfilt.lastout[0];
                register int lastoutr = audiostate.env.hpfilt.lastout[1];
                register int lastinl = audiostate.env.hpfilt.lastin[0];
                register int lastinr = audiostate.env.hpfilt.lastin[1];
                for (register int i = 0; i < audiostate.audbuf.len; ++i) {
                    lastoutl = (lastoutl + audbuf[0][i] - lastinl) * hpmul / audiostate.freq;
                    lastoutr = (lastoutr + audbuf[1][i] - lastinr) * hpmul / audiostate.freq;
                    lastinl = audbuf[0][i];
                    lastinr = audbuf[1][i];
                    audbuf[0][i] = lastoutl;
                    audbuf[1][i] = lastoutr;
                }
                audiostate.env.hpfilt.lastout[0] = lastoutl;
                audiostate.env.hpfilt.lastout[1] = lastoutr;
                audiostate.env.hpfilt.lastin[0] = lastinl;
                audiostate.env.hpfilt.lastin[1] = lastinr;
            }
        }
        amount = audiostate.env.lpfilt.amount[1];
        if (amount > 0.0f) {
            amount = 1.0f - amount;
            int lpmul = (int)roundf(amount * amount * audiostate.freq);
            if (lpmul != audiostate.freq) {
                register int lastoutl = audiostate.env.lpfilt.lastout[0];
                register int lastoutr = audiostate.env.lpfilt.lastout[1];
                for (register int i = 0; i < audiostate.audbuf.len; ++i) {
                    audbuf[0][i] = lastoutl = lastoutl + (audbuf[0][i] - lastoutl) * lpmul / audiostate.freq;
                    audbuf[1][i] = lastoutr = lastoutr + (audbuf[1][i] - lastoutr) * lpmul / audiostate.freq;
                }
                audiostate.env.lpfilt.lastout[0] = lastoutl;
                audiostate.env.lpfilt.lastout[1] = lastoutr;
            }
        }
    } else {
        register float curamount = audiostate.env.hpfilt.amount[0];
        register float nextamount = audiostate.env.hpfilt.amount[1];
        if (curamount > 0.0f || nextamount > 0.0f) {
            int curhpmul = audiostate.freq - (int)roundf(curamount * curamount * audiostate.freq);
            int nexthpmul = audiostate.freq - (int)roundf(nextamount * nextamount * audiostate.freq);
            if (curhpmul != audiostate.freq || nexthpmul != audiostate.freq) {
                register int lastoutl = audiostate.env.hpfilt.lastout[0];
                register int lastoutr = audiostate.env.hpfilt.lastout[1];
                register int lastinl = audiostate.env.hpfilt.lastin[0];
                register int lastinr = audiostate.env.hpfilt.lastin[1];
                for (register int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                    register int hpmul = (curhpmul * ii + nexthpmul * i) / audiostate.audbuf.len;
                    lastoutl = (lastoutl + audbuf[0][i] - lastinl) * hpmul / audiostate.freq;
                    lastoutr = (lastoutr + audbuf[1][i] - lastinr) * hpmul / audiostate.freq;
                    lastinl = audbuf[0][i];
                    lastinr = audbuf[1][i];
                    audbuf[0][i] = lastoutl;
                    audbuf[1][i] = lastoutr;
                }
                audiostate.env.hpfilt.lastout[0] = lastoutl;
                audiostate.env.hpfilt.lastout[1] = lastoutr;
                audiostate.env.hpfilt.lastin[0] = lastinl;
                audiostate.env.hpfilt.lastin[1] = lastinr;
            }
            if (!(nextamount > 0.0f)) {
                audiostate.env.hpfilt.lastout[0] = 0;
                audiostate.env.hpfilt.lastout[1] = 0;
                audiostate.env.hpfilt.lastin[0] = 0;
                audiostate.env.hpfilt.lastin[1] = 0;
            }
        }
        curamount = audiostate.env.lpfilt.amount[0];
        nextamount = audiostate.env.lpfilt.amount[1];
        if (curamount > 0.0f || nextamount > 0.0f) {
            bool cont = (nextamount > 0.0f);
            #if 0
            curamount -= 1.0f;
            nextamount -= 1.0f;
            #else
            curamount = 1.0f - curamount;
            nextamount = 1.0f - nextamount;
            #endif
            int curlpmul = (int)roundf(curamount * curamount * audiostate.freq);
            int nextlpmul = (int)roundf(nextamount * nextamount * audiostate.freq);
            if (curlpmul != audiostate.freq || nextlpmul != audiostate.freq) {
                register int lastoutl = audiostate.env.lpfilt.lastout[0];
                register int lastoutr = audiostate.env.lpfilt.lastout[1];
                for (register int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                    register int lpmul = (curlpmul * ii + nextlpmul * i) / audiostate.audbuf.len;
                    audbuf[0][i] = lastoutl = lastoutl + (audbuf[0][i] - lastoutl) * lpmul / audiostate.freq;
                    audbuf[1][i] = lastoutr = lastoutr + (audbuf[1][i] - lastoutr) * lpmul / audiostate.freq;
                }
                audiostate.env.lpfilt.lastout[0] = lastoutl;
                audiostate.env.lpfilt.lastout[1] = lastoutr;
            }
            if (!cont) {
                audiostate.env.lpfilt.lastout[0] = 0;
                audiostate.env.lpfilt.lastout[1] = 0;
            }
        }
        audiostate.env.lpfilt.amount[0] = audiostate.env.lpfilt.amount[1];
        audiostate.env.hpfilt.amount[0] = audiostate.env.hpfilt.amount[1];
        audiostate.env.filterchanged = 0;
    }
    // 2D mixing
    if (!mixsound_2d(&audiostate.voices.ui, audbuf, 0, 32768, 32768, audiostate.vol.ui)) stopSound_inline(&audiostate.voices.ui);
    for (int i = 0; i < audiostate.voices.alerts.count; ++i) {
        struct audiosound* s = &audiostate.voices.alerts.data[i].data;
        if (s->rc && !mixsound_2d(s, audbuf, 0, 32768, 32768, audiostate.vol.alerts)) stopSound_inline(&audiostate.voices.ui);
    }
    // TODO: music
    // TODO: voice chat
    int16_t* out = audiostate.audbuf.out[buf];
    if (audiostate.channels < 2) {
        for (register int i = 0; i < audiostate.audbuf.len; ++i) {
            register int sample = (audbuf[0][i] + audbuf[1][i]) / 2;
            if (sample > 32767) sample = 32767;
            else if (sample < -32768) sample = -32768;
            out[i] = sample;
        }
    } else {
        for (register int c = 0; c < 2; ++c) {
            for (register int i = 0; i < audiostate.audbuf.len; ++i) {
                register int sample = audbuf[c][i];
                if (sample > 32767) sample = 32767;
                else if (sample < -32768) sample = -32768;
                out[i * audiostate.channels + c] = sample;
            }
        }
    }
}

static void callback(void* data, uint16_t* stream, int len) {
    (void)data;
    #if DEBUG(3)
    plog(LL_INFO | LF_DEBUG, "Playing %d...", audiostate.audbufindex);
    #endif
    int mixaudbufindex = audiostate.mixaudbufindex;
    if (mixaudbufindex == (audiostate.audbufindex + 3) % 4 || mixaudbufindex < 0) {
        #if DEBUG(2)
        plog(LL_INFO | LF_DEBUG, "Mixer is beind!");
        #endif
        memset(stream, 0, len);
    } else {
        int samples = len / sizeof(*stream) / audiostate.channels;
        if (audiostate.audbuf.len == samples) {
            memcpy(stream, audiostate.audbuf.out[audiostate.audbufindex % 2], len);
        } else {
            plog(LL_WARN | LF_DEBUG, "Mismatch between buffer length (%d) and requested samples (%d)", audiostate.audbuf.len, samples);
        }
        #if DEBUG(3)
        plog(LL_INFO | LF_DEBUG, "Finished playing %d", audiostate.audbufindex);
        #endif
        audiostate.audbufindex = (audiostate.audbufindex + 1) % 4;
    }
}

static void setSoundData(struct audiosound* s, struct rc_sound* rc) {
    s->rc = rc;
    switch ((uint8_t)rc->format) {
        case RC_SOUND_FRMT_VORBIS: {
            s->vorbis = stb_vorbis_open_memory(rc->data, rc->size, NULL, NULL);
            //s->audbuf.off = 0;
            s->audbuf.len = audiostate.audbuflen;
            //s->audbuf.data[0] = malloc(s->audbuf.len * sizeof(**s->audbuf.data));
            //s->audbuf.data[1] = malloc(s->audbuf.len * sizeof(**s->audbuf.data));
            //stb_vorbis_get_samples_short(s->vorbis, 2, s->audbuf.data, s->audbuf.len);
            s->audbuf.data[0] = NULL;
            s->audbuf.data[1] = NULL;
        } break;
        #ifdef PSRC_USEMINIMP3
        case RC_SOUND_FRMT_MP3: {
            s->mp3 = malloc(sizeof(*s->mp3));
            mp3dec_ex_open_buf(s->mp3, rc->data, rc->size, MP3D_SEEK_TO_SAMPLE);
            //s->audbuf.off = 0;
            s->audbuf.len = audiostate.audbuflen;
            //s->audbuf.data_mp3 = malloc(s->audbuf.len * rc->channels * sizeof(*s->audbuf.data_mp3));
            //mp3dec_ex_read(s->mp3, s->audbuf.data_mp3, s->audbuf.len);
            s->audbuf.data_mp3 = NULL;
        } break;
        #endif
    }
    s->offset = 0;
    s->frac = 0;
}

static inline int updsnds_cmp(struct audiosound_3d* s1, struct audiosound_3d* s2) {
    if (!s1->data.rc) {
        if (!s2->data.rc) return 0;
        return 1;
    }
    if (!s2->data.rc) return -1;
    return s2->maxvol - s1->maxvol;
}
static void updsnds_world(struct audiovoicegroup_world* g) {
    int glen = g->len;
    if (!glen) return;
    struct audiosound_3d* s = g->data;
    int* sortdata = g->sortdata;
    {
        int si = 0;
        while (1) {
            if (s->data.rc && !s->fxchanged) {
                s->fx[0] = s->fx[1];
                s->fxchanged = true;
                calc3DSoundFx(s);
            }
            ++si;
            if (si == glen) break;
            ++s;
        }
    }
    s = g->data;
    if (glen == 1) {
        sortdata[0] = 0;
    } else {
        for (int i = 0; i < g->size; ++i) {
            sortdata[i] = i;
        }
        struct qssi {
            int l, r;
        };
        int sp = 0;
        struct qssi sdata[16];
        #define qs_swap(a, b) {\
            int qs_swap_tmp = sortdata[(a)];\
            sortdata[(a)] = sortdata[(b)];\
            sortdata[(b)] = qs_swap_tmp;\
        }
        const int lim = 7;
        while (1) {
            struct qssi d = {0, glen};
            struct qssi t;
            if (d.l - d.r > lim) {
                t.l = d.l + 1;
                t.r = d.r - 1;
                {
                    int m = (d.l + d.r) / 2;
                    qs_swap(m, d.l);
                }
                if (updsnds_cmp(&s[sortdata[t.l]], &s[sortdata[t.r]]) > 0) {
                    qs_swap(t.l, t.r);
                }
                if (updsnds_cmp(&s[sortdata[d.l]], &s[sortdata[t.r]]) > 0) {
                    qs_swap(d.l, t.r);
                }
                if (updsnds_cmp(&s[sortdata[t.l]], &s[sortdata[d.l]]) > 0) {
                    qs_swap(t.l, d.l);
                }
                while (1) {
                    do {
                        ++t.l;
                    } while (updsnds_cmp(&s[sortdata[t.l]], &s[sortdata[d.l]]) < 0);
                    do {
                        --t.r;
                    } while (updsnds_cmp(&s[sortdata[t.r]], &s[sortdata[d.l]]) > 0);
                    if (t.l > t.r) break;
                    qs_swap(t.l, t.r);
                }
                qs_swap(d.l, t.r);
                if (t.r - d.l > d.r - t.l) {
                    sdata[sp++] = (struct qssi){d.l, t.r};
                    d.l = t.l;
                } else {
                    sdata[sp++] = (struct qssi){t.l, d.r};
                    d.r = t.r;
                }
            } else {
                for (t.r = d.l, t.l = t.r + 1; t.l < d.r; t.r = t.l, ++t.l) {
                    while (updsnds_cmp(&s[sortdata[t.r]], &s[sortdata[t.r + 1]]) > 0) {
                        qs_swap(t.r, t.r + 1);
                        if (t.r == d.l) break;
                        --t.r;
                    }
                }
                if (sp > 0) d = sdata[--sp];
                else break;
            }
        }
        #undef qs_swap
    }
}
void updateSounds(float framemult) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (audiostate.voices.ambience.oldfade != audiostate.voices.ambience.fade) audiostate.voices.ambience.oldfade = audiostate.voices.ambience.fade;
    if (audiostate.voices.ambience.index) {
        if (audiostate.voices.ambience.fade == 1.0f) {
            if (audiostate.voices.ambience.queue) {
                audiostate.voices.ambience.index = 0;
                struct audiosound* s = &audiostate.voices.ambience.data[0];
                if (audiostate.voices.ambience.queue != s->rc) {
                    if (s->rc) stopSound_inline(s);
                    setSoundData(s, audiostate.voices.ambience.queue);
                }
                audiostate.voices.ambience.queue = NULL;
            }
        } else {
            audiostate.voices.ambience.fade += framemult;
            if (audiostate.voices.ambience.fade > 1.0f) audiostate.voices.ambience.fade = 1.0f;
        }
    } else {
        if (audiostate.voices.ambience.fade == 0.0f) {
            if (audiostate.voices.ambience.queue) {
                audiostate.voices.ambience.index = 1;
                struct audiosound* s = &audiostate.voices.ambience.data[1];
                if (audiostate.voices.ambience.queue != s->rc) {
                    if (s->rc) stopSound_inline(s);
                    setSoundData(s, audiostate.voices.ambience.queue);
                }
                audiostate.voices.ambience.queue = NULL;
            }
        } else {
            audiostate.voices.ambience.fade -= framemult;
            if (audiostate.voices.ambience.fade < 0.0f) audiostate.voices.ambience.fade = 0.0f;
        }
    }
    audiostate.cam.rotradx = audiostate.cam.rot[0] * (float)M_PI / 180.0f;
    audiostate.cam.rotrady = audiostate.cam.rot[1] * -(float)M_PI / 180.0f;
    audiostate.cam.rotradz = audiostate.cam.rot[2] * (float)M_PI / 180.0f;
    audiostate.cam.sinx = sinf(audiostate.cam.rotradx);
    audiostate.cam.cosx = cosf(audiostate.cam.rotradx);
    audiostate.cam.siny = sinf(audiostate.cam.rotrady);
    audiostate.cam.cosy = cosf(audiostate.cam.rotrady);
    audiostate.cam.sinz = sinf(audiostate.cam.rotradz);
    audiostate.cam.cosz = cosf(audiostate.cam.rotradz);
    updsnds_world(&audiostate.voices.world);
    updsnds_world(&audiostate.voices.worldbg);
    if (audiostate.usecallback) {
        while (audiostate.mixaudbufindex != (audiostate.audbufindex + 1) % 4 || audiostate.mixaudbufindex < 0) {
            int mixbufi = (audiostate.mixaudbufindex + 1) % 4;
            #if DEBUG(3)
            plog(LL_INFO | LF_DEBUG, "Mixing %d...", mixbufi);
            #endif
            mixsounds(mixbufi % 2);
            #if DEBUG(3)
            plog(LL_INFO | LF_DEBUG, "Finished mixing %d", mixbufi);
            #endif
            audiostate.mixaudbufindex = mixbufi;
        }
    } else {
        #ifndef PSRC_USESDL1
        uint32_t qs = SDL_GetQueuedAudioSize(audiostate.output);
        if (qs < audiostate.audbuf.outsize * audiostate.outbufcount) {
            int count = (audiostate.audbuf.outsize * audiostate.outbufcount - qs) / audiostate.audbuf.outsize;
            for (int i = 0; i < count; ++i) {
                mixsounds(0);
                SDL_QueueAudio(audiostate.output, audiostate.audbuf.out[0], audiostate.audbuf.outsize);
            }
        }
        #endif
    }
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}

int newAudioEmitter(int max, unsigned bg, ... /*soundfx*/) {
    #ifndef PSRC_NOMT
    acquireReadAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) {
        #ifndef PSRC_NOMT
        releaseReadAccess(&audiostate.lock);
        #endif
        return -1;
    }
    int index = -1;
    for (int i = 0; i < audiostate.emitters.len; ++i) {
        if (audiostate.emitters.data[i].max == -1) {
            index = i;
            break;
        }
    }
    #ifndef PSRC_NOMT
    readToWriteAccess(&audiostate.lock);
    #endif
    if (index == -1) {
        if (audiostate.emitters.len == audiostate.emitters.size) {
            audiostate.emitters.size *= 2;
            audiostate.emitters.data = realloc(audiostate.emitters.data, audiostate.emitters.size);
        }
        index = audiostate.emitters.len++;
    }
    struct audioemitter* e = &audiostate.emitters.data[index];
    *e = (struct audioemitter){
        .max = max,
        .bg = bg,
        .vol[0] = 1.0f,
        .vol[1] = 1.0f,
        .speed = 1.0f,
        .range = 50.0f
    };
    va_list args;
    va_start(args, bg);
    enum soundfx fx;
    while ((fx = va_arg(args, int)) != SOUNDFXENUM__END) {
        switch ((uint8_t)fx) {
            case SOUNDFXENUM_VOL:
                e->vol[0] = va_arg(args, double);
                e->vol[1] = va_arg(args, double);
                break;
            case SOUNDFXENUM_SPEED:
                e->speed = va_arg(args, double);
                break;
            case SOUNDFXENUM_POS:
                e->pos[0] = va_arg(args, double);
                e->pos[1] = va_arg(args, double);
                e->pos[2] = va_arg(args, double);
                break;
            case SOUNDFXENUM_RANGE:
                e->range = va_arg(args, double);
                break;
            case SOUNDFXENUM_LPFILT:
                e->lpfilt = va_arg(args, double);
                break;
            case SOUNDFXENUM_HPFILT:
                e->hpfilt = va_arg(args, double);
                break;
        }
    }
    va_end(args);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
    return index;
}

void deleteAudioEmitter(int ei) {
    if (ei < 0) return;
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) {
        #ifndef PSRC_NOMT
        releaseWriteAccess(&audiostate.lock);
        #endif
        return;
    }
    struct audioemitter* e = &audiostate.emitters.data[ei];
    if (e->bg) {
        for (int si = 0; si < audiostate.voices.worldbg.len; ++si) {
            struct audiosound_3d* s = &audiostate.voices.worldbg.data[si];
            if (s->data.rc && s->emitter == ei) stop3DSound_inline(s);
        }
    } else {
        for (int si = 0; si < audiostate.voices.world.len; ++si) {
            struct audiosound_3d* s = &audiostate.voices.world.data[si];
            if (s->data.rc && s->emitter == ei) stop3DSound_inline(s);
        }
    }
    e->max = -1;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}

void pauseAudioEmitter(int ei, bool p) {
    if (ei < 0) return;
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) {
        #ifndef PSRC_NOMT
        releaseWriteAccess(&audiostate.lock);
        #endif
        return;
    }
    audiostate.emitters.data[ei].paused = p;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}

void stopAudioEmitter(int ei) {
    if (ei < 0) return;
    #ifndef PSRC_NOMT
    acquireReadAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) {
        #ifndef PSRC_NOMT
        releaseReadAccess(&audiostate.lock);
        #endif
        return;
    }
    struct audioemitter* e = &audiostate.emitters.data[ei];
    if (e->bg) {
        for (int si = 0; si < audiostate.voices.worldbg.len; ++si) {
            struct audiosound_3d* s = &audiostate.voices.worldbg.data[si];
            if (s->data.rc && s->emitter == ei) {
                #ifndef PSRC_NOMT
                readToWriteAccess(&audiostate.lock);
                #endif
                stop3DSound_inline(s);
                #ifndef PSRC_NOMT
                releaseWriteAccess(&audiostate.lock);
                #endif
            }
        }
    } else {
        for (int si = 0; si < audiostate.voices.world.len; ++si) {
            struct audiosound_3d* s = &audiostate.voices.world.data[si];
            if (s->data.rc && s->emitter == ei) {
                #ifndef PSRC_NOMT
                readToWriteAccess(&audiostate.lock);
                #endif
                stop3DSound_inline(s);
                #ifndef PSRC_NOMT
                releaseWriteAccess(&audiostate.lock);
                #endif
            }
        }
    }
    #ifndef PSRC_NOMT
    releaseReadAccess(&audiostate.lock);
    #endif
}

void editAudioEmitter(int ei, unsigned immediate, ...) {
    if (ei < 0) return;
    #ifndef PSRC_NOMT
    acquireReadAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) {
        #ifndef PSRC_NOMT
        releaseReadAccess(&audiostate.lock);
        #endif
        return;
    }
    struct audioemitter* e = &audiostate.emitters.data[ei];
    va_list args;
    va_start(args, immediate);
    enum soundfx fx;
    while ((fx = va_arg(args, int)) != SOUNDFXENUM__END) {
        switch ((uint8_t)fx) {
            case SOUNDFXENUM_VOL:
                e->vol[0] = va_arg(args, double);
                e->vol[1] = va_arg(args, double);
                break;
            case SOUNDFXENUM_SPEED:
                e->speed = va_arg(args, double);
                break;
            case SOUNDFXENUM_POS:
                e->pos[0] = va_arg(args, double);
                e->pos[1] = va_arg(args, double);
                e->pos[2] = va_arg(args, double);
                break;
            case SOUNDFXENUM_RANGE:
                e->range = va_arg(args, double);
                break;
            case SOUNDFXENUM_LPFILT:
                e->lpfilt = va_arg(args, double);
                break;
            case SOUNDFXENUM_HPFILT:
                e->hpfilt = va_arg(args, double);
                break;
        }
    }
    va_end(args);
    if (e->bg) {
        for (int si = 0; si < audiostate.voices.worldbg.len; ++si) {
            struct audiosound_3d* s = &audiostate.voices.worldbg.data[si];
            if (s->data.rc && s->emitter == ei) {
                #ifndef PSRC_NOMT
                readToWriteAccess(&audiostate.lock);
                #endif
                if (!immediate && !s->fxchanged) {
                    s->fx[0] = s->fx[1];
                    s->fxchanged = true;
                }
                calc3DSoundFx(s);
                #ifndef PSRC_NOMT
                writeToReadAccess(&audiostate.lock);
                #endif
            }
        }
    } else {
        for (int si = 0; si < audiostate.voices.world.len; ++si) {
            struct audiosound_3d* s = &audiostate.voices.world.data[si];
            if (s->data.rc && s->emitter == ei) {
                #ifndef PSRC_NOMT
                readToWriteAccess(&audiostate.lock);
                #endif
                if (!immediate && !s->fxchanged) {
                    s->fx[0] = s->fx[1];
                    s->fxchanged = true;
                }
                calc3DSoundFx(s);
                #ifndef PSRC_NOMT
                writeToReadAccess(&audiostate.lock);
                #endif
            }
        }
    }
    #ifndef PSRC_NOMT
    releaseReadAccess(&audiostate.lock);
    #endif
}

void playSound(int ei, struct rc_sound* rc, unsigned f, ...) {
    if (ei < 0) return;
    #ifndef PSRC_NOMT
    acquireReadAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) {
        #ifndef PSRC_NOMT
        releaseReadAccess(&audiostate.lock);
        #endif
        return;
    }
    struct audiosound_3d* s;
    {
        struct audiovoicegroup_world* g;
        struct audioemitter* e = &audiostate.emitters.data[ei];
        if (e->max && e->uses == e->max) {
            #ifndef PSRC_NOMT
            releaseReadAccess(&audiostate.lock);
            #endif
            return;
        }
        s = NULL;
        g = (e->bg) ? &audiostate.voices.worldbg : &audiostate.voices.world;
        int glen = g->len;
        for (int i = 0; i < glen; ++i) {
            struct audiosound_3d* tmps = &g->data[i];
            if (!tmps->data.rc) {
                s = tmps;
                #ifndef PSRC_NOMT
                readToWriteAccess(&audiostate.lock);
                #endif
                goto found;
            }
        }
        #ifndef PSRC_NOMT
        readToWriteAccess(&audiostate.lock);
        #endif
        if (glen == g->size) {
            g->size = g->size * 3 / 2;
            g->data = realloc(g->data, g->size * sizeof(*g->data));
        }
        s = &g->data[glen];
        ++g->len;
    }
    found:;
    lockRc(rc);
    *s = (struct audiosound_3d){
        .emitter = ei,
        .flags = f,
        .vol[0] = 1.0f,
        .vol[1] = 1.0f,
        .speed = 1.0f,
        .range = 1.0f
    };
    setSoundData(&s->data, rc);
    va_list args;
    va_start(args, f);
    enum soundfx fx;
    while ((fx = va_arg(args, int)) != SOUNDFXENUM__END) {
        switch ((uint8_t)fx) {
            case SOUNDFXENUM_VOL:
                s->vol[0] = va_arg(args, double);
                s->vol[1] = va_arg(args, double);
                break;
            case SOUNDFXENUM_SPEED:
                s->speed = va_arg(args, double);
                break;
            case SOUNDFXENUM_POS:
                s->pos[0] = va_arg(args, double);
                s->pos[1] = va_arg(args, double);
                s->pos[2] = va_arg(args, double);
                break;
            case SOUNDFXENUM_RANGE:
                s->range = va_arg(args, double);
                break;
            case SOUNDFXENUM_LPFILT:
                s->lpfilt = va_arg(args, double);
                break;
            case SOUNDFXENUM_HPFILT:
                s->hpfilt = va_arg(args, double);
                break;
        }
    }
    va_end(args);
    calc3DSoundFx(s);
    s->fxoff = s->fx[1].posoff;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void playUISound(struct rc_sound* rc) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (audiostate.voices.ui.rc) stopSound_inline(&audiostate.voices.ui);
    lockRc(rc);
    setSoundData(&audiostate.voices.ui, rc);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void setAmbientSound(struct rc_sound* rc) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (audiostate.voices.ambience.queue) unlockRc(audiostate.voices.ambience.queue);
    lockRc(rc);
    audiostate.voices.ambience.queue = rc;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}

void editSoundEnv(enum soundenv env, ...) {
    if (env == SOUNDENVENUM__END) return;
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) {
        #ifndef PSRC_NOMT
        releaseWriteAccess(&audiostate.lock);
        #endif
        return;
    }
    va_list args;
    va_start(args, env);
    do {
        switch ((uint8_t)env) {
            case SOUNDENVENUM_LPFILT:
                audiostate.env.lpfilt.amount[1] = va_arg(args, double);
                audiostate.env.filterchanged = 1;
                break;
            case SOUNDENVENUM_HPFILT:
                audiostate.env.hpfilt.amount[1] = va_arg(args, double);
                audiostate.env.filterchanged = 1;
                break;
            case SOUNDENVENUM_REVERB_DELAY:
                audiostate.env.reverb.delay[1] = va_arg(args, double);
                audiostate.env.reverbchanged = 1;
                break;
            case SOUNDENVENUM_REVERB_FEEDBACK:
                audiostate.env.reverb.feedback[1] = va_arg(args, double);
                audiostate.env.reverbchanged = 1;
                break;
            case SOUNDENVENUM_REVERB_MIX:
                audiostate.env.reverb.mix[1] = va_arg(args, double);
                audiostate.env.reverbchanged = 1;
                break;
            case SOUNDENVENUM_REVERB_LPFILT:
                audiostate.env.reverb.lpfilt.amount[1] = va_arg(args, double);
                audiostate.env.reverbchanged = 1;
                break;
            case SOUNDENVENUM_REVERB_HPFILT:
                audiostate.env.reverb.hpfilt.amount[1] = va_arg(args, double);
                audiostate.env.reverbchanged = 1;
                break;
        }
    } while ((env = va_arg(args, int)) != SOUNDENVENUM__END);
    va_end(args);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}

void updateAudioConfig(enum audioopt opt, ...) {
    va_list args;
    va_start(args, opt);
    while (1) {
        switch (opt) {
            case AUDIOOPT_END: {
                goto ret;
            } break;
            case AUDIOOPT_FREQ: {
                va_arg(args, int); // ignored for now
            } break;
            case AUDIOOPT_VOICES: {
                va_arg(args, int); // ignored for now
            } break;
        }
        opt = va_arg(args, int);
    }
    ret:;
    va_end(args);
}

bool initAudio(void) {
    #ifndef PSRC_NOMT
    if (!createAccessLock(&audiostate.lock)) return false;
    #endif
    if (SDL_Init(SDL_INIT_AUDIO)) {
        plog(LL_CRIT | LF_FUNCLN, "Failed to init audio: %s", SDL_GetError());
        return false;
    }
    return true;
}

bool startAudio(void) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    char* tmp = cfg_getvar(&config, "Audio", "disable");
    if (tmp) {
        bool disable = strbool(tmp, false);
        free(tmp);
        if (disable) {
            audiostate.valid = false;
            #ifndef PSRC_NOMT
            releaseWriteAccess(&audiostate.lock);
            #endif
            plog(LL_INFO, "Audio disabled");
            return true;
        }
    }
    SDL_AudioSpec inspec;
    SDL_AudioSpec outspec;
    inspec.format = AUDIO_S16SYS;
    inspec.channels = 2;
    #ifndef PSRC_USESDL1
    #ifndef PSRC_NOMT
    tmp = cfg_getvar(&config, "Audio", "callback");
    if (tmp) {
        audiostate.usecallback = strbool(tmp, false);
        free(tmp);
    } else {
        audiostate.usecallback = false;
    }
    #else
    audiostate.usecallback = false;
    #endif
    #else
    audiostate.usecallback = true;
    #endif
    #ifndef PSRC_USESDL1
    int flags = 0;
    #endif
    tmp = cfg_getvar(&config, "Audio", "freq");
    if (tmp) {
        inspec.freq = atoi(tmp);
        free(tmp);
    } else {
        #if PLATFORM == PLAT_3DS || PLATFORM == PLAT_DREAMCAST
        inspec.freq = 11025;
        #elif PLATFORM == PLAT_NXDK
        inspec.freq = 22050;
        #else
        inspec.freq = 44100;
        #if !defined(PSRC_USESDL1) && PLATFORM != PLAT_EMSCR
        flags = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE;
        #endif
        #endif
    }
    tmp = cfg_getvar(&config, "Audio", "buffer");
    if (tmp) {
        inspec.samples = atoi(tmp);
        free(tmp);
    } else {
        inspec.samples = inspec.freq * 4 / 11025 * 32;
        #if !defined(PSRC_USESDL1) && PLATFORM != PLAT_3DS && PLATFORM != PLAT_DREAMCAST && PLATFORM != PLAT_NXDK
        flags |= SDL_AUDIO_ALLOW_SAMPLES_CHANGE;
        #endif
    }
    #ifndef PSRC_USESDL1
    typedef SDL_AudioCallback audcb;
    #else
    typedef void (*audcb)(void*, uint8_t*, int);
    #endif
    inspec.callback = (audiostate.usecallback) ? (audcb)callback : NULL;
    inspec.userdata = NULL;
    bool success;
    #ifndef PSRC_USESDL1
    SDL_AudioDeviceID output = SDL_OpenAudioDevice(NULL, false, &inspec, &outspec, flags);
    if (output > 0) {
        success = true;
    } else {
        inspec.channels = 1;
        success = ((output = SDL_OpenAudioDevice(NULL, false, &inspec, &outspec, flags)) > 0);
    }
    #else
    success = (SDL_OpenAudio(&inspec, &outspec) != -1);
    #endif
    if (success) {
        #ifndef PSRC_USESDL1
        audiostate.output = output;
        plog(LL_INFO, "Audio info (device id %d):", (int)output);
        #else
        plog(LL_INFO, "Audio info:");
        #endif
        plog(LL_INFO, "  Frequency: %d", outspec.freq);
        plog(LL_INFO, "  Channels: %d (%s)", outspec.channels, (outspec.channels == 1) ? "mono" : "stereo");
        plog(LL_INFO, "  Samples: %d", (int)outspec.samples);
        audiostate.freq = outspec.freq;
        audiostate.channels = outspec.channels;
        audiostate.audbuf.len = outspec.samples;
        audiostate.audbuf.data[0][0] = malloc(outspec.samples * sizeof(***audiostate.audbuf.data));
        audiostate.audbuf.data[0][1] = malloc(outspec.samples * sizeof(***audiostate.audbuf.data));
        if (audiostate.usecallback) {
            audiostate.audbuf.data[1][0] = malloc(outspec.samples * sizeof(***audiostate.audbuf.data));
            audiostate.audbuf.data[1][1] = malloc(outspec.samples * sizeof(***audiostate.audbuf.data));
        }
        audiostate.audbuf.outsize = outspec.samples * sizeof(**audiostate.audbuf.out) * outspec.channels;
        audiostate.audbuf.out[0] = malloc(audiostate.audbuf.outsize);
        if (audiostate.usecallback) {
            audiostate.audbuf.out[1] = malloc(audiostate.audbuf.outsize);
        }
        {
            int voicecount;
            tmp = cfg_getvar(&config, "Audio", "worldvoices");
            if (tmp) {
                voicecount = atoi(tmp);
                free(tmp);
                if (voicecount < 0) voicecount = 1;
            } else {
                voicecount = 32;
            }
            audiostate.voices.world.len = 0;
            audiostate.voices.world.size = 32;
            audiostate.voices.world.playcount = voicecount;
            audiostate.voices.world.data = malloc(audiostate.voices.world.size * sizeof(*audiostate.voices.world.data));
            audiostate.voices.world.sortdata = malloc(audiostate.voices.world.size * sizeof(*audiostate.voices.world.sortdata));
            tmp = cfg_getvar(&config, "Audio", "worldbgvoices");
            if (tmp) {
                voicecount = atoi(tmp);
                free(tmp);
                if (voicecount < 0) voicecount = 1;
            } else {
                voicecount = 16;
            }
            audiostate.voices.worldbg.len = 0;
            audiostate.voices.worldbg.size = 16;
            audiostate.voices.worldbg.playcount = voicecount;
            audiostate.voices.worldbg.data = malloc(audiostate.voices.worldbg.size * sizeof(*audiostate.voices.worldbg.data));
            audiostate.voices.worldbg.sortdata = malloc(audiostate.voices.worldbg.size * sizeof(*audiostate.voices.worldbg.sortdata));
            tmp = cfg_getvar(&config, "Audio", "alertvoices");
            if (tmp) {
                voicecount = atoi(tmp);
                free(tmp);
                if (voicecount < 0) voicecount = 1;
            } else {
                voicecount = 8;
            }
            audiostate.voices.alerts.next = 0;
            audiostate.voices.alerts.count = voicecount;
            audiostate.voices.alerts.data = malloc(voicecount * sizeof(*audiostate.voices.alerts.data));
            for (int i = 0; i < voicecount; ++i) {
                audiostate.voices.alerts.data[i].data.rc = NULL;
            }
        }
        audiostate.voices.ambience.queue = NULL;
        audiostate.voices.ambience.data[0].rc = NULL;
        audiostate.voices.ambience.data[1].rc = NULL;
        audiostate.voices.ambience.oldfade = 1.0;
        audiostate.voices.ambience.fade = 1.0;
        audiostate.voices.ambience.index = 1;
        audiostate.emitters.len = 0;
        audiostate.emitters.size = 4;
        audiostate.emitters.data = malloc(audiostate.emitters.size * sizeof(*audiostate.emitters.data));
        tmp = cfg_getvar(&config, "Audio", "outbufcount");
        if (tmp) {
            audiostate.outbufcount = atoi(tmp);
            free(tmp);
        } else {
            audiostate.outbufcount = 2;
        }
        tmp = cfg_getvar(&config, "Audio", "decodebuf");
        if (tmp) {
            audiostate.audbuflen = atoi(tmp);
            free(tmp);
        } else {
            audiostate.audbuflen = 4096;
        }
        tmp = cfg_getvar(&config, "Audio", "decodewhole");
        if (tmp) {
            #if PLATFORM != PLAT_NXDK
            audiostate.soundrcopt.decodewhole = strbool(tmp, true);
            #else
            audiostate.soundrcopt.decodewhole = strbool(tmp, false);
            #endif
            free(tmp);
        } else {
            #if PLATFORM != PLAT_NXDK
            audiostate.soundrcopt.decodewhole = true;
            #else
            audiostate.soundrcopt.decodewhole = false;
            #endif
        }
        audiostate.audbufindex = 0;
        audiostate.mixaudbufindex = -1;
        {
            char s[4];
            #define sa_getvolcfg(v, d) ((cfg_getvarto(&config, "Audio", (v), s, sizeof(s))) ? atoi(s) : (d))
            audiostate.vol.master = sa_getvolcfg("mastervol", 100);
            audiostate.vol.ui = sa_getvolcfg("uivol", 100);
            audiostate.vol.alerts = sa_getvolcfg("alertvol", 100);
            audiostate.vol.world = sa_getvolcfg("worldvol", 100);
            audiostate.vol.worldbg = sa_getvolcfg("worldbgvol", 100);
            audiostate.vol.ambience = sa_getvolcfg("ambiencevol", 50);
            audiostate.vol.music = sa_getvolcfg("musicvol", 50);
            audiostate.vol.voice = sa_getvolcfg("voicevol", 100);
            #undef sa_getvolcfg
        }
        audiostate.valid = true;
        #ifndef PSRC_USESDL1
        SDL_PauseAudioDevice(output, 0);
        #else
        SDL_PauseAudio(0);
        #endif
    } else {
        audiostate.valid = false;
        plog(LL_ERROR, "Failed to get audio info for default output device; audio disabled: %s", SDL_GetError());
    }
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
    return true;
}

void stopAudio(void) {
    if (audiostate.valid) {
        #ifndef PSRC_NOMT
        acquireWriteAccess(&audiostate.lock);
        #endif
        audiostate.valid = false;
        #ifndef PSRC_USESDL1
        SDL_PauseAudioDevice(audiostate.output, 1);
        SDL_CloseAudioDevice(audiostate.output);
        #else
        SDL_PauseAudio(1);
        SDL_CloseAudio();
        #endif
        if (audiostate.voices.ui.rc) stopSound_inline(&audiostate.voices.ui);
        for (int i = 0; i < audiostate.voices.alerts.count; ++i) {
            struct audiosound* s = &audiostate.voices.alerts.data[i].data;
            if (s->rc) stopSound_inline(s);
        }
        free(audiostate.voices.alerts.data);
        for (int i = 0; i < audiostate.voices.world.len; ++i) {
            struct audiosound* s = &audiostate.voices.world.data[i].data;
            if (s->rc) stopSound_inline(s);
        }
        free(audiostate.voices.world.data);
        for (int i = 0; i < audiostate.voices.worldbg.len; ++i) {
            struct audiosound* s = &audiostate.voices.worldbg.data[i].data;
            if (s->rc) stopSound_inline(s);
        }
        free(audiostate.voices.worldbg.data);
        if (audiostate.voices.ambience.queue) unlockRc(audiostate.voices.ambience.queue);
        if (audiostate.voices.ambience.data[0].rc) stopSound_inline(&audiostate.voices.ambience.data[0]);
        if (audiostate.voices.ambience.data[1].rc) stopSound_inline(&audiostate.voices.ambience.data[1]);
        free(audiostate.audbuf.data[0][0]);
        free(audiostate.audbuf.data[0][1]);
        if (audiostate.usecallback) {
            free(audiostate.audbuf.data[1][0]);
            free(audiostate.audbuf.data[1][1]);
        }
        free(audiostate.audbuf.out[0]);
        if (audiostate.usecallback) {
            free(audiostate.audbuf.out[1]);
        }
        #ifndef PSRC_NOMT
        releaseWriteAccess(&audiostate.lock);
        #endif
    }
}

void quitAudio(void) {
    #ifndef PSRC_NOMT
    destroyAccessLock(&audiostate.lock);
    #endif
}
