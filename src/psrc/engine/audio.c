#include "audio.h"

#include "../debug.h"
#include "../logging.h"
#include "../common.h"
#include "../common/config.h"

struct audiostate audiostate;

void doLPFilter(unsigned mul, unsigned div, int16_t* lastoutp, unsigned len, int* buf) {
    register int lastout = *lastoutp;
    for (register unsigned i = 0; i < len; ++i) {
        register int s = buf[i];
        buf[i] = lastout = lastout + (s - lastout) * mul / div;
    }
    *lastoutp = lastout;
}
void doHPFilter(unsigned mul, unsigned div, int16_t* lastinp, int16_t* lastoutp, unsigned len, int* buf) {
    register int lastin = *lastinp;
    register int lastout = *lastoutp;
    for (register unsigned i = 0; i < len; ++i) {
        register int s = buf[i];
        buf[i] = lastout = (lastout + s - lastin) * mul / div;
        lastin = s;
    }
    *lastinp = lastin;
    *lastoutp = lastout;
}
void doLPFilter_interp(unsigned mul1, unsigned mul2, unsigned div, int16_t* lastoutp, unsigned len, int* buf) {
    register int lastout = *lastoutp;
    for (register unsigned i = 0, ii = len; i < len; ++i, --ii) {
        register int s = buf[i];
        buf[i] = lastout = lastout + (s - lastout) * ((mul1 * ii + mul2 * i) / len) / div;
    }
    *lastoutp = lastout;
}
void doHPFilter_interp(unsigned mul1, unsigned mul2, unsigned div, int16_t* lastinp, int16_t* lastoutp, unsigned len, int* buf) {
    register int lastin = *lastinp;
    register int lastout = *lastoutp;
    for (register unsigned i = 0, ii = len; i < len; ++i, --ii) {
        register int s = buf[i];
        buf[i] = lastout = (lastout + s - lastin) * ((mul1 * ii + mul2 * i) / len) / div;
        lastin = s;
    }
    *lastinp = lastin;
    *lastoutp = lastout;
}

static void mixsound_cb_wav(struct audiosound* s, long loop, long pos, long* start, long* end, int16_t** buf) {
    (void)loop;
    if (!buf && s->rc->is8bit) free(s->wav.cvtbuf);
    if (!s->rc->is8bit) {
        *start = 0;
        *end = s->rc->len - 1;
        *buf = (int16_t*)s->rc->data;
    } else {
        int16_t* dataout = s->wav.cvtbuf;
        *buf = dataout;
        long tmpend = pos + audiostate.decbuflen;
        if (tmpend > s->rc->len) tmpend = s->rc->len;
        *start = pos;
        *end = tmpend - 1;
        tmpend *= s->rc->channels;
        uint8_t* datain = s->rc->data;
        for (register long i = 0; i < tmpend; ++i) {
            register int tmp = datain[i];
            dataout[i] = (tmp - 128) * 256 + tmp;
        }
    }
}

static void mixsound_cb_vorbis(struct audiosound* s, long loop, long pos, long* start, long* end, int16_t** buf) {
    (void)loop;
    if (!buf) free(s->vorbis.decbuf);
    
}

static void mixsound_cb_mp3(struct audiosound* s, long loop, long pos, long* start, long* end, int16_t** buf) {
    (void)loop;
    if (!buf) free(s->mp3.decbuf);
    
}

