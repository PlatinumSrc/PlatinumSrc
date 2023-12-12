#include "audio.h"

#include "../utils/logging.h"
#include "../utils/string.h"

#include "../common/common.h"
#include "../common/time.h"

#include "../debug.h"

#include "../../cglm/cglm.h"

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
    //printf("<- [%d, %d] [%d]\n", s->audbuf.off, s->audbuf.len, pos);
    *out_l = ab->data[0][pos - ab->off];
    *out_r = ab->data[1][pos - ab->off];
}

static inline __attribute__((always_inline)) void getvorbisat_forcemono(struct audiosound* s, struct audiosound_audbuf* ab, int pos, int len, int* out_l, int* out_r) {
    if (pos < 0 || pos >= len) {
        *out_l = 0;
        *out_r = 0;
        return;
    }
    getvorbisat_prepbuf(s, ab, pos, len);
    int tmp = ((int)ab->data[0][pos - ab->off] + (int)ab->data[1][pos - ab->off]) / 2;
    *out_l = tmp;
    *out_r = tmp;
}

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

static inline __attribute__((always_inline)) void getmp3at_forcemono(struct audiosound* s, struct audiosound_audbuf* ab, int pos, int len, int* out_l, int* out_r) {
    if (pos < 0 || pos >= len) {
        *out_l = 0;
        *out_r = 0;
        return;
    }
    int channels = s->rc->channels;
    getmp3at_prepbuf(s, ab, pos, len, channels);
    int tmp = ((int)ab->data_mp3[(pos - ab->off) * channels] + (int)ab->data_mp3[(pos - ab->off) * channels + 1]) / 2;
    *out_l = tmp;
    *out_r = tmp;
}

static inline __attribute__((always_inline)) void interpfx(struct audiosound_fx* sfx, struct audiosound_fx* fx, int i, int ii, int samples) {
    fx->posoff = (sfx[0].posoff * ii + sfx[1].posoff * i) / samples;
    fx->speedmul = (sfx[0].speedmul * ii + sfx[1].speedmul * i) / samples;
    fx->volmul[0] = (sfx[0].volmul[0] * ii + sfx[1].volmul[0] * i) / samples;
    fx->volmul[1] = (sfx[0].volmul[1] * ii + sfx[1].volmul[1] * i) / samples;
}

static inline __attribute__((always_inline)) int64_t calcpos(struct audiosound_fx* fx, int64_t offset, int64_t i, int64_t freq, int64_t outfreq) {
    return (offset + i * (int64_t)fx->speedmul / 1000) * freq / outfreq + (int64_t)fx->posoff;
}

