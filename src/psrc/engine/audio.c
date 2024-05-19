#include "audio.h"

#include "../common/logging.h"
#include "../common/string.h"
#include "../common/time.h"

#include "../common.h"
#include "../debug.h"

#include <inttypes.h>
#include <stdarg.h>
#include <math.h>

struct audiostate audiostate;

static inline __attribute__((always_inline)) void getvorbisat_fillbuf(struct audiosound* s, struct audiosound_audbuf* ab) {
    stb_vorbis_seek(s->vorbis, ab->off);
    stb_vorbis_get_samples_short(s->vorbis, 2, ab->data, ab->len);
}
static inline __attribute__((always_inline)) void getvorbisat_prepbuf(struct audiosound* s, struct audiosound_audbuf* ab, int pos, int len) {
    if (pos >= ab->off + ab->len) {
        int oldoff = ab->off;
        ab->off = pos;
        if (ab->off + ab->len >= len) {
            ab->off = len - ab->len;
            if (ab->off < 0) ab->off = 0;
        }
        if (ab->off != oldoff) {
            s->audbuf.off = ab->off;
            getvorbisat_fillbuf(s, ab);
        }
    } else if (pos < ab->off) {
        int oldoff = ab->off;
        ab->off = pos - ab->len / 2 + 1;
        if (ab->off < 0) ab->off = 0;
        if (ab->off != oldoff) {
            s->audbuf.off = ab->off;
            getvorbisat_fillbuf(s, ab);
        }
    }
}
static inline __attribute__((always_inline)) void getvorbisat(struct audiosound* s, struct audiosound_audbuf* ab, int pos, int len, int* out_l, int* out_r) {
    if (pos < 0 || pos >= len) {
        *out_l = 0;
        *out_r = 0;
        return;
    }
    getvorbisat_prepbuf(s, ab, pos, len);
    *out_l = ab->data[0][pos - ab->off];
    *out_r = ab->data[1][pos - ab->off];
}

#ifdef PSRC_USEMINIMP3
static inline __attribute__((always_inline)) void getmp3at_fillbuf(struct audiosound* s, struct audiosound_audbuf* ab, int ch) {
    mp3dec_ex_seek(s->mp3, ab->off * ch);
    mp3dec_ex_read(s->mp3, ab->data_mp3, ab->len * ch);
}
static inline __attribute__((always_inline)) void getmp3at_prepbuf(struct audiosound* s, struct audiosound_audbuf* ab, int pos, int len, int ch) {
    if (pos >= ab->off + ab->len) {
        int oldoff = ab->off;
        ab->off = pos;
        if (ab->off + ab->len >= len) {
            ab->off = len - ab->len;
            if (ab->off < 0) ab->off = 0;
        }
        if (ab->off != oldoff) {
            s->audbuf.off = ab->off;
            getmp3at_fillbuf(s, ab, ch);
        }
    } else if (pos < ab->off) {
        int oldoff = ab->off;
        ab->off = pos - ab->len / 2 + 1;
        if (ab->off < 0) ab->off = 0;
        if (ab->off != oldoff) {
            s->audbuf.off = ab->off;
            getmp3at_fillbuf(s, ab, ch);
        }
    }
}
static inline __attribute__((always_inline)) void getmp3at(struct audiosound* s, struct audiosound_audbuf* ab, int pos, int len, int* out_l, int* out_r) {
    if (pos < 0 || pos >= len) {
        *out_l = 0;
        *out_r = 0;
        return;
    }
    int channels = s->rc->channels;
    getmp3at_prepbuf(s, ab, pos, len, channels);
    *out_l = ab->data_mp3[(pos - ab->off) * channels];
    *out_r = ab->data_mp3[(pos - ab->off) * channels + 1];
}
#endif