#define MIXSOUND__DOMIXING(l) do {\
    if (!oob) {\
        register int_fast16_t sample_l, sample_r;\
        if (pos > bufend || pos < bufstart) {\
            cb(ctx, loop, pos, &bufstart, &bufend, &buf);\
        }\
        register long bufpos = (pos - bufstart) * (long)ch;\
        sample_l = buf[bufpos];\
        sample_r = buf[bufpos + (ch != 1)];\
        if (frac) {\
            int mix = frac / outfreq;\
            int imix = 256 - mix;\
            sample_l *= imix;\
            sample_r *= imix;\
            long pos2 = pos + 1;\
            if (pos2 > bufend || pos2 < bufstart) {\
                register long loop2 = loop;\
                if (pos2 == len) {\
                    if (!(flags & SOUNDFLAG_LOOP)) {\
                        goto skipinterp_##l;\
                    } else {\
                        pos2 = 0;\
                        ++loop2;\
                    }\
                }\
                cb(ctx, loop2, pos2, &bufstart, &bufend, &buf);\
            }\
            bufpos = (pos2 - bufstart) * (long)ch;\
            sample_l += buf[bufpos] * mix;\
            sample_r += buf[bufpos + (ch != 1)] * mix;\
            skipinterp_##l:;\
            sample_l /= 256;\
            sample_r /= 256;\
        }\
        if (mixmono) {\
            audiostate.fxbuf[0][i] = (sample_l + sample_r) / 2;\
        } else {\
            audiostate.fxbuf[0][i] = sample_l;\
            audiostate.fxbuf[1][i] = sample_r;\
        }\
    } else {\
        audiostate.fxbuf[0][i] = 0;\
        if (!mixmono) audiostate.fxbuf[1][i] = 0;\
    }\
} while (0)
#define MIXSOUND__DOMIXING_EVALLINE(l) MIXSOUND__DOMIXING(l)
#define MIXSOUND_DOMIXING() MIXSOUND__DOMIXING_EVALLINE(__LINE__)
#define MIXSOUND_DOPOSMATH(...) do {\
    pos += frac / div;\
    frac %= div;\
    if (frac < 0) {\
        frac += div;\
        --pos;\
    }\
    if (pos >= len) {\
        loop += pos / len;\
        pos %= len;\
    } else if (pos < 0) {\
        loop += pos / len - 1;\
        pos %= len;\
        pos += len;\
    } else {\
        __VA_ARGS__\
    }\
} while (0)
#define MIXSOUND_OOBCHECK() do {\
    if (!(flags & SOUNDFLAG_LOOP)) {\
        if (!(flags & SOUNDFLAG_WRAP)) {\
            oob = (loop != 0);\
        } else {\
            oob = (loop < -1 || loop > 0);\
        }\
    } else {\
        if (!(flags & SOUNDFLAG_WRAP)) {\
            oob = (loop < 0);\
        } else {\
            oob = false;\
        }\
    }\
} while (0)
/*static*/ void mixsound(struct audiosound* s, bool fake, bool mixmono, int oldvol, int newvol) {
    audiocb cb;
    void* ctx;
    long len;
    long freq;
    unsigned ch;
    if (!s->iflags & SOUNDIFLAG_USESCB) {
        switch (s->rc->format) {
            /*case RC_SOUND_FRMT_WAV*/ default: cb = (audiocb)mixsound_cb_wav; break;
            #ifdef PSRC_USESTBVORBIS
            case RC_SOUND_FRMT_VORBIS: cb = (audiocb)mixsound_cb_vorbis; break;
            #endif
            #ifdef PSRC_USEMINIMP3
            case RC_SOUND_FRMT_MP3: cb = (audiocb)mixsound_cb_mp3; break;
            #endif
        }
        ctx = s;
        len = s->rc->len;
        freq = s->rc->freq;
        ch = s->rc->channels;
    } else {
        cb = s->cb.cb;
        ctx = s->cb.ctx;
        len = s->cb.len;
        freq = s->cb.freq;
        ch = s->cb.ch;
    }

    uint8_t flags = s->flags;
    long loop = s->loop;
    long oldloop = loop;
    register long pos = s->pos;
    register long frac = s->frac;
    long outfreq = audiostate.freq;
    #if 0
    {
        long outfreq2 = outfreq, gcd = freq;
        while (outfreq2) {
            long tmp = gcd % outfreq2;
            gcd = outfreq2;
            outfreq2 = tmp;
        }
        outfreq /= gcd;
        freq /= gcd;
    }
    #endif
    long div = outfreq * 256;
    int vol[2][2];

    int16_t* buf;
    long bufstart = 0, bufend = -1;

    uint_fast8_t curfxi = s->fxi;
    uint_fast8_t newfxi = (curfxi + 1) % 2;

    if (!(s->iflags & SOUNDIFLAG_FXCH_OTHER)) {
        uint_fast8_t curfxi = s->fxi;
        long speedmul = s->calcfx[curfxi].speedmul;
        long change = freq * speedmul;
        register unsigned i = 0;
        if (!fake) {
            bool oob;
            while (1) {
                MIXSOUND_OOBCHECK();
                skipoobchk_1:;
                MIXSOUND_DOMIXING();
                frac += change;
                MIXSOUND_DOPOSMATH(++i; if (i == audiostate.buflen) goto brkloop_1; goto skipoobchk_1;);
                ++i;
                if (i == audiostate.buflen) break;
            }
            brkloop_1:;
        } else {
            while (1) {
                frac += change;
                MIXSOUND_DOPOSMATH();
                ++i;
                if (i == audiostate.buflen) break;
            }
        }
    } else {
        uint_fast8_t oldfxi = s->fxi;
        uint_fast8_t newfxi = (oldfxi + 1) % 2;
        long posoff[2];
        posoff[0] = 0;
        posoff[1] = s->calcfx[newfxi].posoff - s->calcfx[oldfxi].posoff;
        long speedmul[2];
        speedmul[0] = s->calcfx[oldfxi].speedmul;
        speedmul[1] = s->calcfx[newfxi].speedmul;
        register unsigned i = 0, ii = audiostate.buflen;
        if (!fake) {
            bool oob;
            while (1) {
                MIXSOUND_OOBCHECK();
                skipoobchk_2:;
                MIXSOUND_DOMIXING();
                long tmpposoff = posoff[1] * i / 1024 - posoff[0];
                posoff[0] = tmpposoff;
                tmpposoff *= freq;
                pos += tmpposoff / outfreq;
                frac += freq * ((speedmul[0] * ii + speedmul[1] * i) / audiostate.buflen) + (tmpposoff % outfreq) * 256;
                MIXSOUND_DOPOSMATH(++i; if (i == audiostate.buflen) goto brkloop_2; --ii; goto skipoobchk_2;);
                ++i;
                if (i == audiostate.buflen) break;
                --ii;
            }
            brkloop_2:;
        } else {
            while (1) {
                long tmpposoff = posoff[1] * i / 1024 - posoff[0];
                posoff[0] = tmpposoff;
                tmpposoff *= freq;
                pos += tmpposoff / outfreq;
                frac += freq * ((speedmul[0] * ii + speedmul[1] * i) / audiostate.buflen) + (tmpposoff % outfreq) * 256;
                MIXSOUND_DOPOSMATH();
                ++i;
                if (i == audiostate.buflen) break;
                --ii;
            }
        }
        posoff[1] -= posoff[0];
        posoff[1] *= freq;
        pos += posoff[1] / outfreq;
        frac += (posoff[1] % outfreq) * 256;
        MIXSOUND_DOPOSMATH();
    }
    if (!fake) {
        if (!(s->iflags & SOUNDIFLAG_FXCH_OTHER)) {
            unsigned tmp = s->calcfx[curfxi].hpfiltmul;
            if (tmp) {
                doHPFilter(tmp, audiostate.freq, &s->hplastin[0], &s->hplastout[0], audiostate.buflen, audiostate.fxbuf[0]);
                if (!mixmono) doHPFilter(tmp, audiostate.freq, &s->hplastin[1], &s->hplastout[1], audiostate.buflen, audiostate.fxbuf[1]);
            }
            tmp = s->calcfx[curfxi].lpfiltmul;
            if (s->calcfx[curfxi].lpfiltmul) {
                doLPFilter(tmp, audiostate.freq, &s->lplastout[0], audiostate.buflen, audiostate.fxbuf[0]);
                if (!mixmono) doLPFilter(tmp, audiostate.freq, &s->lplastout[1], audiostate.buflen, audiostate.fxbuf[1]);
            }
        } else {
            unsigned tmp1 = s->calcfx[curfxi].hpfiltmul;
            unsigned tmp2 = s->calcfx[newfxi].hpfiltmul;
            if (tmp1 || tmp2) {
                doHPFilter_interp(tmp1, tmp2, audiostate.freq, &s->hplastin[0], &s->hplastout[0], audiostate.buflen, audiostate.fxbuf[0]);
                if (!mixmono) doHPFilter_interp(tmp1, tmp2, audiostate.freq, &s->hplastin[1], &s->hplastout[1], audiostate.buflen, audiostate.fxbuf[1]);
            }
            tmp1 = s->calcfx[curfxi].lpfiltmul;
            tmp2 = s->calcfx[newfxi].lpfiltmul;
            if (tmp1 || tmp2) {
                doLPFilter_interp(tmp1, tmp2, audiostate.freq, &s->lplastout[0], audiostate.buflen, audiostate.fxbuf[0]);
                if (!mixmono) doLPFilter_interp(tmp1, tmp2, audiostate.freq, &s->lplastout[1], audiostate.buflen, audiostate.fxbuf[1]);
            }
        }
        if (!(s->iflags & SOUNDIFLAG_FXCH_VOL)) {
            vol[0][0] = s->calcfx[curfxi].volmul[0] * oldvol / 32768;
            vol[0][1] = s->calcfx[curfxi].volmul[1] * oldvol / 32768;
            vol[1][0] = s->calcfx[curfxi].volmul[0] * newvol / 32768;
            vol[1][1] = s->calcfx[curfxi].volmul[1] * newvol / 32768;
        } else {
            vol[0][0] = s->calcfx[curfxi].volmul[0] * oldvol / 32768;
            vol[0][1] = s->calcfx[curfxi].volmul[1] * oldvol / 32768;
            vol[1][0] = s->calcfx[newfxi].volmul[0] * newvol / 32768;
            vol[1][1] = s->calcfx[newfxi].volmul[1] * newvol / 32768;
        }
        int* fxbuf[2] = {
            audiostate.fxbuf[0],
            audiostate.fxbuf[!mixmono]
        };
        for (register unsigned i = 0, ii = audiostate.buflen; i < audiostate.buflen; ++i, --ii) {
            audiostate.mixbuf[0][i] += fxbuf[0][i] * ((vol[0][0] * ii + vol[1][0] * i) / audiostate.buflen) / 32768;
            audiostate.mixbuf[1][i] += fxbuf[1][i] * ((vol[0][1] * ii + vol[1][1] * i) / audiostate.buflen) / 32768;
        }
    }

    if (!(flags & SOUNDFLAG_LOOP)) {
        if (loop > 0) {
            if (loop > oldloop) goto unload;
        } else if (loop < ((!(flags & SOUNDFLAG_WRAP)) ? 0 : -1)) {
            if (loop < oldloop) goto unload;
        }
    } else if (!(flags & SOUNDFLAG_WRAP)) {
        if (loop < 0) {
            if (loop < oldloop) goto unload;
        }
    }

    s->loop = loop;
    s->pos = pos;
    s->frac = frac;
    if (s->iflags & SOUNDIFLAG_FXCH) {
        s->iflags &= ~SOUNDIFLAG_FXCH;
        s->fxi = newfxi;
    }

    return;

    unload:;

    cb(ctx, 0, 0, NULL, NULL, NULL);
}

