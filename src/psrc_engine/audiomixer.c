#include "audiomixer.h"

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

void mixsounds(struct audiostate* a, int samples) {
    if (a->audbuf.len < samples) {
        a->audbuf.len = samples;
        a->audbuf.data[0] = realloc(a->audbuf.data[0], samples * sizeof(*a->audbuf.data[0]));
        a->audbuf.data[1] = realloc(a->audbuf.data[1], samples * sizeof(*a->audbuf.data[1]));
    }
    memset(a->audbuf.data[0], 0, samples * sizeof(*a->audbuf.data[0]));
    memset(a->audbuf.data[1], 0, samples * sizeof(*a->audbuf.data[1]));
    int* audbuf[2] = {a->audbuf.data[0], a->audbuf.data[1]};
    int outfreq = a->freq;
    int tmpbuf[2];
    for (int si = 0; si < a->voices; ++si) {
        struct audiosound* s = &a->voicedata[si];
        if (s->id < 0 || s->state.paused) continue;
        struct rc_sound* rc = s->rc;
        struct audiosound_fx sfx[2];
        struct audiosound_fx fx;
        bool chfx = s->state.fxchanged;
        if (chfx) {
            sfx[0] = s->fx[0];
            sfx[1] = s->fx[1];
        } else {
            fx = s->fx[1];
        }
        int64_t offset = s->offset;
        int freq = rc->freq;
        int len = rc->len;
        uint8_t flags = s->flags;
        bool loop = flags & SOUNDFLAG_LOOP;
        switch (rc->format) {
            case RC_SOUND_FRMT_VORBIS: {
                struct audiosound_audbuf ab = s->audbuf;
                if (loop) {
                    if (flags & SOUNDFLAG_FORCEMONO) {
                        for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                            if (chfx) interpfx(sfx, &fx, i, ii, samples);
                            int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                            if (pos >= 0) pos %= len;
                            getvorbisat_forcemono(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            audbuf[0][i] += tmpbuf[0] * fx.volmul[0] / 65536;
                            audbuf[1][i] += tmpbuf[1] * fx.volmul[1] / 65536;
                        }
                    } else {
                        for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                            if (chfx) interpfx(sfx, &fx, i, ii, samples);
                            int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                            if (pos >= 0) pos %= len;
                            //printf("[%"PRId64"]\n", pos);
                            getvorbisat(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            audbuf[0][i] += tmpbuf[0] * fx.volmul[0] / 65536;
                            audbuf[1][i] += tmpbuf[1] * fx.volmul[1] / 65536;
                        }
                    }
                } else {
                    if (flags & SOUNDFLAG_FORCEMONO) {
                        for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                            if (chfx) interpfx(sfx, &fx, i, ii, samples);
                            int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                            getvorbisat_forcemono(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            audbuf[0][i] += tmpbuf[0] * fx.volmul[0] / 65536;
                            audbuf[1][i] += tmpbuf[1] * fx.volmul[1] / 65536;
                        }
                    } else {
                        for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                            if (chfx) interpfx(sfx, &fx, i, ii, samples);
                            int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                            getvorbisat(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            audbuf[0][i] += tmpbuf[0] * fx.volmul[0] / 65536;
                            audbuf[1][i] += tmpbuf[1] * fx.volmul[1] / 65536;
                        }
                    }
                }
            } break;
            case RC_SOUND_FRMT_MP3: {
                struct audiosound_audbuf ab = s->audbuf;
                if (loop) {
                    if (flags & SOUNDFLAG_FORCEMONO) {
                        for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                            if (chfx) interpfx(sfx, &fx, i, ii, samples);
                            int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                            if (pos >= 0) pos %= len;
                            getmp3at_forcemono(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            audbuf[0][i] += tmpbuf[0] * fx.volmul[0] / 65536;
                            audbuf[1][i] += tmpbuf[1] * fx.volmul[1] / 65536;
                        }
                    } else {
                        for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                            if (chfx) interpfx(sfx, &fx, i, ii, samples);
                            int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                            if (pos >= 0) pos %= len;
                            //printf("[%"PRId64"]\n", pos);
                            getmp3at(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            audbuf[0][i] += tmpbuf[0] * fx.volmul[0] / 65536;
                            audbuf[1][i] += tmpbuf[1] * fx.volmul[1] / 65536;
                        }
                    }
                } else {
                    if (flags & SOUNDFLAG_FORCEMONO) {
                        for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                            if (chfx) interpfx(sfx, &fx, i, ii, samples);
                            int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                            getmp3at_forcemono(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            audbuf[0][i] += tmpbuf[0] * fx.volmul[0] / 65536;
                            audbuf[1][i] += tmpbuf[1] * fx.volmul[1] / 65536;
                        }
                    } else {
                        for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                            if (chfx) interpfx(sfx, &fx, i, ii, samples);
                            int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                            getmp3at(s, &ab, pos, len, &tmpbuf[0], &tmpbuf[1]);
                            audbuf[0][i] += tmpbuf[0] * fx.volmul[0] / 65536;
                            audbuf[1][i] += tmpbuf[1] * fx.volmul[1] / 65536;
                        }
                    }
                }
            } break;
            case RC_SOUND_FRMT_WAV: {
                union {
                    uint8_t* ptr;
                    int8_t* i8;
                    int16_t* i16;
                } data;
                data.ptr = rc->data;
                bool is8bit = rc->is8bit;
                bool stereo = rc->stereo;
                int channels = rc->channels;
                if (loop) {
                    if (flags & SOUNDFLAG_FORCEMONO) {
                        if (is8bit) {
                            for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                                if (chfx) interpfx(sfx, &fx, i, ii, samples);
                                int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                                if (pos >= 0) {
                                    pos %= len;
                                    tmpbuf[0] = data.i8[pos * channels];
                                    tmpbuf[0] = tmpbuf[0] * 256 + (tmpbuf[0] + 128);
                                    tmpbuf[1] = data.i8[pos * channels + stereo];
                                    tmpbuf[1] = tmpbuf[1] * 256 + (tmpbuf[1] + 128);
                                    int tmp = (tmpbuf[0] + tmpbuf[1]) / 2;
                                    audbuf[0][i] += tmp * fx.volmul[0] / 65536;
                                    audbuf[1][i] += tmp * fx.volmul[1] / 65536;
                                }
                            }
                        } else {
                            for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                                if (chfx) interpfx(sfx, &fx, i, ii, samples);
                                int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                                if (pos >= 0) {
                                    pos %= len;
                                    tmpbuf[0] = data.i16[pos * channels];
                                    tmpbuf[1] = data.i16[pos * channels + stereo];
                                    int tmp = (tmpbuf[0] + tmpbuf[1]) / 2;
                                    audbuf[0][i] += tmp * fx.volmul[0] / 65536;
                                    audbuf[1][i] += tmp * fx.volmul[1] / 65536;
                                }
                            }
                        }
                    } else {
                        if (is8bit) {
                            for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                                if (chfx) interpfx(sfx, &fx, i, ii, samples);
                                int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                                if (pos >= 0) {
                                    pos %= len;
                                    tmpbuf[0] = data.i8[pos * channels];
                                    tmpbuf[0] = tmpbuf[0] * 256 + (tmpbuf[0] + 128);
                                    tmpbuf[1] = data.i8[pos * channels + stereo];
                                    tmpbuf[1] = tmpbuf[1] * 256 + (tmpbuf[1] + 128);
                                    audbuf[0][i] += tmpbuf[0] * fx.volmul[0] / 65536;
                                    audbuf[1][i] += tmpbuf[1] * fx.volmul[1] / 65536;
                                }
                            }
                        } else {
                            for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                                if (chfx) interpfx(sfx, &fx, i, ii, samples);
                                int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                                if (pos >= 0) {
                                    pos %= len;
                                    tmpbuf[0] = data.i16[pos * channels];
                                    tmpbuf[1] = data.i16[pos * channels + stereo];
                                    audbuf[0][i] += tmpbuf[0] * fx.volmul[0] / 65536;
                                    audbuf[1][i] += tmpbuf[1] * fx.volmul[1] / 65536;
                                }
                            }
                        }
                    }
                } else {
                    if (flags & SOUNDFLAG_FORCEMONO) {
                        if (is8bit) {
                            for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                                if (chfx) interpfx(sfx, &fx, i, ii, samples);
                                int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                                if (pos >= 0 && pos < len) {
                                    tmpbuf[0] = data.i8[pos * channels];
                                    tmpbuf[0] = tmpbuf[0] * 256 + (tmpbuf[0] + 128);
                                    tmpbuf[1] = data.i8[pos * channels + stereo];
                                    tmpbuf[1] = tmpbuf[1] * 256 + (tmpbuf[1] + 128);
                                    int tmp = (tmpbuf[0] + tmpbuf[1]) / 2;
                                    audbuf[0][i] += tmp * fx.volmul[0] / 65536;
                                    audbuf[1][i] += tmp * fx.volmul[1] / 65536;
                                }
                            }
                        } else {
                            for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                                if (chfx) interpfx(sfx, &fx, i, ii, samples);
                                int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                                if (pos >= 0 && pos < len) {
                                    tmpbuf[0] = data.i16[pos * channels];
                                    tmpbuf[1] = data.i16[pos * channels + stereo];
                                    int tmp = (tmpbuf[0] + tmpbuf[1]) / 2;
                                    audbuf[0][i] += tmp * fx.volmul[0] / 65536;
                                    audbuf[1][i] += tmp * fx.volmul[1] / 65536;
                                }
                            }
                        }
                    } else {
                        if (is8bit) {
                            for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                                if (chfx) interpfx(sfx, &fx, i, ii, samples);
                                int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                                if (pos >= 0 && pos < len) {
                                    tmpbuf[0] = data.i8[pos * channels];
                                    tmpbuf[0] = tmpbuf[0] * 256 + (tmpbuf[0] + 128);
                                    tmpbuf[1] = data.i8[pos * channels + stereo];
                                    tmpbuf[1] = tmpbuf[1] * 256 + (tmpbuf[1] + 128);
                                    audbuf[0][i] += tmpbuf[0] * fx.volmul[0] / 65536;
                                    audbuf[1][i] += tmpbuf[1] * fx.volmul[1] / 65536;
                                }
                            }
                        } else {
                            for (int i = 0, ii = samples; i < samples; ++i, --ii) {
                                if (chfx) interpfx(sfx, &fx, i, ii, samples);
                                int64_t pos = calcpos(&fx, offset, i, freq, outfreq);
                                if (pos >= 0 && pos < len) {
                                    tmpbuf[0] = data.i16[pos * channels];
                                    tmpbuf[1] = data.i16[pos * channels + stereo];
                                    audbuf[0][i] += tmpbuf[0] * fx.volmul[0] / 65536;
                                    audbuf[1][i] += tmpbuf[1] * fx.volmul[1] / 65536;
                                }
                            }
                        }
                    }
                }
            } break;
        }
        if (chfx) {
            s->state.fxchanged = 0;
            s->offset += samples * sfx[1].speedmul / 1000;
        } else {
            s->offset += samples * fx.speedmul / 1000;
        }
    }
}