static inline void calc3DSoundFx(struct audiosound_3d* s) {
    struct audioemitter* e = &audiostate.emitters.data[s->emitter];
    s->fx[1].speedmul = roundf(s->speed * 256.0f);
    float vol[2] = {s->vol[0] * e->vol[0], s->vol[1] * e->vol[1]};
    float pos[3];
    pos[0] = e->pos[0] + s->pos[0] - audiostate.cam.pos[0];
    pos[1] = e->pos[1] + s->pos[1] - audiostate.cam.pos[1];
    pos[2] = e->pos[2] + s->pos[2] - audiostate.cam.pos[2];
    float range = e->range * s->range;
    if (isnormal(range)) {
        float dist = sqrt(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
        if (isnormal(dist)) {
            if (dist < range) {
                float loudness = 1.0f - (dist / range);
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
    releaseResource(s->rc);
    s->rc = NULL;
}
static inline void stop3DSound_inline(struct audiosound_3d* s) {
    stopSound_inline(&s->data);
    --audiostate.emitters.data[s->emitter].uses;
}

static inline __attribute__((always_inline)) void interpfx(struct audiosound_fx* sfx, struct audiosound_fx* fx, int i, int ii, int samples) {
    fx->posoff = (sfx[0].posoff * ii + sfx[1].posoff * i) / samples;
    fx->speedmul = (sfx[0].speedmul * ii + sfx[1].speedmul * i) / samples;
    fx->volmul[0] = (sfx[0].volmul[0] * ii + sfx[1].volmul[0] * i) / samples;
    fx->volmul[1] = (sfx[0].volmul[1] * ii + sfx[1].volmul[1] * i) / samples;
}
#define MIXSOUND3D_CALCPOS() {\
    int mul = ((fx.posoff - fxoff + 1) * freq) * fx.speedmul;\
    fxoff = fx.posoff;\
    offset += mul / div;\
    frac += mul % div;\
    offset += frac / div;\
    frac %= div;\
    pos = offset;\
}
#define MIXSOUND3D_BODY(c) {\
    if (s->fxchanged) {\
        if (stereo) {\
            int i = 0, ii = audiostate.audbuf.len;\
            while (1) {\
                interpfx(sfx, &fx, i, ii, audiostate.audbuf.len);\
                MIXSOUND3D_CALCPOS();\
                c\
                int tmp = (l + r) / 2;\
                audbuf[0][i] += tmp * fx.volmul[0] / 32768;\
                audbuf[1][i] += tmp * fx.volmul[1] / 32768;\
                ++i;\
                if (i == audiostate.audbuf.len) {if (MIXSOUND_POSCHECK) return false; break;};\
                --ii;\
            }\
        } else {\
            int i = 0, ii = audiostate.audbuf.len;\
            while (1) {\
                interpfx(sfx, &fx, i, ii, audiostate.audbuf.len);\
                MIXSOUND3D_CALCPOS();\
                c\
                audbuf[0][i] += l * fx.volmul[0] / 32768;\
                audbuf[1][i] += r * fx.volmul[1] / 32768;\
                ++i;\
                if (i == audiostate.audbuf.len) {if (MIXSOUND_POSCHECK) return false; break;};\
                --ii;\
            }\
        }\
    } else {\
        if (stereo) {\
            int i = 0;\
            while (1) {\
                MIXSOUND3D_CALCPOS();\
                c\
                int tmp = (l + r) / 2;\
                audbuf[0][i] += tmp * fx.volmul[0] / 32768;\
                audbuf[1][i] += tmp * fx.volmul[1] / 32768;\
                ++i;\
                if (i == audiostate.audbuf.len) {if (MIXSOUND_POSCHECK) return false; break;};\
            }\
        } else {\
            int i = 0;\
            while (1) {\
                MIXSOUND3D_CALCPOS();\
                c\
                audbuf[0][i] += l * fx.volmul[0] / 32768;\
                audbuf[1][i] += r * fx.volmul[1] / 32768;\
                ++i;\
                if (i == audiostate.audbuf.len) {if (MIXSOUND_POSCHECK) return false; break;}\
            }\
        }\
    }\
}
#define MIXSOUND3D_INTERPBODY(c) {\
    if (frac) {\
        pos2 = pos + 1;\
        c\
        uint8_t tmpfrac = frac / 16 / audiostate.freq;\
        uint8_t ifrac = 16 - tmpfrac;\
        l = (l * ifrac + l2 * tmpfrac) / 16;\
        r = (r * ifrac + r2 * tmpfrac) / 16;\
    }\
}
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
    if (s->fxchanged) {
        //puts("fxchanged");
        sfx[0] = s->fx[0];
        sfx[1] = s->fx[1];
        sfx[0].volmul[0] = sfx[0].volmul[0] * audiostate.vol.master / 100;
        sfx[0].volmul[1] = sfx[0].volmul[1] * audiostate.vol.master / 100;
        sfx[1].volmul[0] = sfx[1].volmul[0] * audiostate.vol.master / 100;
        sfx[1].volmul[1] = sfx[1].volmul[1] * audiostate.vol.master / 100;
    } else {
        fx = s->fx[1];
        fx.volmul[0] = fx.volmul[0] * audiostate.vol.master / 100;
        fx.volmul[1] = fx.volmul[1] * audiostate.vol.master / 100;
    }
    #if 0
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
    #endif
    int div = audiostate.freq * 256;
    long pos;
    long pos2;
    bool stereo = rc->stereo;
    int l, r;
    int l2, r2;
    switch (rc->format) {
        case RC_SOUND_FRMT_VORBIS: {
            struct audiosound_audbuf ab = s->data.audbuf;
            if (flags & SOUNDFLAG_LOOP) {
                if (flags & SOUNDFLAG_WRAP) {
                    #define MIXSOUND_POSCHECK 0
                    MIXSOUND3D_BODY (
                        pos = ((pos % len) + len) % len;
                        getvorbisat(&s->data, &ab, pos, len, &l, &r);
                        MIXSOUND3D_INTERPBODY (
                            pos2 = ((pos2 % len) + len) % len;
                            getvorbisat(&s->data, &ab, pos2, len, &l2, &r2);
                        )
                    )
                    #undef MIXSOUND_POSCHECK
                } else {
                    #define MIXSOUND_POSCHECK (pos < 0)
                    MIXSOUND3D_BODY (
                        if (pos >= 0) pos %= len;
                        getvorbisat(&s->data, &ab, pos, len, &l, &r);
                        MIXSOUND3D_INTERPBODY (
                            if (pos2 >= 0) pos2 %= len;
                            getvorbisat(&s->data, &ab, pos2, len, &l2, &r2);
                        )
                    )
                    #undef MIXSOUND_POSCHECK
                }
            } else {
                #define MIXSOUND_POSCHECK (pos >= len || pos < 0)
                MIXSOUND3D_BODY (
                    getvorbisat(&s->data, &ab, pos, len, &l, &r);
                    MIXSOUND3D_INTERPBODY (
                        getvorbisat(&s->data, &ab, pos2, len, &l2, &r2);
                    )
                )
                #undef MIXSOUND_POSCHECK
            }
        } break;
        #ifdef PSRC_USEMINIMP3
        case RC_SOUND_FRMT_MP3: {
            struct audiosound_audbuf ab = s->data.audbuf;
            if (flags & SOUNDFLAG_LOOP) {
                if (flags & SOUNDFLAG_WRAP) {
                    #define MIXSOUND_POSCHECK 0
                    MIXSOUND3D_BODY (
                        pos = ((pos % len) + len) % len;
                        getmp3at(&s->data, &ab, pos, len, &l, &r);
                        MIXSOUND3D_INTERPBODY (
                            pos2 = ((pos2 % len) + len) % len;
                            getmp3at(&s->data, &ab, pos2, len, &l2, &r2);
                        )
                    )
                    #undef MIXSOUND_POSCHECK
                } else {
                    #define MIXSOUND_POSCHECK (pos < 0)
                    MIXSOUND3D_BODY (
                        if (pos >= 0) pos %= len;
                        getmp3at(&s->data, &ab, pos, len, &l, &r);
                        MIXSOUND3D_INTERPBODY (
                            if (pos2 >= 0) pos2 %= len;
                            getmp3at(&s->data, &ab, pos2, len, &l2, &r2);
                        )
                    )
                    #undef MIXSOUND_POSCHECK
                }
            } else {
                #define MIXSOUND_POSCHECK (pos >= len || pos < 0)
                MIXSOUND3D_BODY (
                    getmp3at(&s->data, &ab, pos, len, &l, &r);
                    MIXSOUND3D_INTERPBODY (
                        getmp3at(&s->data, &ab, pos2, len, &l2, &r2);
                    )
                )
                #undef MIXSOUND_POSCHECK
            }
        } break;
        #endif
        case RC_SOUND_FRMT_WAV: {
            union {
                uint8_t* ptr;
                uint8_t* i8;
                int16_t* i16;
            } data;
            data.ptr = rc->data;
            bool is8bit = rc->is8bit;
            int channels = rc->channels;
            if (flags & SOUNDFLAG_LOOP) {
                if (flags & SOUNDFLAG_WRAP) {
                    #define MIXSOUND_POSCHECK 0
                    if (is8bit) {
                        MIXSOUND3D_BODY (
                            pos = ((pos % len) + len) % len;
                            l = data.i8[pos * channels] - 128;
                            l = l * 256 + (l + 128);
                            r = data.i8[pos * channels + stereo] - 128;
                            r = r * 256 + (r + 128);
                            MIXSOUND3D_INTERPBODY (
                                pos2 = ((pos2 % len) + len) % len;
                                l2 = data.i8[pos2 * channels] - 128;
                                l2 = l2 * 256 + (l2 + 128);
                                r2 = data.i8[pos2 * channels + stereo] - 128;
                                r2 = r2 * 256 + (r2 + 128);
                            )
                        )
                    } else {
                        MIXSOUND3D_BODY (
                            pos = ((pos % len) + len) % len;
                            l = data.i16[pos * channels];
                            r = data.i16[pos * channels + stereo];
                            MIXSOUND3D_INTERPBODY (
                                pos2 = ((pos2 % len) + len) % len;
                                l2 = data.i16[pos2 * channels];
                                r2 = data.i16[pos2 * channels + stereo];
                            )
                        )
                    }
                    #undef MIXSOUND_POSCHECK
                } else {
                    #define MIXSOUND_POSCHECK (pos < 0)
                    if (is8bit) {
                        MIXSOUND3D_BODY (
                            if (pos >= 0) {
                                pos %= len;
                                l = data.i8[pos * channels] - 128;
                                l = l * 256 + (l + 128);
                                r = data.i8[pos * channels + stereo] - 128;
                                r = r * 256 + (r + 128);
                                MIXSOUND3D_INTERPBODY (
                                    if (pos2 >= 0) {
                                        pos2 %= len;
                                        l2 = data.i8[pos2 * channels] - 128;
                                        l2 = l2 * 256 + (l2 + 128);
                                        r2 = data.i8[pos2 * channels + stereo] - 128;
                                        r2 = r2 * 256 + (r2 + 128);
                                    } else {
                                        l2 = 0;
                                        r2 = 0;
                                    }
                                )
                            } else {
                                l = 0;
                                r = 0;
                            }
                        )
                    } else {
                        MIXSOUND3D_BODY (
                            if (pos >= 0) {
                                pos %= len;
                                l = data.i16[pos * channels];
                                r = data.i16[pos * channels + stereo];
                                MIXSOUND3D_INTERPBODY (
                                    if (pos2 >= 0) {
                                        pos2 %= len;
                                        l2 = data.i16[pos2 * channels];
                                        r2 = data.i16[pos2 * channels + stereo];
                                    } else {
                                        l2 = 0;
                                        r2 = 0;
                                    }
                                )
                            } else {
                                l = 0;
                                r = 0;
                            }
                        )
                    }
                    #undef MIXSOUND_POSCHECK
                }
            } else {
                #define MIXSOUND_POSCHECK (pos >= len || pos < 0)
                if (is8bit) {
                    MIXSOUND3D_BODY (
                        if (pos >= 0 && pos < len) {
                            l = data.i8[pos * channels] - 128;
                            l = l * 256 + (l + 128);
                            r = data.i8[pos * channels + stereo] - 128;
                            r = r * 256 + (r + 128);
                            MIXSOUND3D_INTERPBODY (
                                if (pos2 >= 0 && pos2 < len) {
                                    l2 = data.i8[pos2 * channels] - 128;
                                    l2 = l2 * 256 + (l2 + 128);
                                    r2 = data.i8[pos2 * channels + stereo] - 128;
                                    r2 = r2 * 256 + (r2 + 128);
                                } else {
                                    l2 = 0;
                                    r2 = 0;
                                }
                            )
                        } else {
                            l = 0;
                            r = 0;
                        }
                    )
                } else {
                    MIXSOUND3D_BODY (
                        if (pos >= 0 && pos < len) {
                            l = data.i16[pos * channels];
                            r = data.i16[pos * channels + stereo];
                            MIXSOUND3D_INTERPBODY (
                                if (pos2 >= 0 && pos2 < len) {
                                    l2 = data.i16[pos2 * channels];
                                    r2 = data.i16[pos2 * channels + stereo];
                                } else {
                                    l2 = 0;
                                    r2 = 0;
                                }
                            )
                        } else {
                            l = 0;
                            r = 0;
                        }
                    )
                }
                #undef MIXSOUND_POSCHECK
            }
        } break;
    }
    #ifndef PSRC_NOMT
    readToWriteAccess(&audiostate.lock);
    #endif
    if (s->fxchanged) s->fxchanged = false;
    s->data.offset = offset;
    s->data.frac = frac;
    s->fxoff = fxoff;
    #ifndef PSRC_NOMT
    writeToReadAccess(&audiostate.lock);
    #endif
    return true;
}
#define MIXSOUND3DFAKE_BODY(c) {\
    if (s->fxchanged) {\
        int i = 0, ii = audiostate.audbuf.len;\
        while (1) {\
            interpfx(sfx, &fx, i, ii, audiostate.audbuf.len);\
            MIXSOUND3D_CALCPOS();\
            c\
            ++i;\
            if (i == audiostate.audbuf.len) {if (MIXSOUND_POSCHECK) return false; break;}\
            --ii;\
        }\
    } else {\
        int i = 0;\
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
    int div = audiostate.freq * 256;
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
    #ifndef PSRC_NOMT
    readToWriteAccess(&audiostate.lock);
    #endif
    if (s->fxchanged) s->fxchanged = false;
    s->data.offset = offset;
    s->data.frac = frac;
    s->fxoff = fxoff;
    #ifndef PSRC_NOMT
    writeToReadAccess(&audiostate.lock);
    #endif
    return true;
}
#define MIXSOUND2D_CALCPOS() {\
    offset += freq / audiostate.freq;\
    frac += freq % audiostate.freq;\
    offset += frac / audiostate.freq;\
    frac %= audiostate.freq;\
    pos = offset;\
}
#define MIXSOUND2D_BODY(c) {\
    if (oldvol != newvol) {\
        int i = 0, ii = audiostate.audbuf.len;\
        while (1) {\
            vol = (oldvol * ii + newvol * i) / audiostate.audbuf.len;\
            MIXSOUND2D_CALCPOS();\
            c\
            audbuf[0][i] += l * vol / 32768;\
            audbuf[1][i] += r * vol / 32768;\
            ++i;\
            if (i == audiostate.audbuf.len) {if (MIXSOUND_POSCHECK) return false; break;}\
            --ii;\
        }\
    } else {\
        int i = 0;\
        while (1) {\
            MIXSOUND2D_CALCPOS();\
            c\
            audbuf[0][i] += l * newvol / 32768;\
            audbuf[1][i] += r * newvol / 32768;\
            ++i;\
            if (i == audiostate.audbuf.len) {if (MIXSOUND_POSCHECK) return false; break;}\
        }\
    }\
}
#define MIXSOUND2D_INTERPBODY(c) {\
    if (frac) {\
        pos2 = pos + 1;\
        c\
        uint8_t tmpfrac = frac * 16 / audiostate.freq;\
        uint8_t ifrac = 16 - tmpfrac;\
        l = (l * ifrac + l2 * tmpfrac) / 16;\
        r = (r * ifrac + r2 * tmpfrac) / 16;\
    }\
}
static bool mixsound_2d(struct audiosound* s, int** audbuf, uint8_t flags, int oldvol, int newvol, int volmul) {
    struct rc_sound* rc = s->rc;
    if (!rc) return true;
    int len = rc->len;
    if (!len) return true;
    int freq = rc->freq;
    long offset = s->offset;
    int frac = s->frac;
    long pos;
    long pos2;
    bool stereo = rc->stereo;
    int l, r;
    int l2, r2;
    oldvol = oldvol * volmul * audiostate.vol.master / 10000;
    newvol = newvol * volmul * audiostate.vol.master / 10000;
    int vol;
    switch (rc->format) {
        case RC_SOUND_FRMT_VORBIS: {
            struct audiosound_audbuf ab = s->audbuf;
            if (flags & SOUNDFLAG_LOOP) {
                #define MIXSOUND_POSCHECK 0
                MIXSOUND2D_BODY (
                    if (pos >= 0) pos %= len;
                    getvorbisat(s, &ab, pos, len, &l, &r);
                    MIXSOUND2D_INTERPBODY (
                        if (pos2 >= 0) pos2 %= len;
                        getvorbisat(s, &ab, pos2, len, &l2, &r2);
                    )
                )
                #undef MIXSOUND_POSCHECK
            } else {
                #define MIXSOUND_POSCHECK (pos >= len)
                MIXSOUND2D_BODY (
                    getvorbisat(s, &ab, pos, len, &l, &r);
                    MIXSOUND2D_INTERPBODY (
                        getvorbisat(s, &ab, pos2, len, &l2, &r2);
                    )
                )
                #undef MIXSOUND_POSCHECK
            }
        } break;
        #ifdef PSRC_USEMINIMP3
        case RC_SOUND_FRMT_MP3: {
            struct audiosound_audbuf ab = s->audbuf;
            if (flags & SOUNDFLAG_LOOP) {
                #define MIXSOUND_POSCHECK 0
                MIXSOUND2D_BODY (
                    if (pos >= 0) pos %= len;
                    getmp3at(s, &ab, pos, len, &l, &r);
                    MIXSOUND2D_INTERPBODY (
                        if (pos2 >= 0) pos2 %= len;
                        getmp3at(s, &ab, pos2, len, &l2, &r2);
                    )
                )
                #undef MIXSOUND_POSCHECK
            } else {
                #define MIXSOUND_POSCHECK (pos >= len)
                MIXSOUND2D_BODY (
                    getmp3at(s, &ab, pos, len, &l, &r);
                    MIXSOUND2D_INTERPBODY (
                        getmp3at(s, &ab, pos2, len, &l2, &r2);
                    )
                )
                #undef MIXSOUND_POSCHECK
            }
        } break;
        #endif
        case RC_SOUND_FRMT_WAV: {
            union {
                uint8_t* ptr;
                uint8_t* i8;
                int16_t* i16;
            } data;
            data.ptr = rc->data;
            bool is8bit = rc->is8bit;
            int channels = rc->channels;
            if (flags & SOUNDFLAG_LOOP) {
                #define MIXSOUND_POSCHECK 0
                if (is8bit) {
                    MIXSOUND2D_BODY (
                        if (pos >= 0) {
                            pos %= len;
                            l = data.i8[pos * channels] - 128;
                            l = l * 256 + (l + 128);
                            r = data.i8[pos * channels + stereo] - 128;
                            r = r * 256 + (r + 128);
                            MIXSOUND2D_INTERPBODY (
                                if (pos2 >= 0) {
                                    pos2 %= len;
                                    l2 = data.i8[pos2 * channels] - 128;
                                    l2 = l2 * 256 + (l2 + 128);
                                    r2 = data.i8[pos2 * channels + stereo] - 128;
                                    r2 = r2 * 256 + (r2 + 128);
                                } else {
                                    l2 = 0;
                                    r2 = 0;
                                }
                            )
                        } else {
                            l = 0;
                            r = 0;
                        }
                    )
                } else {
                    MIXSOUND2D_BODY (
                        if (pos >= 0) {
                            pos %= len;
                            l = data.i16[pos * channels];
                            r = data.i16[pos * channels + stereo];
                            MIXSOUND2D_INTERPBODY (
                                if (pos2 >= 0) {
                                    pos2 %= len;
                                    l2 = data.i16[pos2 * channels];
                                    r2 = data.i16[pos2 * channels + stereo];
                                } else {
                                    l2 = 0;
                                    r2 = 0;
                                }
                            )
                        } else {
                            l = 0;
                            r = 0;
                        }
                    )
                }
                #undef MIXSOUND_POSCHECK
            } else {
                #define MIXSOUND_POSCHECK (pos >= len)
                if (is8bit) {
                    MIXSOUND2D_BODY (
                        if (pos >= 0 && pos < len) {
                            l = data.i8[pos * channels] - 128;
                            l = l * 256 + (l + 128);
                            r = data.i8[pos * channels + stereo] - 128;
                            r = r * 256 + (r + 128);
                            MIXSOUND2D_INTERPBODY (
                                if (pos2 >= 0 && pos2 < len) {
                                    l2 = data.i8[pos2 * channels] - 128;
                                    l2 = l2 * 256 + (l2 + 128);
                                    r2 = data.i8[pos2 * channels + stereo] - 128;
                                    r2 = r2 * 256 + (r2 + 128);
                                } else {
                                    l2 = 0;
                                    r2 = 0;
                                }
                            )
                        } else {
                            l = 0;
                            r = 0;
                        }
                    )
                } else {
                    MIXSOUND2D_BODY (
                        if (pos >= 0 && pos < len) {
                            l = data.i16[pos * channels];
                            r = data.i16[pos * channels + stereo];
                            MIXSOUND2D_INTERPBODY (
                                if (pos2 >= 0 && pos2 < len) {
                                    l2 = data.i16[pos2 * channels];
                                    r2 = data.i16[pos2 * channels + stereo];
                                } else {
                                    l2 = 0;
                                    r2 = 0;
                                }
                            )
                        } else {
                            l = 0;
                            r = 0;
                        }
                    )
                }
                #undef MIXSOUND_POSCHECK
            }
        } break;
    }
    #ifndef PSRC_NOMT
    readToWriteAccess(&audiostate.lock);
    #endif
    s->offset = offset;
    s->frac = frac;
    #ifndef PSRC_NOMT
    writeToReadAccess(&audiostate.lock);
    #endif
    return true;
}
#define MIXSOUND2DFAKE_CALCPOS() {\
    offset += freq / audiostate.freq;\
    frac += freq % audiostate.freq;\
    offset += frac / audiostate.freq;\
    frac %= audiostate.freq;\
    pos = offset;\
}
#define MIXSOUND2DFAKE_BODY(c) {\
    int i = 0;\
    while (1) {\
        MIXSOUND2DFAKE_CALCPOS();\
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
    #ifndef PSRC_NOMT
    readToWriteAccess(&audiostate.lock);
    #endif
    s->offset = offset;
    s->frac = frac;
    #ifndef PSRC_NOMT
    writeToReadAccess(&audiostate.lock);
    #endif
    return true;
}
static void mixsounds(int buf) {
    int* audbuf[2] = {audiostate.audbuf.data[buf][0], audiostate.audbuf.data[buf][1]};
    memset(audbuf[0], 0, audiostate.audbuf.len * sizeof(*audbuf[0]));
    memset(audbuf[1], 0, audiostate.audbuf.len * sizeof(*audbuf[1]));
    #ifndef PSRC_NOMT
    acquireReadAccess(&audiostate.lock);
    #endif
    // 3D mixing
    {
        int playcount = audiostate.voices.world.playcount;
        if (playcount > audiostate.voices.world.len) {
            playcount = audiostate.voices.world.len;
        } else if (playcount < audiostate.voices.world.len) {
            int si = playcount;
            do {
                struct audiosound_3d* s = &audiostate.voices.world.data[audiostate.voices.world.sortdata[si++]];
                if (!mixsound_3d_fake(s)) {
                    #ifndef PSRC_NOMT
                    readToWriteAccess(&audiostate.lock);
                    #endif
                    stop3DSound_inline(s);
                    #ifndef PSRC_NOMT
                    writeToReadAccess(&audiostate.lock);
                    #endif
                }
            } while (si < audiostate.voices.world.len);
        }
        for (int si = 0; si < playcount; ++si) {
            struct audiosound_3d* s = &audiostate.voices.world.data[audiostate.voices.world.sortdata[si]];
            if (!mixsound_3d(s, audbuf)) {
                #ifndef PSRC_NOMT
                readToWriteAccess(&audiostate.lock);
                #endif
                stop3DSound_inline(s);
                #ifndef PSRC_NOMT
                writeToReadAccess(&audiostate.lock);
                #endif
            }
        }
        playcount = audiostate.voices.worldbg.playcount;
        if (playcount > audiostate.voices.worldbg.len) {
            playcount = audiostate.voices.worldbg.len;
        } else if (playcount < audiostate.voices.worldbg.len) {
            int si = playcount;
            do {
                struct audiosound_3d* s = &audiostate.voices.worldbg.data[audiostate.voices.worldbg.sortdata[si++]];
                if (!mixsound_3d_fake(s)) {
                    #ifndef PSRC_NOMT
                    readToWriteAccess(&audiostate.lock);
                    #endif
                    stop3DSound_inline(s);
                    #ifndef PSRC_NOMT
                    writeToReadAccess(&audiostate.lock);
                    #endif
                }
            } while (si < audiostate.voices.worldbg.len);
        }
        for (int si = 0; si < playcount; ++si) {
            struct audiosound_3d* s = &audiostate.voices.worldbg.data[audiostate.voices.worldbg.sortdata[si]];
            if (!mixsound_3d(s, audbuf)) {
                #ifndef PSRC_NOMT
                readToWriteAccess(&audiostate.lock);
                #endif
                stop3DSound_inline(s);
                #ifndef PSRC_NOMT
                writeToReadAccess(&audiostate.lock);
                #endif
            }
        }
    }
    // TODO: proximity voice chat
    // TODO: apply effects to 3D mix output
    // 2D mixing
    if (!mixsound_2d(&audiostate.voices.ui, audbuf, 0, 32768, 32768, audiostate.vol.ui)) {
        #ifndef PSRC_NOMT
        readToWriteAccess(&audiostate.lock);
        #endif
        stopSound_inline(&audiostate.voices.ui);
        #ifndef PSRC_NOMT
        writeToReadAccess(&audiostate.lock);
        #endif
    }
    for (int i = 0; i < audiostate.voices.alerts.count; ++i) {
        struct audiosound* s = &audiostate.voices.alerts.data[i].data;
        if (s->rc && !mixsound_2d(s, audbuf, 0, 32768, 32768, audiostate.vol.alerts)) {
            #ifndef PSRC_NOMT
            readToWriteAccess(&audiostate.lock);
            #endif
            stopSound_inline(&audiostate.voices.ui);
            #ifndef PSRC_NOMT
            writeToReadAccess(&audiostate.lock);
            #endif
        }
    }
    if (audiostate.voices.ambience.fade == 1.0 && audiostate.voices.ambience.oldfade == 1.0) {
        if (!mixsound_2d_fake(&audiostate.voices.ambience.data[0], SOUNDFLAG_LOOP)) {
            #ifndef PSRC_NOMT
            readToWriteAccess(&audiostate.lock);
            #endif
            stopSound_inline(&audiostate.voices.ambience.data[0]);
            #ifndef PSRC_NOMT
            writeToReadAccess(&audiostate.lock);
            #endif
        }
    } else {
        if (!mixsound_2d(&audiostate.voices.ambience.data[0], audbuf, SOUNDFLAG_LOOP,
        (1.0 - audiostate.voices.ambience.oldfade) * 32768.0, (1.0 - audiostate.voices.ambience.fade) * 32768.0, audiostate.vol.ambience)) {
            #ifndef PSRC_NOMT
            readToWriteAccess(&audiostate.lock);
            #endif
            stopSound_inline(&audiostate.voices.ambience.data[0]);
            #ifndef PSRC_NOMT
            writeToReadAccess(&audiostate.lock);
            #endif
        }
    }
    if (audiostate.voices.ambience.fade == 0.0 && audiostate.voices.ambience.oldfade == 0.0) {
        if (!mixsound_2d_fake(&audiostate.voices.ambience.data[1], SOUNDFLAG_LOOP)) {
            #ifndef PSRC_NOMT
            readToWriteAccess(&audiostate.lock);
            #endif
            stopSound_inline(&audiostate.voices.ambience.data[1]);
            #ifndef PSRC_NOMT
            writeToReadAccess(&audiostate.lock);
            #endif
        }
    } else {
        if (!mixsound_2d(&audiostate.voices.ambience.data[1], audbuf, SOUNDFLAG_LOOP,
        audiostate.voices.ambience.oldfade * 32768.0, audiostate.voices.ambience.fade * 32768.0, audiostate.vol.ambience)) {
            #ifndef PSRC_NOMT
            readToWriteAccess(&audiostate.lock);
            #endif
            stopSound_inline(&audiostate.voices.ambience.data[1]);
            #ifndef PSRC_NOMT
            writeToReadAccess(&audiostate.lock);
            #endif
        }
    }
    // TODO: music
    // TODO: voice chat
    #ifndef PSRC_NOMT
    releaseReadAccess(&audiostate.lock);
    #endif
    int16_t* out = audiostate.audbuf.out[buf];
    if (audiostate.channels < 2) {
        for (int i = 0; i < audiostate.audbuf.len; ++i) {
            int sample = (audbuf[0][i] + audbuf[1][i]) / 2;
            if (sample > 32767) sample = 32767;
            else if (sample < -32768) sample = -32768;
            out[i] = sample;
        }
    } else {
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < audiostate.audbuf.len; ++i) {
                int sample = audbuf[c][i];
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
            s->audbuf.off = 0;
            s->audbuf.len = audiostate.audbuflen;
            s->audbuf.data[0] = malloc(s->audbuf.len * sizeof(*s->audbuf.data[0]));
            s->audbuf.data[1] = malloc(s->audbuf.len * sizeof(*s->audbuf.data[1]));
            stb_vorbis_get_samples_short(s->vorbis, 2, s->audbuf.data, s->audbuf.len);
        } break;
        #ifdef PSRC_USEMINIMP3
        case RC_SOUND_FRMT_MP3: {
            s->mp3 = malloc(sizeof(*s->mp3));
            mp3dec_ex_open_buf(s->mp3, rc->data, rc->size, MP3D_SEEK_TO_SAMPLE);
            s->audbuf.off = 0;
            s->audbuf.len = audiostate.audbuflen;
            s->audbuf.data_mp3 = malloc(s->audbuf.len * rc->channels * sizeof(*s->audbuf.data_mp3));
            mp3dec_ex_read(s->mp3, s->audbuf.data_mp3, s->audbuf.len);
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
        if (audiostate.voices.ambience.fade == 1.0) {
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
            if (audiostate.voices.ambience.fade > 1.0) audiostate.voices.ambience.fade = 1.0;
        }
    } else {
        if (audiostate.voices.ambience.fade == 0.0) {
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
            if (audiostate.voices.ambience.fade < 0.0) audiostate.voices.ambience.fade = 0.0;
        }
    }
    audiostate.cam.rotradx = audiostate.cam.rot[0] * (float)M_PI / 180.0f;
    audiostate.cam.rotrady = audiostate.cam.rot[1] * (float)-M_PI / 180.0f;
    audiostate.cam.rotradz = audiostate.cam.rot[2] * (float)M_PI / 180.0f;
    audiostate.cam.sinx = sin(audiostate.cam.rotradx);
    audiostate.cam.cosx = cos(audiostate.cam.rotradx);
    audiostate.cam.siny = sin(audiostate.cam.rotrady);
    audiostate.cam.cosy = cos(audiostate.cam.rotrady);
    audiostate.cam.sinz = sin(audiostate.cam.rotradz);
    audiostate.cam.cosz = cos(audiostate.cam.rotradz);
    updsnds_world(&audiostate.voices.world);
    updsnds_world(&audiostate.voices.worldbg);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
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
}