static void mixsounds(unsigned buf) {
    memset(audiostate.mixbuf[0], 0, audiostate.buflen * sizeof(**audiostate.mixbuf));
    memset(audiostate.mixbuf[1], 0, audiostate.buflen * sizeof(**audiostate.mixbuf));
    // ...
    int16_t* out = audiostate.outbuf[buf];
    if (audiostate.channels < 2) {
        for (register unsigned i = 0; i < audiostate.buflen; ++i) {
            register int sample = (audiostate.mixbuf[0][i] + audiostate.mixbuf[1][i]) / 2;
            out[i] = (sample <= 32767) ? ((sample >= -32768) ? sample : -32768) : 32767;
        }
    } else {
        for (register unsigned c = 0; c < 2; ++c) {
            for (register unsigned i = 0; i < audiostate.buflen; ++i) {
                register int sample = audiostate.mixbuf[c][i];
                out[i * audiostate.channels + c] = (sample <= 32767) ? ((sample >= -32768) ? sample : -32768) : 32767;
            }
        }
        #if 0
        for (register unsigned c = 2; c < audiostate.channels; ++c) {
            out[i * audiostate.channels + c] = 0;
        }
        #endif
    }
}

void updateAudio(float framemult) {
    (void)framemult;
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (audiostate.usecallback) {
        while (audiostate.mixoutbufi != (audiostate.outbufi + 1) % 4 || audiostate.mixoutbufi == -1U) {
            unsigned mixoutbufi = (audiostate.mixoutbufi + 1) % 4;
            #if DEBUG(3)
            plog(LL_INFO | LF_DEBUG, "Mixing %u...", mixoutbufi);
            #endif
            mixsounds(mixoutbufi % 2);
            #if DEBUG(3)
            plog(LL_INFO | LF_DEBUG, "Finished mixing %u", mixoutbufi);
            #endif
            audiostate.mixoutbufi = mixoutbufi;
        }
    } else {
        #ifndef PSRC_USESDL1
        uint32_t queuesz = SDL_GetQueuedAudioSize(audiostate.output);
        unsigned targetsz = audiostate.outsize * audiostate.outqueue;
        if (queuesz < targetsz) {
            unsigned count = (targetsz - queuesz) / audiostate.outsize;
            for (unsigned i = 0; i < count; ++i) {
                mixsounds(0);
                SDL_QueueAudio(audiostate.output, audiostate.outbuf[0], audiostate.outsize);
            }
        }
        #endif
    }
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}