static inline void calcSoundFX(struct audiosound* s) {
    s->fx[1].speedmul = roundf(s->speed * 1000.0);
    if (s->flags & SOUNDFLAG_POSEFFECT) {
        float vol[2] = {s->vol[0], s->vol[1]};
        float pos[3];
        if (s->flags & SOUNDFLAG_RELPOS) {
            pos[0] = s->pos[0];
            pos[1] = s->pos[1];
            pos[2] = s->pos[2];
        } else {
            pos[0] = s->pos[0] - audiostate.campos[0];
            pos[1] = s->pos[1] - audiostate.campos[1];
            pos[2] = s->pos[2] - audiostate.campos[2];
        }
        float range = s->range;
        if (isnormal(range)) {
            float dist = sqrt(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
            if (isnormal(dist)) {
                if (vol[0] * range >= dist && vol[1] * range >= dist) {
                    float loudness = 1.0 - (dist / range);
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
                        s->fx[1].posoff = roundf(dist * -0.0025 * (float)s->rc->freq);
                    }
                    if (!(s->flags & SOUNDFLAG_RELPOS)) {
                        // TODO: optimize?
                        float rotradx = audiostate.camrot[0] * M_PI / 180.0;
                        float rotrady = audiostate.camrot[1] * -M_PI / 180.0;
                        float rotradz = audiostate.camrot[2] * M_PI / 180.0;
                        glm_vec3_rotate(pos, rotrady, (vec3){0.0, 1.0, 0.0});
                        glm_vec3_rotate(pos, rotradx, (vec3){1.0, 0.0, 0.0});
                        glm_vec3_rotate(pos, rotradz, (vec3){0.0, 0.0, 1.0});
                    }
                    if (pos[2] > 0.0) {
                        pos[0] *= 1.0 - 0.2 * pos[2];
                    } else if (pos[2] < 0.0) {
                        pos[0] *= 1.0 - 0.25 * pos[2];
                        vol[0] *= 1.0 + 0.25 * pos[2];
                        vol[1] *= 1.0 + 0.25 * pos[2];
                    }
                    if (pos[1] > 0.0) {
                        pos[0] *= 1.0 - 0.2 * pos[1];
                        vol[0] *= 1.0 - 0.1 * pos[1];
                        vol[1] *= 1.0 - 0.1 * pos[1];
                    } else if (pos[1] < 0.0) {
                        pos[0] *= 1.0 - 0.2 * pos[1];
                    }
                    if (pos[0] > 0.0) {
                        vol[0] *= 1.0 - pos[0] * 0.75;
                    } else if (pos[0] < 0.0) {
                        vol[1] *= 1.0 + pos[0] * 0.75;
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

#define mixsounds_pre() {\
    if (chfx) interpfx(sfx, &fx, i, ii, audiostate.audbuf.len);\
    pos = calcpos(&fx, offset, i, freq, outfreq);\
}
#define mixsounds_post(l, r) {\
    audbuf[0][i] += l * fx.volmul[0] / 65536;\
    audbuf[1][i] += r * fx.volmul[1] / 65536;\
}
void mixsounds(int buf) {
    int* audbuf[2] = {audiostate.audbuf.data[buf][0], audiostate.audbuf.data[buf][1]};
    memset(audbuf[0], 0, audiostate.audbuf.len * sizeof(*audbuf[0]));
    memset(audbuf[1], 0, audiostate.audbuf.len * sizeof(*audbuf[1]));
    int outfreq = audiostate.freq;
    int tmpbuf[2];
    acquireReadAccess(&audiostate.lock);
    for (int si = 0; si < audiostate.voicecount; ++si) {
        struct audiosound* s = &audiostate.voices[si];
        if (s->id < 0 || s->state.paused) continue;
        struct rc_sound* rc = s->rc;
        uint8_t flags = s->flags;
        bool chfx = s->state.fxchanged;
        struct audiosound_fx sfx[2];
        struct audiosound_fx fx;
        if (chfx) {
            //puts("fxchanged");
            sfx[0] = s->fx[0];
            sfx[1] = s->fx[1];
        } else {
            fx = s->fx[1];
        }
        int64_t offset = s->offset;
        int freq = rc->freq;
        int len = rc->len;
        int64_t pos;
        bool stereo = rc->stereo;
        switch (rc->format) {
            case RC_SOUND_FRMT_VORBIS: {
                struct audiosound_audbuf ab = s->audbuf;
                if (flags & SOUNDFLAG_LOOP) {
                    if (flags & SOUNDFLAG_WRAP) {
                        if (flags & SOUNDFLAG_FORCEMONO && stereo) {
                            for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                mixsounds_pre();
                                pos = ((pos % len) + len) % len;
                                getvorbisat_forcemono(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                                mixsounds_post(tmpbuf[0], tmpbuf[1]);
                            }
                        } else {
                            for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                mixsounds_pre();
                                pos = ((pos % len) + len) % len;
                                getvorbisat(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                                mixsounds_post(tmpbuf[0], tmpbuf[1]);
                            }
                        }
                    } else {
                        if (flags & SOUNDFLAG_FORCEMONO && stereo) {
                            for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                mixsounds_pre();
                                if (pos >= 0) pos %= len;
                                getvorbisat_forcemono(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                                mixsounds_post(tmpbuf[0], tmpbuf[1]);
                            }
                        } else {
                            for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                mixsounds_pre();
                                if (pos >= 0) pos %= len;
                                getvorbisat(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                                mixsounds_post(tmpbuf[0], tmpbuf[1]);
                            }
                        }
                    }
                } else {
                    if (flags & SOUNDFLAG_FORCEMONO && stereo) {
                        for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                            mixsounds_pre();
                            getvorbisat_forcemono(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            mixsounds_post(tmpbuf[0], tmpbuf[1]);
                        }
                    } else {
                        for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                            mixsounds_pre();
                            getvorbisat(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            mixsounds_post(tmpbuf[0], tmpbuf[1]);
                        }
                    }
                }
            } break;
            case RC_SOUND_FRMT_MP3: {
                struct audiosound_audbuf ab = s->audbuf;
                if (flags & SOUNDFLAG_LOOP) {
                    if (flags & SOUNDFLAG_WRAP) {
                        if (flags & SOUNDFLAG_FORCEMONO && stereo) {
                            for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                mixsounds_pre();
                                pos = ((pos % len) + len) % len;
                                getmp3at_forcemono(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                                mixsounds_post(tmpbuf[0], tmpbuf[1]);
                            }
                        } else {
                            for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                mixsounds_pre();
                                pos = ((pos % len) + len) % len;
                                getmp3at(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                                mixsounds_post(tmpbuf[0], tmpbuf[1]);
                            }
                        }
                    } else {
                        if (flags & SOUNDFLAG_FORCEMONO && stereo) {
                            for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                mixsounds_pre();
                                if (pos >= 0) pos %= len;
                                getmp3at_forcemono(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                                mixsounds_post(tmpbuf[0], tmpbuf[1]);
                            }
                        } else {
                            for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                mixsounds_pre();
                                if (pos >= 0) pos %= len;
                                getmp3at(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                                mixsounds_post(tmpbuf[0], tmpbuf[1]);
                            }
                        }
                    }
                } else {
                    if (flags & SOUNDFLAG_FORCEMONO && stereo) {
                        for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                            mixsounds_pre();
                            getmp3at_forcemono(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            mixsounds_post(tmpbuf[0], tmpbuf[1]);
                        }
                    } else {
                        for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                            mixsounds_pre();
                            getmp3at(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            mixsounds_post(tmpbuf[0], tmpbuf[1]);
                        }
                    }
                }
            } break;
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
                        if (flags & SOUNDFLAG_FORCEMONO && stereo) {
                            if (is8bit) {
                                for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                    mixsounds_pre();
                                    pos = ((pos % len) + len) % len;
                                    tmpbuf[0] = data.i8[pos * channels] - 128;
                                    tmpbuf[0] = tmpbuf[0] * 256 + (tmpbuf[0] + 128);
                                    tmpbuf[1] = data.i8[pos * channels + stereo] - 128;
                                    tmpbuf[1] = tmpbuf[1] * 256 + (tmpbuf[1] + 128);
                                    int tmp = (tmpbuf[0] + tmpbuf[1]) / 2;
                                    mixsounds_post(tmp, tmp);
                                }
                            } else {
                                for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                    mixsounds_pre();
                                    pos = ((pos % len) + len) % len;
                                    tmpbuf[0] = data.i16[pos * channels];
                                    tmpbuf[1] = data.i16[pos * channels + stereo];
                                    int tmp = (tmpbuf[0] + tmpbuf[1]) / 2;
                                    mixsounds_post(tmp, tmp);
                                }
                            }
                        } else {
                            if (is8bit) {
                                for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                    mixsounds_pre();
                                    pos = ((pos % len) + len) % len;
                                    tmpbuf[0] = data.i8[pos * channels] - 128;
                                    tmpbuf[0] = tmpbuf[0] * 256 + (tmpbuf[0] + 128);
                                    tmpbuf[1] = data.i8[pos * channels + stereo] - 128;
                                    tmpbuf[1] = tmpbuf[1] * 256 + (tmpbuf[1] + 128);
                                    mixsounds_post(tmpbuf[0], tmpbuf[1]);
                                }
                            } else {
                                for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                    mixsounds_pre();
                                    pos = ((pos % len) + len) % len;
                                    tmpbuf[0] = data.i16[pos * channels];
                                    tmpbuf[1] = data.i16[pos * channels + stereo];
                                    mixsounds_post(tmpbuf[0], tmpbuf[1]);
                                }
                            }
                        }
                    } else {
                        if (flags & SOUNDFLAG_FORCEMONO && stereo) {
                            if (is8bit) {
                                for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                    mixsounds_pre();
                                    if (pos >= 0) {
                                        pos %= len;
                                        tmpbuf[0] = data.i8[pos * channels] - 128;
                                        tmpbuf[0] = tmpbuf[0] * 256 + (tmpbuf[0] + 128);
                                        tmpbuf[1] = data.i8[pos * channels + stereo] - 128;
                                        tmpbuf[1] = tmpbuf[1] * 256 + (tmpbuf[1] + 128);
                                        int tmp = (tmpbuf[0] + tmpbuf[1]) / 2;
                                        mixsounds_post(tmp, tmp);
                                    }
                                }
                            } else {
                                for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                    mixsounds_pre();
                                    if (pos >= 0) {
                                        pos %= len;
                                        tmpbuf[0] = data.i16[pos * channels];
                                        tmpbuf[1] = data.i16[pos * channels + stereo];
                                        int tmp = (tmpbuf[0] + tmpbuf[1]) / 2;
                                        mixsounds_post(tmp, tmp);
                                    }
                                }
                            }
                        } else {
                            if (is8bit) {
                                for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                    mixsounds_pre();
                                    if (pos >= 0) {
                                        pos %= len;
                                        tmpbuf[0] = data.i8[pos * channels] - 128;
                                        tmpbuf[0] = tmpbuf[0] * 256 + (tmpbuf[0] + 128);
                                        tmpbuf[1] = data.i8[pos * channels + stereo] - 128;
                                        tmpbuf[1] = tmpbuf[1] * 256 + (tmpbuf[1] + 128);
                                        mixsounds_post(tmpbuf[0], tmpbuf[1]);
                                    }
                                }
                            } else {
                                for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                    mixsounds_pre();
                                    if (pos >= 0) {
                                        pos %= len;
                                        tmpbuf[0] = data.i16[pos * channels];
                                        tmpbuf[1] = data.i16[pos * channels + stereo];
                                        mixsounds_post(tmpbuf[0], tmpbuf[1]);
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (flags & SOUNDFLAG_FORCEMONO && stereo) {
                        if (is8bit) {
                            for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                mixsounds_pre();
                                if (pos >= 0 && pos < len) {
                                    tmpbuf[0] = data.i8[pos * channels] - 128;
                                    tmpbuf[0] = tmpbuf[0] * 256 + (tmpbuf[0] + 128);
                                    tmpbuf[1] = data.i8[pos * channels + stereo] - 128;
                                    tmpbuf[1] = tmpbuf[1] * 256 + (tmpbuf[1] + 128);
                                    int tmp = (tmpbuf[0] + tmpbuf[1]) / 2;
                                    mixsounds_post(tmp, tmp);
                                }
                            }
                        } else {
                            for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                mixsounds_pre();
                                if (pos >= 0 && pos < len) {
                                    tmpbuf[0] = data.i16[pos * channels];
                                    tmpbuf[1] = data.i16[pos * channels + stereo];
                                    int tmp = (tmpbuf[0] + tmpbuf[1]) / 2;
                                    mixsounds_post(tmp, tmp);
                                }
                            }
                        }
                    } else {
                        if (is8bit) {
                            for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                mixsounds_pre();
                                if (pos >= 0 && pos < len) {
                                    tmpbuf[0] = data.i8[pos * channels] - 128;
                                    tmpbuf[0] = tmpbuf[0] * 256 + (tmpbuf[0] + 128);
                                    tmpbuf[1] = data.i8[pos * channels + stereo] - 128;
                                    tmpbuf[1] = tmpbuf[1] * 256 + (tmpbuf[1] + 128);
                                    mixsounds_post(tmpbuf[0], tmpbuf[1]);
                                }
                            }
                        } else {
                            for (int i = 0, ii = audiostate.audbuf.len; i < audiostate.audbuf.len; ++i, --ii) {
                                mixsounds_pre();
                                if (pos >= 0 && pos < len) {
                                    tmpbuf[0] = data.i16[pos * channels];
                                    tmpbuf[1] = data.i16[pos * channels + stereo];
                                    mixsounds_post(tmpbuf[0], tmpbuf[1]);
                                }
                            }
                        }
                    }
                }
            } break;
        }
        readToWriteAccess(&audiostate.lock);
        if (chfx) {
            s->state.fxchanged = false;
            s->offset += audiostate.audbuf.len * sfx[1].speedmul / 1000;
        } else {
            s->offset += audiostate.audbuf.len * fx.speedmul / 1000;
        }
        writeToReadAccess(&audiostate.lock);
    }
    releaseReadAccess(&audiostate.lock);
    if (audiostate.channels < 2) {
        for (int i = 0; i < audiostate.audbuf.len; ++i) {
            int sample = (audbuf[0][i] + audbuf[1][i]) / 2;
            if (sample > 32767) sample = 32767;
            else if (sample < -32768) sample = -32768;
            audiostate.audbuf.out[i] = sample;
        }
    } else {
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < audiostate.audbuf.len; ++i) {
                int sample = audbuf[c][i];
                if (sample > 32767) sample = 32767;
                else if (sample < -32768) sample = -32768;
                audiostate.audbuf.out[i * audiostate.channels + c] = sample;
            }
        }
    }
}