int newAudioEmitter(int max, bool bg, ... /*soundfx*/) {
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
        .range = 20.0f
    };
    va_list args;
    va_start(args, bg);
    enum soundfx fx;
    while ((fx = va_arg(args, enum soundfx)) != SOUNDFX_END) {
        switch ((uint8_t)fx) {
            case SOUNDFX_VOL:
                e->vol[0] = va_arg(args, double);
                e->vol[1] = va_arg(args, double);
                break;
            case SOUNDFX_SPEED:
                e->speed = va_arg(args, double);
                break;
            case SOUNDFX_POS:
                e->pos[0] = va_arg(args, double);
                e->pos[1] = va_arg(args, double);
                e->pos[2] = va_arg(args, double);
                break;
            case SOUNDFX_RANGE:
                e->range = va_arg(args, double);
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

void editAudioEmitter(int ei, bool immediate, ...) {
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
    while ((fx = va_arg(args, enum soundfx)) != SOUNDFX_END) {
        switch ((uint8_t)fx) {
            case SOUNDFX_VOL:
                e->vol[0] = va_arg(args, double);
                e->vol[1] = va_arg(args, double);
                break;
            case SOUNDFX_SPEED:
                e->speed = va_arg(args, double);
                break;
            case SOUNDFX_POS:
                e->pos[0] = va_arg(args, double);
                e->pos[1] = va_arg(args, double);
                e->pos[2] = va_arg(args, double);
                break;
            case SOUNDFX_RANGE:
                e->range = va_arg(args, double);
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

void playSound(int ei, struct rc_sound* rc, uint8_t f, ...) {
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
    grabResource(rc);
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
    while ((fx = va_arg(args, enum soundfx)) != SOUNDFX_END) {
        switch ((uint8_t)fx) {
            case SOUNDFX_VOL:
                s->vol[0] = va_arg(args, double);
                s->vol[1] = va_arg(args, double);
                break;
            case SOUNDFX_SPEED:
                s->speed = va_arg(args, double);
                break;
            case SOUNDFX_POS:
                s->pos[0] = va_arg(args, double);
                s->pos[1] = va_arg(args, double);
                s->pos[2] = va_arg(args, double);
                break;
            case SOUNDFX_RANGE:
                s->range = va_arg(args, double);
                break;
        }
    }
    va_end(args);
    calc3DSoundFx(s);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void playUISound(struct rc_sound* rc) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (audiostate.voices.ui.rc) stopSound_inline(&audiostate.voices.ui);
    grabResource(rc);
    setSoundData(&audiostate.voices.ui, rc);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void setAmbientSound(struct rc_sound* rc) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (audiostate.voices.ambience.queue) releaseResource(audiostate.voices.ambience.queue);
    grabResource(rc);
    audiostate.voices.ambience.queue = rc;
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
        opt = va_arg(args, enum audioopt);
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
    char* tmp = cfg_getvar(config, "Audio", "disable");
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
    tmp = cfg_getvar(config, "Audio", "callback");
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
    tmp = cfg_getvar(config, "Audio", "freq");
    if (tmp) {
        inspec.freq = atoi(tmp);
        free(tmp);
    } else {
        #if PLATFORM != PLAT_NXDK
        inspec.freq = 44100;
        #ifndef PSRC_USESDL1
        flags = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE;
        #endif
        #else
        inspec.freq = 22050;
        #endif
    }
    tmp = cfg_getvar(config, "Audio", "buffer");
    if (tmp) {
        inspec.samples = atoi(tmp);
        free(tmp);
    } else {
        inspec.samples = 1024;
        #ifndef PSRC_USESDL1
        #if PLATFORM != PLAT_NXDK
        flags |= SDL_AUDIO_ALLOW_SAMPLES_CHANGE;
        #endif
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
        audiostate.audbuf.data[0][0] = malloc(outspec.samples * sizeof(*audiostate.audbuf.data[0][0]));
        audiostate.audbuf.data[0][1] = malloc(outspec.samples * sizeof(*audiostate.audbuf.data[0][1]));
        if (audiostate.usecallback) {
            audiostate.audbuf.data[1][0] = malloc(outspec.samples * sizeof(*audiostate.audbuf.data[1][0]));
            audiostate.audbuf.data[1][1] = malloc(outspec.samples * sizeof(*audiostate.audbuf.data[1][1]));
        }
        audiostate.audbuf.outsize = outspec.samples * sizeof(**audiostate.audbuf.out) * outspec.channels;
        audiostate.audbuf.out[0] = malloc(audiostate.audbuf.outsize);
        if (audiostate.usecallback) {
            audiostate.audbuf.out[1] = malloc(audiostate.audbuf.outsize);
        }
        {
            int voicecount;
            tmp = cfg_getvar(config, "Audio", "worldvoices");
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
            tmp = cfg_getvar(config, "Audio", "worldbgvoices");
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
            tmp = cfg_getvar(config, "Audio", "alertvoices");
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
        tmp = cfg_getvar(config, "Audio", "outbufcount");
        if (tmp) {
            audiostate.outbufcount = atoi(tmp);
            free(tmp);
        } else {
            audiostate.outbufcount = 2;
        }
        tmp = cfg_getvar(config, "Audio", "decodebuf");
        if (tmp) {
            audiostate.audbuflen = atoi(tmp);
            free(tmp);
        } else {
            audiostate.audbuflen = 4096;
        }
        tmp = cfg_getvar(config, "Audio", "decodewhole");
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
            #define sa_getvolcfg(v, d) ((cfg_getvarto(config, "Audio", (v), s, sizeof(s))) ? atoi(s) : (d))
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
        if (audiostate.voices.ambience.queue) releaseResource(audiostate.voices.ambience.queue);
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