static void callback(void* data, uint16_t* stream, int len) {
    (void)data;
    #if DEBUG(3)
    plog(LL_INFO | LF_DEBUG, "Playing %u...", audiostate.outbufi);
    #endif
    unsigned mixoutbufi = audiostate.mixoutbufi;
    if (mixoutbufi != (audiostate.outbufi + 3) % 4 && mixoutbufi != -1U) {
        unsigned samples = len / sizeof(*stream) / audiostate.channels;
        if (audiostate.buflen == samples) {
            memcpy(stream, audiostate.outbuf[audiostate.outbufi % 2], len);
        } else {
            plog(LL_WARN | LF_DEBUG, "Mismatch between buffer length (%u) and requested samples (%d)", audiostate.buflen, samples);
        }
        #if DEBUG(3)
        plog(LL_INFO | LF_DEBUG, "Finished playing %u", audiostate.outbufi);
        #endif
        audiostate.outbufi = (audiostate.outbufi + 1) % 4;
    } else {
        #if DEBUG(2)
        plog(LL_INFO | LF_DEBUG, "Mixer is beind!");
        #endif
        memset(stream, 0, len);
    }
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
    if (!success) {
        audiostate.valid = false;
        plog(LL_ERROR, "Failed to get audio info for default output device; audio disabled: %s", SDL_GetError());
        #ifndef PSRC_NOMT
        releaseWriteAccess(&audiostate.lock);
        #endif
        return false;
    }
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
    audiostate.buflen = outspec.samples;
    audiostate.outsize = outspec.samples * sizeof(**audiostate.outbuf) * outspec.channels;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
    return true;
}

void stopAudio(void) {

}

bool restartAudio(void) {
    stopAudio();
    return startAudio();
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

void quitAudio(bool quick) {
    (void)quick;
    #ifndef PSRC_NOMT
    destroyAccessLock(&audiostate.lock);
    #endif
}