static void callback(void* data, uint16_t* stream, int len) {
    (void)data;
    if (audiostate.valid) {
        int bufi = audiostate.audbufindex;
        #if DEBUG(3)
        plog(LL_INFO | LF_DEBUG, "Playing %d...", bufi);
        #endif
        int samples = len / sizeof(*stream) / audiostate.channels;
        if (audiostate.audbuf.len == samples) {
            memcpy(stream, audiostate.audbuf.out, len);
        } else {
            plog(LL_WARN | LF_DEBUG, "Mismatch between buffer length (%d) and requested samples (%d)", audiostate.audbuf.len, samples);
        }
        #if DEBUG(3)
        plog(LL_INFO | LF_DEBUG, "Finished playing %d", bufi);
        #endif
        audiostate.audbufindex = (bufi + 1) % 4;
        if ((audiostate.mixaudbufindex + 1) % 4 == bufi || audiostate.mixaudbufindex < 0) {
            #if DEBUG(2)
            plog(LL_INFO | LF_DEBUG, "Mix thread is beind!");
            #endif
        } else {
            signalCond(&audiostate.mixcond);
        }
    } else {
        memset(stream, 0, len);
    }
}

static void* mixthread(struct thread_data* td) {
    mutex_t m;
    createMutex(&m);
    lockMutex(&m);
    while (!td->shouldclose) {
        int mixbufi = audiostate.mixaudbufindex;
        int bufi = audiostate.audbufindex;
        bool mixbufilt0 = (mixbufi < 0);
        if (bufi == mixbufi || bufi == (mixbufi + 1) % 4 || mixbufilt0) {
            mixbufi = (mixbufi + 1) % 4;
            #if DEBUG(3)
            plog(LL_INFO | LF_DEBUG, "Mixing %d...", mixbufi);
            #endif
            mixsounds(bufi % 2);
            #if DEBUG(3)
            plog(LL_INFO | LF_DEBUG, "Finished mixing %d", mixbufi);
            #endif
            audiostate.mixaudbufindex = mixbufi;
            if (!mixbufilt0) {
                if (td->shouldclose) break;
                waitOnCond(&audiostate.mixcond, &m, 0);
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

void stopSound(int64_t id) {
    acquireReadAccess(&audiostate.lock);
    if (!audiostate.valid) {
        releaseReadAccess(&audiostate.lock);
        return;
    }
    if (id >= 0) {
        int i = (int64_t)(id % (int64_t)audiostate.voicecount);
        struct audiosound* v = &audiostate.voices[i];
        if (v->id >= 0 && id == v->id) {
            readToWriteAccess(&audiostate.lock);
            stopSound_inline(v);
            releaseWriteAccess(&audiostate.lock);
        } else {
            releaseReadAccess(&audiostate.lock);
        }
    }
}

void changeSoundFX(int64_t id, int immediate, ...) {
    acquireReadAccess(&audiostate.lock);
    if (!audiostate.valid) {
        releaseReadAccess(&audiostate.lock);
        return;
    }
    if (id >= 0) {
        int i = (int64_t)(id % (int64_t)audiostate.voicecount);
        struct audiosound* v = &audiostate.voices[i];
        if (v->id >= 0 && id == v->id) {
            readToWriteAccess(&audiostate.lock);
            if (!immediate && !v->state.fxchanged) {
                v->fx[0] = v->fx[1];
                v->state.fxchanged = true;
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
            calcSoundFX(v);
            //v->state.updatefx = true;
            releaseWriteAccess(&audiostate.lock);
        } else {
            releaseReadAccess(&audiostate.lock);
        }
    } else {
        releaseReadAccess(&audiostate.lock);
    }
}

void changeSoundFlags(int64_t id, unsigned disable, unsigned enable) {
    acquireReadAccess(&audiostate.lock);
    if (!audiostate.valid) {
        releaseReadAccess(&audiostate.lock);
        return;
    }
    if (id >= 0) {
        int i = (int64_t)(id % (int64_t)audiostate.voicecount);
        struct audiosound* v = &audiostate.voices[i];
        if (v->id >= 0 && id == v->id) {
            readToWriteAccess(&audiostate.lock);
            v->flags &= ~(uint8_t)disable;
            v->flags |= (uint8_t)enable;
            //v->state.updatefx = true;
            releaseWriteAccess(&audiostate.lock);
        } else {
            releaseReadAccess(&audiostate.lock);
        }
    }
}

int64_t playSound(bool paused, struct rc_sound* rc, unsigned f, ...) {
    acquireReadAccess(&audiostate.lock);
    if (!audiostate.valid) {
        releaseReadAccess(&audiostate.lock);
        return -1;
    }
    int64_t nextid = audiostate.nextid;
    int64_t stopid = nextid + audiostate.voicecount;
    int voicecount = audiostate.voicecount;
    int64_t id = -1;
    struct audiosound* v = NULL;
    for (int64_t vi = nextid; vi < stopid; ++vi) {
        int i = (int64_t)(vi % (int64_t)voicecount);
        struct audiosound* tmpv = &audiostate.voices[i];
        if (tmpv->id < 0) {
            id = vi;
            v = tmpv;
            break;
        }
    }
    if (v) {
        readToWriteAccess(&audiostate.lock);
    } else {
        for (int64_t vi = nextid; vi < stopid; ++vi) {
            int i = (int64_t)(vi % (int64_t)voicecount);
            struct audiosound* tmpv = &audiostate.voices[i];
            if (!(tmpv->flags & SOUNDFLAG_UNINTERRUPTIBLE)) {
                readToWriteAccess(&audiostate.lock);
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
                v->audbuf.len = audiostate.audbuflen;
                v->audbuf.data[0] = malloc(v->audbuf.len * sizeof(*v->audbuf.data[0]));
                v->audbuf.data[1] = malloc(v->audbuf.len * sizeof(*v->audbuf.data[1]));
                stb_vorbis_get_samples_short(v->vorbis, 2, v->audbuf.data, v->audbuf.len);
            } break;
            case RC_SOUND_FRMT_MP3: {
                v->mp3 = malloc(sizeof(*v->mp3));
                mp3dec_ex_open_buf(v->mp3, rc->data, rc->size, MP3D_SEEK_TO_SAMPLE);
                v->audbuf.off = 0;
                v->audbuf.len = audiostate.audbuflen;
                v->audbuf.data_mp3 = malloc(v->audbuf.len * v->rc->channels * sizeof(*v->audbuf.data_mp3));
                mp3dec_ex_read(v->mp3, v->audbuf.data_mp3, v->audbuf.len);
            } break;
        }
        v->offset = 0;
        v->flags = f;
        v->state.paused = paused;
        v->state.fxchanged = false;
        //v->state.updatefx = false;
        v->vol[0] = 1.0;
        v->vol[1] = 1.0;
        v->speed = 1.0;
        v->pos[0] = 0.0;
        v->pos[1] = 0.0;
        v->pos[2] = 0.0;
        v->range = 20.0;
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
        calcSoundFX(v);
        releaseWriteAccess(&audiostate.lock);
    } else {
        releaseReadAccess(&audiostate.lock);
    }
    return id;
}

bool initAudio(void) {
    if (!createAccessLock(&audiostate.lock)) return false;
    if (!createCond(&audiostate.mixcond)) return false;
    if (SDL_Init(SDL_INIT_AUDIO)) {
        plog(LL_CRIT | LF_FUNCLN, "Failed to init audio: %s", SDL_GetError());
        return false;
    }
    return true;
}

void updateSounds(void) {
    acquireWriteAccess(&audiostate.lock);
    for (int si = 0; si < audiostate.voicecount; ++si) {
        struct audiosound* s = &audiostate.voices[si];
        if (s->id < 0) continue;
        if (!s->state.fxchanged && (s->flags & SOUNDFLAG_POSEFFECT) && !(s->flags & SOUNDFLAG_RELPOS)) {
            s->fx[0] = s->fx[1];
            s->state.fxchanged = true;
            calcSoundFX(s);
            //s->state.updatefx = true;
        }
    }
    releaseWriteAccess(&audiostate.lock);
    if (!audiostate.multithreaded) {
        uint32_t qs = SDL_GetQueuedAudioSize(audiostate.output);
        //printf("samples: %u\n", (unsigned)(qs / sizeof(int16_t) / audiostate.channels));
        if (qs < audiostate.audbuf.outsize * audiostate.outbufcount) {
            int count = (audiostate.audbuf.outsize * audiostate.outbufcount - qs) / audiostate.audbuf.outsize;
            for (int i = 0; i < count; ++i) {
                mixsounds(0);
                SDL_QueueAudio(audiostate.output, audiostate.audbuf.out, audiostate.audbuf.outsize);
            }
        }
    }
}

bool startAudio(void) {
    acquireWriteAccess(&audiostate.lock);
    char* tmp = cfg_getvar(config, "Sound", "disable");
    if (tmp && strbool(tmp, false)) {
        audiostate.valid = false;
        releaseWriteAccess(&audiostate.lock);
        plog(LL_INFO, "Audio disabled");
        return true;
    }
    SDL_AudioSpec inspec;
    SDL_AudioSpec outspec;
    inspec.format = AUDIO_S16SYS;
    inspec.channels = 2;
    tmp = cfg_getvar(config, "Sound", "freq");
    if (tmp) {
        inspec.freq = atoi(tmp);
        free(tmp);
    } else {
        #if PLATFORM != PLAT_NXDK
        inspec.freq = 44100;
        #else
        inspec.freq = 22050;
        #endif
    }
    tmp = cfg_getvar(config, "Sound", "buffer");
    if (tmp) {
        inspec.samples = atoi(tmp);
        free(tmp);
    } else {
        inspec.samples = 1024;
    }
    {
        bool mutlithread = SDL_GetCPUCount() > 2;
        tmp = cfg_getvar(config, "Sound", "multithreaded");
        if (tmp) {
            audiostate.multithreaded = strbool(tmp, mutlithread);
            free(tmp);
        } else {
            audiostate.multithreaded = mutlithread;
        }
    }
    inspec.callback = (audiostate.multithreaded) ? (SDL_AudioCallback)callback : NULL;
    inspec.userdata = NULL;
    #if PLATFORM != PLAT_NXDK
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
        audiostate.output = output;
        plog(LL_INFO, "Audio info (device id %d):", (int)output);
        plog(LL_INFO, "  Frequency: %d", outspec.freq);
        plog(LL_INFO, "  Channels: %d (%s)", outspec.channels, (outspec.channels == 1) ? "mono" : "stereo");
        plog(LL_INFO, "  Samples: %d", (int)outspec.samples);
        audiostate.freq = outspec.freq;
        audiostate.channels = outspec.channels;
        audiostate.audbuf.len = outspec.samples;
        audiostate.audbuf.data[0][0] = malloc(outspec.samples * sizeof(*audiostate.audbuf.data[0][0]));
        audiostate.audbuf.data[0][1] = malloc(outspec.samples * sizeof(*audiostate.audbuf.data[0][1]));
        if (audiostate.multithreaded) {
            audiostate.audbuf.data[1][0] = malloc(outspec.samples * sizeof(*audiostate.audbuf.data[1][0]));
            audiostate.audbuf.data[1][1] = malloc(outspec.samples * sizeof(*audiostate.audbuf.data[1][1]));
        }
        audiostate.audbuf.outsize = outspec.samples * sizeof(*audiostate.audbuf.out) * outspec.channels;
        audiostate.audbuf.out = malloc(audiostate.audbuf.outsize);
        int voicecount;
        tmp = cfg_getvar(config, "Sound", "voices");
        if (tmp) {
            voicecount = atoi(tmp);
            free(tmp);
        } else {
            voicecount = 32;
        }
        if (audiostate.voicecount != voicecount) {
            audiostate.voicecount = voicecount;
            audiostate.voices = realloc(audiostate.voices, voicecount * sizeof(*audiostate.voices));
        }
        for (int i = 0; i < voicecount; ++i) {
            audiostate.voices[i].id = -1;
        }
        tmp = cfg_getvar(config, "Sound", "outbufcount");
        if (tmp) {
            audiostate.outbufcount = atoi(tmp);
            free(tmp);
        } else {
            audiostate.outbufcount = 2;
        }
        tmp = cfg_getvar(config, "Sound", "decodebuf");
        if (tmp) {
            audiostate.audbuflen = atoi(tmp);
            free(tmp);
        } else {
            audiostate.audbuflen = 4096;
        }
        tmp = cfg_getvar(config, "Sound", "decodewhole");
        if (tmp) {
            audiostate.soundrcopt.decodewhole = strbool(tmp, true);
            free(tmp);
        } else {
            #if PLATFORM != PLAT_NXDK
            audiostate.soundrcopt.decodewhole = true;
            #else
            audiostate.soundrcopt.decodewhole = false;
            #endif
        }
        audiostate.nextid = 0;
        audiostate.audbufindex = 0;
        audiostate.mixaudbufindex = -1;
        audiostate.buftime = outspec.samples * 1000000 / outspec.freq;
        if (audiostate.multithreaded) createThread(&audiostate.mixthread, "mix", mixthread, NULL);
        audiostate.valid = true;
        SDL_PauseAudioDevice(output, 0);
    } else {
        audiostate.valid = false;
        plog(LL_ERROR, "Failed to get audio info for default output device; audio disabled: %s", SDL_GetError());
    }
    releaseWriteAccess(&audiostate.lock);
    return true;
}

void stopAudio(void) {
    if (audiostate.valid) {
        if (audiostate.multithreaded) {
            quitThread(&audiostate.mixthread);
            broadcastCond(&audiostate.mixcond);
            destroyThread(&audiostate.mixthread, NULL);
        }
        acquireWriteAccess(&audiostate.lock);
        audiostate.valid = false;
        SDL_PauseAudioDevice(audiostate.output, 1);
        SDL_CloseAudioDevice(audiostate.output);
        for (int i = 0; i < audiostate.voicecount; ++i) {
            struct audiosound* v = &audiostate.voices[i];
            if (v->id >= 0) {
               stopSound_inline(v);
            }
        }
        free(audiostate.voices);
        free(audiostate.audbuf.data[0][0]);
        free(audiostate.audbuf.data[0][1]);
        if (audiostate.multithreaded) {
            free(audiostate.audbuf.data[1][0]);
            free(audiostate.audbuf.data[1][1]);
        }
        free(audiostate.audbuf.out);
        releaseWriteAccess(&audiostate.lock);
    }
}

void termAudio(void) {
    destroyAccessLock(&audiostate.lock);
    destroyCond(&audiostate.mixcond);
}
