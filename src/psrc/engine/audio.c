#include "audio.h"

#include "../debug.h"
#include "../logging.h"
#include "../common.h"
#include "../common/config.h"
#include "../common/math/vec3.h"

#include <stdlib.h>
#include <limits.h>

struct audiostate audiostate;

static void mixsound_cb_wav(struct audiosound* s, long loop, long pos, long* start, long* end, int16_t** buf) {
    (void)loop;
    if (!buf && s->rc->is8bit) {
        free(s->wav.cvtbuf);
        unlockRc(s->rc);
    }
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
        tmpend -= pos;
        tmpend *= s->rc->channels;
        uint8_t* datain = s->rc->data;
        for (register long i = 0; i < tmpend; ++i) {
            register int tmp = datain[i];
            dataout[i] = (tmp - 128) * 256 + tmp;
        }
    }
}
#ifdef PSRC_USESTBVORBIS
static void mixsound_cb_vorbis(struct audiosound* s, long loop, long pos, long* start, long* end, int16_t** buf) {
    (void)loop;
    if (!buf) {
        stb_vorbis_close(s->vorbis.state);
        free(s->vorbis.decbuf);
        unlockRc(s->rc);
    }
    *buf = s->vorbis.decbuf;
    if (pos < s->vorbis.decbufhead) {
        pos -= audiostate.decbuflen;
        if (pos < 0) pos = 0;
        stb_vorbis_seek(s->vorbis.state, pos);
    } else if (pos != s->vorbis.decbufhead + s->vorbis.decbuflen) {
        stb_vorbis_seek(s->vorbis.state, pos);
    }
    long tmpend = pos + audiostate.decbuflen;
    if (tmpend > s->rc->len) tmpend = s->rc->len;
    *start = pos;
    s->vorbis.decbufhead = pos;
    *end = tmpend - 1;
    tmpend -= pos;
    s->vorbis.decbuflen = tmpend;
    stb_vorbis_get_samples_short_interleaved(s->vorbis.state, s->rc->channels, s->vorbis.decbuf, tmpend);
}
#endif
#ifdef PSRC_USEMINIMP3
static void mixsound_cb_mp3(struct audiosound* s, long loop, long pos, long* start, long* end, int16_t** buf) {
    (void)loop;
    if (!buf) {
        mp3dec_ex_close(s->mp3.state);
        free(s->mp3.decbuf);
        unlockRc(s->rc);
    }
    *buf = s->mp3.decbuf;
    if (pos < s->mp3.decbufhead) {
        pos -= audiostate.decbuflen;
        if (pos < 0) pos = 0;
        mp3dec_ex_seek(s->mp3.state, pos * s->rc->channels);
    } else if (pos != s->mp3.decbufhead + s->mp3.decbuflen) {
        mp3dec_ex_seek(s->mp3.state, pos * s->rc->channels);
    }
    long tmpend = pos + audiostate.decbuflen;
    if (tmpend > s->rc->len) tmpend = s->rc->len;
    *start = pos;
    s->mp3.decbufhead = pos;
    *end = tmpend - 1;
    tmpend -= pos;
    s->mp3.decbuflen = tmpend;
    mp3dec_ex_read(s->mp3.state, s->mp3.decbuf, tmpend * s->rc->channels);
}
#endif

static inline void deleteSound(struct audiosound* s) {
    audiocb cb;
    void* ctx;
    if (!(s->iflags & SOUNDIFLAG_USESCB)) {
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
    } else {
        cb = s->cb.cb;
        ctx = s->cb.ctx;
    }
    cb(ctx, 0, 0, NULL, NULL, NULL);
    s->prio = AUDIOPRIO_INVALID;
}
static void delete3DSound(size_t si, struct audiosound* s) {
    deleteSound(s);
    --audiostate.sounds3dorder.len;
    if (audiostate.sounds3dorder.data[audiostate.sounds3dorder.len] != si) {
        for (size_t i = 0; i < audiostate.sounds3dorder.len; ++i) {
            if (audiostate.sounds3dorder.data[i] == si) {
                audiostate.sounds3dorder.data[i] = audiostate.sounds3dorder.data[audiostate.sounds3dorder.len];
                break;
            }
        }
    }
}
static void delete2DSound(size_t si, struct audiosound* s) {
    deleteSound(s);
    --audiostate.sounds2dorder.len;
    if (audiostate.sounds2dorder.data[audiostate.sounds2dorder.len] != si) {
        for (size_t i = 0; i < audiostate.sounds2dorder.len; ++i) {
            if (audiostate.sounds2dorder.data[i] == si) {
                audiostate.sounds2dorder.data[i] = audiostate.sounds2dorder.data[audiostate.sounds2dorder.len];
                break;
            }
        }
    }
}

int new3DAudioEmitter(int8_t prio, unsigned maxsounds, unsigned fxmask, const struct audiofx* fx, unsigned fx3dmask, const struct audio3dfx* fx3d) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    struct audioemitter3d* e;
    int ei;
    {
        size_t i = 0;
        while (1) {
            if (i == audiostate.emitters3d.len) {
                if (audiostate.emitters3d.len > (size_t)INT_MAX) {ei = -1; goto ret;}
                ei = audiostate.emitters3d.len;
                VLB_NEXTPTR(audiostate.emitters3d, e, 3, 2, ei = -1; goto ret;);
                break;
            }
            struct audioemitter3d* tmpe = &audiostate.emitters3d.data[i];
            if (tmpe->prio == AUDIOPRIO_INVALID) {
                e = tmpe;
                ei = i;
                break;
            }
            ++i;
        }
    }
    if (prio == AUDIOPRIO_DEFAULT) prio = AUDIOPRIO_NORMAL;
    e->cursounds = 0;
    e->maxsounds = maxsounds;
    e->prio = prio;
    e->fxch = (uint8_t)AUDIOFXMASK_ALL;
    e->paused = 0;
    e->cfximm = (uint8_t)AUDIOFXMASK_ALL;
    if (fx) {
        e->fx.vol[0] = (fxmask & AUDIOFXMASK_VOL_L) ? fx->vol[0] : 1.0f;
        e->fx.vol[1] = (fxmask & AUDIOFXMASK_VOL_R) ? fx->vol[1] : 1.0f;
        e->fx.speed = (fxmask & AUDIOFXMASK_SPEED) ? fx->speed : 1.0f;
        e->fx.lpfilt[0] = (fxmask & AUDIOFXMASK_LPFILT_L) ? fx->lpfilt[0] : 0.0f;
        e->fx.lpfilt[1] = (fxmask & AUDIOFXMASK_LPFILT_R) ? fx->lpfilt[1] : 0.0f;
        e->fx.hpfilt[0] = (fxmask & AUDIOFXMASK_HPFILT_L) ? fx->hpfilt[0] : 0.0f;
        e->fx.hpfilt[1] = (fxmask & AUDIOFXMASK_HPFILT_R) ? fx->hpfilt[1] : 0.0f;
    } else {
        e->fx = (struct audiofx){
            .vol = {1.0f, 1.0f},
            .speed = 1.0f,
            .lpfilt = {0.0f, 0.0f},
            .hpfilt = {0.0f, 0.0f}
        };
    }
    if (fx3d) {
        if (fx3dmask & AUDIO3DFXMASK_POS) {
            e->fx3d.pos[0] = fx3d->pos[0];
            e->fx3d.pos[1] = fx3d->pos[1];
            e->fx3d.pos[2] = fx3d->pos[2];
        } else {
            e->fx3d.pos[0] = 0.0f;
            e->fx3d.pos[1] = 0.0f;
            e->fx3d.pos[2] = 0.0f;
        }
        if (fx3dmask & AUDIO3DFXMASK_RADIUS) {
            e->fx3d.radius[0] = fx3d->radius[0];
            e->fx3d.radius[1] = fx3d->radius[1];
            e->fx3d.radius[2] = fx3d->radius[2];
        } else {
            e->fx3d.radius[0] = 0.0f;
            e->fx3d.radius[1] = 0.0f;
            e->fx3d.radius[2] = 0.0f;
        }
        e->fx3d.voldamp = (fx3dmask & AUDIO3DFXMASK_VOLDAMP) ? fx3d->voldamp : 0.0f;
        e->fx3d.freqdamp = (fx3dmask & AUDIO3DFXMASK_FREQDAMP) ? fx3d->freqdamp : 0.0f;
        e->fx3d.nodoppler = (fx3dmask & AUDIO3DFXMASK_NODOPPLER) ? fx3d->nodoppler : 0;
        e->fx3d.relpos = (fx3dmask & AUDIO3DFXMASK_RELPOS) ? fx3d->relpos : 0;
        e->fx3d.relrot = (fx3dmask & AUDIO3DFXMASK_RELROT) ? fx3d->relrot : 0;
    } else {
        e->fx3d = (struct audio3dfx){
            .pos = {0.0f, 0.0f, 0.0f},
            .radius = {0.0f, 0.0f, 0.0f},
            .voldamp = 0.0f,
            .freqdamp = 0.0f,
            .nodoppler = 0,
            .relpos = 0,
            .relrot = 0
        };
    }
    ret:;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
    return ei;
}
int new2DAudioEmitter(int8_t prio, unsigned maxsounds, unsigned fxmask, const struct audiofx* fx) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    struct audioemitter2d* e;
    int ei;
    {
        size_t i = 0;
        while (1) {
            if (i == audiostate.emitters2d.len) {
                if (audiostate.emitters2d.len > (size_t)INT_MAX) {ei = -1; goto ret;}
                ei = audiostate.emitters2d.len;
                VLB_NEXTPTR(audiostate.emitters2d, e, 3, 2, ei = -1; goto ret;);
                break;
            }
            struct audioemitter2d* tmpe = &audiostate.emitters2d.data[i];
            if (tmpe->prio == AUDIOPRIO_INVALID) {
                e = tmpe;
                ei = i;
                break;
            }
            ++i;
        }
    }
    if (prio == AUDIOPRIO_DEFAULT) prio = AUDIOPRIO_NORMAL;
    e->cursounds = 0;
    e->maxsounds = maxsounds;
    e->prio = prio;
    e->fxch = (uint8_t)AUDIOFXMASK_ALL;
    e->paused = 0;
    e->cfximm = (uint8_t)AUDIOFXMASK_ALL;
    if (fx) {
        e->fx.vol[0] = (fxmask & AUDIOFXMASK_VOL_L) ? fx->vol[0] : 1.0f;
        e->fx.vol[1] = (fxmask & AUDIOFXMASK_VOL_R) ? fx->vol[1] : 1.0f;
        e->fx.speed = (fxmask & AUDIOFXMASK_SPEED) ? fx->speed : 1.0f;
        e->fx.lpfilt[0] = (fxmask & AUDIOFXMASK_LPFILT_L) ? fx->lpfilt[0] : 0.0f;
        e->fx.lpfilt[1] = (fxmask & AUDIOFXMASK_LPFILT_R) ? fx->lpfilt[1] : 0.0f;
        e->fx.hpfilt[0] = (fxmask & AUDIOFXMASK_HPFILT_L) ? fx->hpfilt[0] : 0.0f;
        e->fx.hpfilt[1] = (fxmask & AUDIOFXMASK_HPFILT_R) ? fx->hpfilt[1] : 0.0f;
    } else {
        e->fx = (struct audiofx){
            .vol = {1.0f, 1.0f},
            .speed = 1.0f,
            .lpfilt = {0.0f, 0.0f},
            .hpfilt = {0.0f, 0.0f}
        };
    }
    ret:;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
    return ei;
}

void edit3DAudioEmitter(int ei, unsigned fxmask, const struct audiofx* fx, unsigned fx3dmask, const struct audio3dfx* fx3d, unsigned immcfxmask) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    struct audioemitter3d* e = &audiostate.emitters3d.data[ei];
    e->fxch |= fxmask;
    e->cfximm |= immcfxmask;
    if (fx) {
        if (fxmask & AUDIOFXMASK_VOL_L) e->fx.vol[0] = fx->vol[0];
        if (fxmask & AUDIOFXMASK_VOL_R) e->fx.vol[1] = fx->vol[1];
        if (fxmask & AUDIOFXMASK_SPEED) e->fx.speed = fx->speed;
        if (fxmask & AUDIOFXMASK_LPFILT_L) e->fx.lpfilt[0] = fx->lpfilt[0];
        if (fxmask & AUDIOFXMASK_LPFILT_R) e->fx.lpfilt[1] = fx->lpfilt[1];
        if (fxmask & AUDIOFXMASK_HPFILT_L) e->fx.hpfilt[0] = fx->hpfilt[0];
        if (fxmask & AUDIOFXMASK_HPFILT_R) e->fx.hpfilt[1] = fx->hpfilt[1];
    }
    if (fx3d) {
        if (fx3dmask & AUDIO3DFXMASK_POS) {
            e->fx3d.pos[0] = fx3d->pos[0];
            e->fx3d.pos[1] = fx3d->pos[1];
            e->fx3d.pos[2] = fx3d->pos[2];
        }
        if (fx3dmask & AUDIO3DFXMASK_RADIUS) {
            e->fx3d.radius[0] = fx3d->radius[0];
            e->fx3d.radius[1] = fx3d->radius[1];
            e->fx3d.radius[2] = fx3d->radius[2];
        }
        if (fx3dmask & AUDIO3DFXMASK_VOLDAMP) e->fx3d.voldamp = fx3d->voldamp;
        if (fx3dmask & AUDIO3DFXMASK_FREQDAMP) e->fx3d.freqdamp = fx3d->freqdamp;
        if (fx3dmask & AUDIO3DFXMASK_NODOPPLER) e->fx3d.nodoppler = fx3d->nodoppler;
        if (fx3dmask & AUDIO3DFXMASK_RELPOS) e->fx3d.relpos = fx3d->relpos;
        if (fx3dmask & AUDIO3DFXMASK_RELROT) e->fx3d.relrot = fx3d->relrot;
    }
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void edit2DAudioEmitter(int ei, unsigned fxmask, const struct audiofx* fx, unsigned immcfxmask) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    struct audioemitter2d* e = &audiostate.emitters2d.data[ei];
    e->fxch |= fxmask;
    e->cfximm |= immcfxmask;
    if (fx) {
        if (fxmask & AUDIOFXMASK_VOL_L) e->fx.vol[0] = fx->vol[0];
        if (fxmask & AUDIOFXMASK_VOL_R) e->fx.vol[1] = fx->vol[1];
        if (fxmask & AUDIOFXMASK_SPEED) e->fx.speed = fx->speed;
        if (fxmask & AUDIOFXMASK_LPFILT_L) e->fx.lpfilt[0] = fx->lpfilt[0];
        if (fxmask & AUDIOFXMASK_LPFILT_R) e->fx.lpfilt[1] = fx->lpfilt[1];
        if (fxmask & AUDIOFXMASK_HPFILT_L) e->fx.hpfilt[0] = fx->hpfilt[0];
        if (fxmask & AUDIOFXMASK_HPFILT_R) e->fx.hpfilt[1] = fx->hpfilt[1];
    }
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}

void pause3DAudioEmitter(int ei, bool p) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    audiostate.emitters3d.data[ei].paused = p;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void pause2DAudioEmitter(int ei, bool p) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    audiostate.emitters2d.data[ei].paused = p;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}

static void stop3DAudioEmitter_internal(int ei, struct audioemitter3d* e) {
    for (size_t i = 0; i < audiostate.sounds3d.len; ++i) {
        struct audiosound* s = &audiostate.sounds3d.data[i];
        if (s->prio != AUDIOPRIO_INVALID && s->emitter == ei) {
            delete3DSound(i, s);
            --e->cursounds;
        }
    }
}
static void stop2DAudioEmitter_internal(int ei, struct audioemitter2d* e) {
    for (size_t i = 0; i < audiostate.sounds2d.len; ++i) {
        struct audiosound* s = &audiostate.sounds2d.data[i];
        if (s->prio != AUDIOPRIO_INVALID && s->emitter == ei) {
            delete2DSound(i, s);
            --e->cursounds;
        }
    }
}

void stop3DAudioEmitter(int ei) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    stop3DAudioEmitter_internal(ei, &audiostate.emitters3d.data[ei]);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void stop2DAudioEmitter(int ei) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    stop2DAudioEmitter_internal(ei, &audiostate.emitters2d.data[ei]);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}

void delete3DAudioEmitter(int ei) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    struct audioemitter3d* e = &audiostate.emitters3d.data[ei];
    stop3DAudioEmitter_internal(ei, e);
    e->prio = AUDIOPRIO_INVALID;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void delete2DAudioEmitter(int ei) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    struct audioemitter2d* e = &audiostate.emitters2d.data[ei];
    stop2DAudioEmitter_internal(ei, e);
    e->prio = AUDIOPRIO_INVALID;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}

static void initSound(struct audiosound* s, struct rc_sound* rc, unsigned fxmask, const struct audiofx* fx) {
    lockRc(rc);
    s->rc = rc;
    s->iflags = SOUNDIFLAG_NEEDSINIT;
    if (fx) {
        s->fx.toff = (fxmask & AUDIOFXMASK_TOFF) ? fx->toff : 0;
        s->fx.speed = (fxmask & AUDIOFXMASK_SPEED) ? fx->speed : 1.0f;
        s->fx.vol[0] = (fxmask & AUDIOFXMASK_VOL_L) ? fx->vol[0] : 1.0f;
        s->fx.vol[1] = (fxmask & AUDIOFXMASK_VOL_R) ? fx->vol[1] : 1.0f;
        s->fx.lpfilt[0] = (fxmask & AUDIOFXMASK_LPFILT_L) ? fx->lpfilt[0] : 0.0f;
        s->fx.lpfilt[1] = (fxmask & AUDIOFXMASK_LPFILT_R) ? fx->lpfilt[1] : 0.0f;
        s->fx.hpfilt[0] = (fxmask & AUDIOFXMASK_HPFILT_L) ? fx->hpfilt[0] : 0.0f;
        s->fx.hpfilt[1] = (fxmask & AUDIOFXMASK_HPFILT_R) ? fx->hpfilt[1] : 0.0f;
    } else {
        s->fx = (struct audiofx){
            .vol = {1.0f, 1.0f},
            .speed = 1.0f,
        };
    }
}
bool play3DSound(int ei, struct rc_sound* rc, int8_t prio, uint8_t flags, unsigned fxmask, const struct audiofx* fx) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    struct audioemitter3d* e = &audiostate.emitters3d.data[ei];
    if (e->cursounds == e->maxsounds) goto retfalse;
    struct audiosound* s;
    {
        size_t i = 0;
        while (1) {
            if (i == audiostate.sounds3d.len) {
                if (audiostate.sounds3d.len == (size_t)audiostate.max3dsounds) goto retfalse;
                VLB_NEXTPTR(audiostate.sounds3d, s, 3, 2, goto retfalse;);
                break;
            }
            struct audiosound* tmps = &audiostate.sounds3d.data[i];
            if (tmps->prio == AUDIOPRIO_INVALID) {
                s = tmps;
                break;
            }
            ++i;
        }
        VLB_ADD(audiostate.sounds3dorder, i, 3, 2, goto retfalse;);
    }
    ++e->cursounds;
    if (prio == AUDIOPRIO_DEFAULT) prio = audiostate.emitters3d.data[ei].prio;
    s->emitter = ei;
    s->prio = prio;
    s->flags = flags;
    initSound(s, rc, fxmask, fx);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
    return true;
    retfalse:;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
    return false;
}
bool play2DSound(int ei, struct rc_sound* rc, int8_t prio, uint8_t flags, unsigned fxmask, const struct audiofx* fx) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    struct audioemitter2d* e = &audiostate.emitters2d.data[ei];
    if (e->cursounds == e->maxsounds) goto retfalse;
    struct audiosound* s;
    {
        size_t i = 0;
        while (1) {
            if (i == audiostate.sounds2d.len) {
                if (audiostate.sounds2d.len == (size_t)audiostate.max2dsounds) goto retfalse;
                VLB_NEXTPTR(audiostate.sounds2d, s, 3, 2, goto retfalse;);
                break;
            }
            struct audiosound* tmps = &audiostate.sounds2d.data[i];
            if (tmps->prio == AUDIOPRIO_INVALID) {
                s = tmps;
                break;
            }
            ++i;
        }
        VLB_ADD(audiostate.sounds2dorder, i, 3, 2, goto retfalse;);
    }
    ++e->cursounds;
    if (prio == AUDIOPRIO_DEFAULT) prio = audiostate.emitters2d.data[ei].prio;
    s->emitter = ei;
    s->prio = prio;
    s->flags = flags;
    initSound(s, rc, fxmask, fx);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
    return true;
    retfalse:;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
    return false;
}

static void doLPFilter(unsigned mul, unsigned div, int16_t* lastoutp, unsigned len, int* buf) {
    register int lastout = *lastoutp;
    for (register unsigned i = 0; i < len; ++i) {
        register int s = buf[i];
        buf[i] = lastout = lastout + (s - lastout) * mul / div;
    }
    *lastoutp = lastout;
}
static void doHPFilter(unsigned mul, unsigned div, int16_t* lastinp, int16_t* lastoutp, unsigned len, int* buf) {
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
static void doLPFilter_interp(unsigned mul1, unsigned mul2, unsigned div, int16_t* lastoutp, unsigned len, int* buf) {
    register int lastout = *lastoutp;
    for (register unsigned i = 0, ii = len; i < len; ++i, --ii) {
        register int s = buf[i];
        buf[i] = lastout = lastout + (s - lastout) * ((mul1 * ii + mul2 * i) / len) / div;
    }
    *lastoutp = lastout;
}
static void doHPFilter_interp(unsigned mul1, unsigned mul2, unsigned div, int16_t* lastinp, int16_t* lastoutp, unsigned len, int* buf) {
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
static bool adjfilters = true;
#define ADJLPFILTMUL(a, m, f) do {\
    if (m < audiostate.freq) {\
        int m2 = (int)roundf(a * a * f);\
        if (m2 < f) {\
            if (audiostate.freq < f) {\
                m += m2;\
                m /= 2;\
            } else {\
                m = m2;\
            }\
            if (m > audiostate.freq) m = audiostate.freq;\
        }\
    }\
} while (0)
#define ADJHPFILTMUL(a, m, f) do {\
    if (m > 0) {\
        int m2 = (int)roundf(a * a * f);\
        if (m2 > 0) {\
            m += m2;\
            m /= 2;\
            if (m > audiostate.freq) m = audiostate.freq;\
        }\
    }\
} while (0)

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
            sample_l =+ sample_r;\
            sample_l /= 2;\
            audiostate.fxbuf[0][i] = sample_l;\
            audiostate.fxbuf[1][i] = sample_l;\
        } else {\
            audiostate.fxbuf[0][i] = sample_l;\
            audiostate.fxbuf[1][i] = sample_r;\
        }\
    } else {\
        audiostate.fxbuf[0][i] = 0;\
        audiostate.fxbuf[1][i] = 0;\
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
static bool mixsound(struct audiosound* s, bool fake, bool mixmono, int oldvol, int newvol) {
    audiocb cb;
    void* ctx;
    long len;
    long freq;
    unsigned ch;
    if (!(s->iflags & SOUNDIFLAG_USESCB)) {
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

    uint_fast8_t curfxi = s->fxi;
    uint_fast8_t newfxi = (curfxi + 1) % 2;

    int vol[2][2];
    if (!fake) {
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
        if (!vol[0][0] && !vol[0][1] && !vol[1][0] && !vol[1][1]) fake = true;
    }

    int16_t* buf;
    long bufstart = 0, bufend = -1;

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
            unsigned tmp = s->calcfx[curfxi].hpfiltmul[0];
            if (tmp != audiostate.freq)
                doHPFilter(tmp, audiostate.freq, &s->hplastin[0], &s->hplastout[0], audiostate.buflen, audiostate.fxbuf[0]);
            tmp = s->calcfx[curfxi].hpfiltmul[1];
            if (tmp != audiostate.freq)
                doHPFilter(tmp, audiostate.freq, &s->hplastin[1], &s->hplastout[1], audiostate.buflen, audiostate.fxbuf[1]);
            tmp = s->calcfx[curfxi].lpfiltmul[0];
            if (tmp != audiostate.freq)
                doLPFilter(tmp, audiostate.freq, &s->lplastout[0], audiostate.buflen, audiostate.fxbuf[0]);
            tmp = s->calcfx[curfxi].lpfiltmul[1];
            if (tmp != audiostate.freq)
                doLPFilter(tmp, audiostate.freq, &s->lplastout[1], audiostate.buflen, audiostate.fxbuf[1]);
        } else {
            unsigned tmp1 = s->calcfx[curfxi].hpfiltmul[0];
            unsigned tmp2 = s->calcfx[newfxi].hpfiltmul[0];
            if (tmp1 != audiostate.freq || tmp2 != audiostate.freq)
                doHPFilter_interp(tmp1, tmp2, audiostate.freq, &s->hplastin[0], &s->hplastout[0], audiostate.buflen, audiostate.fxbuf[0]);
            tmp1 = s->calcfx[curfxi].hpfiltmul[1];
            tmp2 = s->calcfx[newfxi].hpfiltmul[1];
            if (tmp1 != audiostate.freq || tmp2 != audiostate.freq)
                doHPFilter_interp(tmp1, tmp2, audiostate.freq, &s->hplastin[1], &s->hplastout[1], audiostate.buflen, audiostate.fxbuf[1]);
            tmp1 = s->calcfx[curfxi].lpfiltmul[0];
            tmp2 = s->calcfx[newfxi].lpfiltmul[0];
            if (tmp1 != audiostate.freq || tmp2 != audiostate.freq)
                doLPFilter_interp(tmp1, tmp2, audiostate.freq, &s->lplastout[0], audiostate.buflen, audiostate.fxbuf[0]);
            tmp1 = s->calcfx[curfxi].lpfiltmul[1];
            tmp2 = s->calcfx[newfxi].lpfiltmul[1];
            if (tmp1 != audiostate.freq || tmp2 != audiostate.freq)
                doLPFilter_interp(tmp1, tmp2, audiostate.freq, &s->lplastout[1], audiostate.buflen, audiostate.fxbuf[1]);
        }
        for (register unsigned i = 0, ii = audiostate.buflen; i < audiostate.buflen; ++i, --ii) {
            audiostate.mixbuf[0][i] += audiostate.fxbuf[0][i] * ((vol[0][0] * ii + vol[1][0] * i) / audiostate.buflen) / 32768;
            audiostate.mixbuf[1][i] += audiostate.fxbuf[1][i] * ((vol[0][1] * ii + vol[1][1] * i) / audiostate.buflen) / 32768;
        }
    }

    if (!(flags & SOUNDFLAG_LOOP)) {
        if (loop > 0) {
            if (loop > oldloop) goto retfalse;
        } else if (loop < ((!(flags & SOUNDFLAG_WRAP)) ? 0 : -1)) {
            if (loop < oldloop) goto retfalse;
        }
    } else if (!(flags & SOUNDFLAG_WRAP)) {
        if (loop < 0) {
            if (loop < oldloop) goto retfalse;
        }
    }

    s->loop = loop;
    s->pos = pos;
    s->frac = frac;
    if (s->iflags & SOUNDIFLAG_FXCH) {
        s->iflags &= ~SOUNDIFLAG_FXCH;
        s->fxi = newfxi;
    }
    return true;

    retfalse:;
    return false;
}

static inline void calc3DEmitterFx(struct audioemitter3d* e) {
    float mincoord[3] = {
        e->fx3d.pos[0] - e->fx3d.radius[0],
        e->fx3d.pos[1] - e->fx3d.radius[1],
        e->fx3d.pos[2] - e->fx3d.radius[2]
    };
    float maxcoord[3] = {
        e->fx3d.pos[0] + e->fx3d.radius[0],
        e->fx3d.pos[1] + e->fx3d.radius[1],
        e->fx3d.pos[2] + e->fx3d.radius[2]
    };
    if (!e->fx3d.relpos) {
        mincoord[0] -= audiostate.cam.pos[0];
        mincoord[1] -= audiostate.cam.pos[1];
        mincoord[2] -= audiostate.cam.pos[2];
        maxcoord[0] -= audiostate.cam.pos[0];
        maxcoord[1] -= audiostate.cam.pos[1];
        maxcoord[2] -= audiostate.cam.pos[2];
    }
    float pos[3] = {
        (maxcoord[0] > 0.0f) ? ((mincoord[0] < 0.0f) ? 0.0f : mincoord[0]) : maxcoord[0],
        (maxcoord[1] > 0.0f) ? ((mincoord[1] < 0.0f) ? 0.0f : mincoord[1]) : maxcoord[1],
        (maxcoord[2] > 0.0f) ? ((mincoord[2] < 0.0f) ? 0.0f : mincoord[2]) : maxcoord[2]
    };
    float dist = sqrt(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
    e->dist = dist;
    if (dist > 0.0f) {
        pos[0] /= dist;
        pos[1] /= dist;
        pos[2] /= dist;
    }
    if (!e->fx3d.relrot) vec3_trigrotate(pos, audiostate.cam.sin, audiostate.cam.cos, pos);
    if (!e->fx3d.nodoppler) {
        long posoff = dist * -audiostate.soundspeedmul;
        if (posoff != e->fx3dout.posoff) {
            e->fx3dout.posoff = posoff;
            e->fxch |= AUDIOFXMASK_TOFF;
        }
    } else if (e->fx3dout.posoff) {
        e->fx3dout.posoff = 0;
        e->fxch |= AUDIOFXMASK_TOFF;
    }
    float vol[2];
    {
        float tmp = dist * dist;
        if (tmp < 0.5f) tmp = 0.5f;
        vol[1] = vol[0] = 1.0f / tmp;
    }
    float lpfilt[2] = {0.0f, 0.0f};
    float hpfilt[2] = {0.0f, 0.0f};
    if (pos[2] > 0.0f) {
        pos[0] *= 1.0f - 0.2f * pos[2];
    } else if (pos[2] < 0.0f) {
        pos[0] *= 1.0f - 0.25f * pos[2];
        vol[0] *= 1.0f + 0.25f * pos[2];
        vol[1] *= 1.0f + 0.25f * pos[2];
        lpfilt[0] += pos[2] * -0.1f;
        lpfilt[1] += pos[2] * -0.1f;
    }
    if (pos[1] > 0.0f) {
        pos[0] *= 1.0f - 0.2f * pos[1];
        vol[0] *= 1.0f - 0.1f * pos[1];
        vol[1] *= 1.0f - 0.1f * pos[1];
        hpfilt[0] += pos[1] * 0.1f;
        hpfilt[1] += pos[1] * 0.1f;
    } else if (pos[1] < 0.0f) {
        pos[0] *= 1.0f - 0.2f * pos[1];
        lpfilt[0] += pos[1] * -0.1f;
        lpfilt[1] += pos[1] * -0.1f;
    }
    if (pos[0] > 0.0f) {
        vol[0] *= 1.0f - pos[0] * 0.7f;
        hpfilt[0] += pos[0] * 0.2f;
    } else if (pos[0] < 0.0f) {
        vol[1] *= 1.0f + pos[0] * 0.7f;
        hpfilt[1] += pos[0] * 0.2f;
    }
    {
        float tmp = 0.5f - pos[2] * 0.5f;
        hpfilt[0] *= tmp;
        hpfilt[1] *= tmp;
    }
    if (vol[0] != e->fx3dout.volmul[0] || vol[1] != e->fx3dout.volmul[1]) {
        e->fx3dout.volmul[0] = vol[0];
        e->fx3dout.volmul[1] = vol[1];
        e->fxch |= AUDIOFXMASK_VOL;
    }
    if (lpfilt[0] != e->fx3dout.lpfiltmul[0] || lpfilt[1] != e->fx3dout.lpfiltmul[1] ||\
        hpfilt[0] != e->fx3dout.hpfiltmul[0] || hpfilt[1] != e->fx3dout.hpfiltmul[1]) {
        e->fx3dout.lpfiltmul[0] = lpfilt[0];
        e->fx3dout.lpfiltmul[1] = lpfilt[1];
        e->fx3dout.hpfiltmul[0] = hpfilt[0];
        e->fx3dout.hpfiltmul[1] = hpfilt[1];
    }
}
static inline void calc3DSoundFx(struct audiosound* s, struct audioemitter3d* e) {
    uint8_t fxch;
    if (s->iflags & SOUNDIFLAG_NEEDSINIT) {
        fxch = (uint8_t)AUDIOFXMASK_ALL;
        s->iflags &= ~SOUNDIFLAG_NEEDSINIT;
    } else {
        fxch = e->fxch;
        if (!fxch) return;
    }
    uint8_t imm = e->cfximm;
    uint8_t curfxi = s->fxi;
    uint8_t newfxi = (curfxi + 1) % 2;
    if (fxch & AUDIOFXMASK_TOFF) {
        int64_t toff = (e->fx.toff + s->fx.toff) * audiostate.freq / 1000000 + e->fx3dout.posoff;
        if (imm & AUDIOFXMASK_TOFF) {
            long oldposoff = s->calcfx[curfxi].posoff;
            s->calcfx[curfxi].posoff = s->calcfx[newfxi].posoff = toff;
            toff -= oldposoff;
            toff *= ((!(s->iflags & SOUNDIFLAG_USESCB)) ? s->rc->freq : s->cb.freq);
            long loop = s->loop;
            long pos = s->pos;
            long frac = s->frac;
            pos += toff / audiostate.freq;
            frac += (toff % audiostate.freq) * 256;
            long div = audiostate.freq * 256;
            pos += frac / div;
            frac %= div;
            if (frac < 0) {
                frac += div;
                --pos;
            }
            long len = ((!(s->iflags & SOUNDIFLAG_USESCB)) ? s->rc->len : s->cb.len);
            if (pos >= len) {
                loop += pos / len;
                pos %= len;
            } else if (pos < 0) {
                loop += pos / len - 1;
                pos %= len;
                pos += len;
            }
            s->loop = loop;
            s->pos = pos;
            s->frac = frac;
        } else {
            s->calcfx[newfxi].posoff = toff;
            s->iflags |= SOUNDIFLAG_FXCH_OTHER;
        }
    }
    if (fxch & AUDIOFXMASK_SPEED) {
        s->calcfx[newfxi].speedmul = roundf(s->fx.speed * e->fx.speed * 256.0f);
        if (imm & AUDIOFXMASK_SPEED) s->calcfx[curfxi].speedmul = s->calcfx[newfxi].speedmul;
        else s->iflags |= SOUNDIFLAG_FXCH_OTHER;
    }
    if (fxch & AUDIOFXMASK_VOL) {
        s->calcfx[newfxi].volmul[0] = roundf(s->fx.vol[0] * e->fx3dout.volmul[0] * e->fx.vol[0] * 32768.0f);
        s->calcfx[newfxi].volmul[1] = roundf(s->fx.vol[1] * e->fx3dout.volmul[1] * e->fx.vol[1] * 32768.0f);
        if (imm & AUDIOFXMASK_VOL) {
            if (imm & AUDIOFXMASK_VOL_L) s->calcfx[curfxi].volmul[0] = s->calcfx[newfxi].volmul[0];
            if (imm & AUDIOFXMASK_VOL_R) s->calcfx[curfxi].volmul[1] = s->calcfx[newfxi].volmul[1];
            s->iflags |= SOUNDIFLAG_FXCH_VOL;
        }
    }
    if (fxch & AUDIOFXMASK_LPFILT) {
        float tmpf = (1.0f - s->fx.lpfilt[0]) * (1.0f - e->fx3dout.lpfiltmul[0]) * (1.0f - e->fx.lpfilt[0]);
        unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters && audiostate.freq != 44100) ADJLPFILTMUL(tmpf, tmpu, 44100);
        if (tmpu > audiostate.freq) tmpu = audiostate.freq;
        s->calcfx[newfxi].lpfiltmul[0] = tmpu;
        if (imm & AUDIOFXMASK_LPFILT_L) s->calcfx[curfxi].lpfiltmul[0] = tmpu;
        tmpf = (1.0f - s->fx.lpfilt[1]) * (1.0f - e->fx3dout.lpfiltmul[1]) * (1.0f - e->fx.lpfilt[1]);
        tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters && audiostate.freq != 44100) ADJLPFILTMUL(tmpf, tmpu, 44100);
        if (tmpu > audiostate.freq) tmpu = audiostate.freq;
        s->calcfx[newfxi].lpfiltmul[1] = tmpu;
        if (imm & AUDIOFXMASK_LPFILT_R) s->calcfx[curfxi].lpfiltmul[1] = tmpu;
        if (!(imm & AUDIOFXMASK_LPFILT)) s->iflags |= SOUNDIFLAG_FXCH_FILT;
    }
    if (fxch & AUDIOFXMASK_HPFILT) {
        float tmpf = 1.0f - (1.0f - s->fx.hpfilt[0]) * (1.0f - e->fx3dout.hpfiltmul[0]) * (1.0f - e->fx.hpfilt[0]);
        unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters && audiostate.freq != 44100) ADJHPFILTMUL(tmpf, tmpu, 44100);
        if (tmpu >= audiostate.freq) tmpu = 0;
        else tmpu = audiostate.freq - tmpu;
        s->calcfx[newfxi].hpfiltmul[0] = tmpu;
        if (imm & AUDIOFXMASK_HPFILT_L) s->calcfx[curfxi].hpfiltmul[0] = tmpu;
        tmpf = 1.0f - (1.0f - s->fx.hpfilt[1]) * (1.0f - e->fx3dout.hpfiltmul[1]) * (1.0f - e->fx.hpfilt[1]);
        tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters && audiostate.freq != 44100) ADJHPFILTMUL(tmpf, tmpu, 44100);
        if (tmpu >= audiostate.freq) tmpu = 0;
        else tmpu = audiostate.freq - tmpu;
        s->calcfx[newfxi].hpfiltmul[1] = tmpu;
        if (imm & AUDIOFXMASK_HPFILT_R) s->calcfx[curfxi].hpfiltmul[1] = tmpu;
        if (!(imm & AUDIOFXMASK_HPFILT)) s->iflags |= SOUNDIFLAG_FXCH_FILT;
    }
}

static inline void calc2DSoundFx(struct audiosound* s, struct audioemitter2d* e) {
    uint8_t fxch;
    if (s->iflags & SOUNDIFLAG_NEEDSINIT) {
        fxch = (uint8_t)AUDIOFXMASK_ALL;
        s->iflags &= ~SOUNDIFLAG_NEEDSINIT;
    } else {
        fxch = e->fxch;
        if (!fxch) return;
    }
    uint8_t imm = e->cfximm;
    uint8_t curfxi = s->fxi;
    uint8_t newfxi = (curfxi + 1) % 2;
    if (fxch & AUDIOFXMASK_TOFF) {
        int64_t toff = (e->fx.toff + s->fx.toff) * audiostate.freq / 1000000;
        if (imm & AUDIOFXMASK_TOFF) {
            long oldposoff = s->calcfx[curfxi].posoff;
            s->calcfx[curfxi].posoff = s->calcfx[newfxi].posoff = toff;
            toff -= oldposoff;
            toff *= ((!(s->iflags & SOUNDIFLAG_USESCB)) ? s->rc->freq : s->cb.freq);
            long loop = s->loop;
            long pos = s->pos;
            long frac = s->frac;
            pos += toff / audiostate.freq;
            frac += (toff % audiostate.freq) * 256;
            long div = audiostate.freq * 256;
            pos += frac / div;
            frac %= div;
            if (frac < 0) {
                frac += div;
                --pos;
            }
            long len = ((!(s->iflags & SOUNDIFLAG_USESCB)) ? s->rc->len : s->cb.len);
            if (pos >= len) {
                loop += pos / len;
                pos %= len;
            } else if (pos < 0) {
                loop += pos / len - 1;
                pos %= len;
                pos += len;
            }
            s->loop = loop;
            s->pos = pos;
            s->frac = frac;
        } else {
            s->calcfx[newfxi].posoff = toff;
            s->iflags |= SOUNDIFLAG_FXCH_OTHER;
        }
    }
    if (fxch & AUDIOFXMASK_SPEED) {
        s->calcfx[newfxi].speedmul = roundf(s->fx.speed * e->fx.speed * 256.0f);
        if (imm & AUDIOFXMASK_SPEED) s->calcfx[curfxi].speedmul = s->calcfx[newfxi].speedmul;
        else s->iflags |= SOUNDIFLAG_FXCH_OTHER;
    }
    if (fxch & AUDIOFXMASK_VOL) {
        s->calcfx[newfxi].volmul[0] = roundf(s->fx.vol[0] * e->fx.vol[0] * 32768.0f);
        s->calcfx[newfxi].volmul[1] = roundf(s->fx.vol[1] * e->fx.vol[1] * 32768.0f);
        if (imm & AUDIOFXMASK_VOL) {
            if (imm & AUDIOFXMASK_VOL_L) s->calcfx[curfxi].volmul[0] = s->calcfx[newfxi].volmul[0];
            if (imm & AUDIOFXMASK_VOL_R) s->calcfx[curfxi].volmul[1] = s->calcfx[newfxi].volmul[1];
            s->iflags |= SOUNDIFLAG_FXCH_VOL;
        }
    }
    if (fxch & AUDIOFXMASK_LPFILT) {
        float tmpf = (1.0f - s->fx.lpfilt[0]) * (1.0f - e->fx.lpfilt[0]);
        unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters && audiostate.freq != 44100) ADJLPFILTMUL(tmpf, tmpu, 44100);
        if (tmpu > audiostate.freq) tmpu = audiostate.freq;
        s->calcfx[newfxi].lpfiltmul[0] = tmpu;
        if (imm & AUDIOFXMASK_LPFILT_L) s->calcfx[curfxi].lpfiltmul[0] = tmpu;
        tmpf = (1.0f - s->fx.lpfilt[1]) * (1.0f - e->fx.lpfilt[1]);
        tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters && audiostate.freq != 44100) ADJLPFILTMUL(tmpf, tmpu, 44100);
        if (tmpu > audiostate.freq) tmpu = audiostate.freq;
        s->calcfx[newfxi].lpfiltmul[1] = tmpu;
        if (imm & AUDIOFXMASK_LPFILT_R) s->calcfx[curfxi].lpfiltmul[1] = tmpu;
        if (!(imm & AUDIOFXMASK_LPFILT)) s->iflags |= SOUNDIFLAG_FXCH_FILT;
    }
    if (fxch & AUDIOFXMASK_HPFILT) {
        float tmpf = 1.0f - (1.0f - s->fx.hpfilt[0]) * (1.0f - e->fx.hpfilt[0]);
        unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters && audiostate.freq != 44100) ADJHPFILTMUL(tmpf, tmpu, 44100);
        if (tmpu >= audiostate.freq) tmpu = 0;
        else tmpu = audiostate.freq - tmpu;
        s->calcfx[newfxi].hpfiltmul[0] = tmpu;
        if (imm & AUDIOFXMASK_HPFILT_L) s->calcfx[curfxi].hpfiltmul[0] = tmpu;
        tmpf = 1.0f - (1.0f - s->fx.hpfilt[1]) * (1.0f - e->fx.hpfilt[1]);
        tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters && audiostate.freq != 44100) ADJHPFILTMUL(tmpf, tmpu, 44100);
        if (tmpu >= audiostate.freq) tmpu = 0;
        else tmpu = audiostate.freq - tmpu;
        s->calcfx[newfxi].hpfiltmul[1] = tmpu;
        if (imm & AUDIOFXMASK_HPFILT_R) s->calcfx[curfxi].hpfiltmul[1] = tmpu;
        if (!(imm & AUDIOFXMASK_HPFILT)) s->iflags |= SOUNDIFLAG_FXCH_FILT;
    }
}

static int mixsounds_3dsortcb(const void* a, const void* b) {
    struct audiosound* s1 = &audiostate.sounds3d.data[*(size_t*)a];
    struct audiosound* s2 = &audiostate.sounds3d.data[*(size_t*)b];
    struct audioemitter3d* e1 = &audiostate.emitters3d.data[s1->emitter];
    struct audioemitter3d* e2 = &audiostate.emitters3d.data[s2->emitter];
    if (e1->paused != e2->paused) return (int)e1->paused - (int)e2->paused;
    if (s1->prio == s2->prio) {
        return (e1->dist != e2->dist) ? ((e1->dist > e2->dist) ? 1 : -1) : 0;
    } else {
        return (int)s2->prio - (int)s1->prio;
    }
}
static int mixsounds_2dsortcb(const void* a, const void* b) {
    struct audiosound* s1 = &audiostate.sounds2d.data[*(size_t*)a];
    struct audiosound* s2 = &audiostate.sounds2d.data[*(size_t*)b];
    struct audioemitter2d* e1 = &audiostate.emitters2d.data[s1->emitter];
    struct audioemitter2d* e2 = &audiostate.emitters2d.data[s2->emitter];
    if (e1->paused != e2->paused) return (int)e1->paused - (int)e2->paused;
    return (int)s2->prio - (int)s1->prio;
}
static void mixsounds(unsigned buf) {
    for (size_t i = 0; i < audiostate.emitters3d.len; ++i) {
        struct audioemitter3d* e = &audiostate.emitters3d.data[i];
        if (!e->paused) calc3DEmitterFx(e);
    }
    for (size_t i = 0; i < audiostate.sounds3d.len; ++i) {
        struct audiosound* s = &audiostate.sounds3d.data[i];
        struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
        if (!e->paused) calc3DSoundFx(s, e);
    }
    for (size_t i = 0; i < audiostate.sounds2d.len; ++i) {
        struct audiosound* s = &audiostate.sounds2d.data[i];
        struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
        if (!e->paused) calc2DSoundFx(s, e);
    }
    for (size_t i = 0; i < audiostate.emitters3d.len; ++i) {
        struct audioemitter3d* e = &audiostate.emitters3d.data[i];
        if (e->fxch) e->fxch = 0;
        if (e->cfximm) e->cfximm = 0;
    }
    for (size_t i = 0; i < audiostate.emitters2d.len; ++i) {
        struct audioemitter2d* e = &audiostate.emitters2d.data[i];
        if (e->fxch) e->fxch = 0;
        if (e->cfximm) e->cfximm = 0;
    }

    memset(audiostate.mixbuf[0], 0, audiostate.buflen * sizeof(**audiostate.mixbuf));
    memset(audiostate.mixbuf[1], 0, audiostate.buflen * sizeof(**audiostate.mixbuf));

    {
        size_t tmp = audiostate.sounds3d.len;
        if (tmp) {
            if (tmp > 1) qsort(audiostate.sounds3dorder.data, tmp, sizeof(*audiostate.sounds3dorder.data), mixsounds_3dsortcb);
            if (tmp <= audiostate.voices3d) {
                for (size_t i = 0; i < tmp; ++i) {
                    struct audiosound* s = &audiostate.sounds3d.data[audiostate.sounds3dorder.data[i]];
                    struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
                    if (!e->paused && !mixsound(s, false, true, 32768, 32768)) delete3DSound(i, s);
                }
            } else {
                for (size_t i = 0; i < audiostate.voices3d; ++i) {
                    struct audiosound* s = &audiostate.sounds3d.data[audiostate.sounds3dorder.data[i]];
                    struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
                    if (!e->paused && !mixsound(s, false, true, 32768, 32768)) delete3DSound(i, s);
                }
                for (size_t i = audiostate.voices3d; i < tmp; ++i) {
                    struct audiosound* s = &audiostate.sounds3d.data[audiostate.sounds3dorder.data[i]];
                    struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
                    if (!e->paused && !mixsound(s, true, true, 32768, 32768)) delete3DSound(i, s);
                }
            }
        }
        tmp = audiostate.sounds2d.len;
        if (tmp) {
            if (tmp > 1) qsort(audiostate.sounds2dorder.data, tmp, sizeof(*audiostate.sounds2dorder.data), mixsounds_2dsortcb);
            if (tmp <= audiostate.voices2d) {
                for (size_t i = 0; i < tmp; ++i) {
                    struct audiosound* s = &audiostate.sounds2d.data[audiostate.sounds2dorder.data[i]];
                    struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
                    if (!e->paused && !mixsound(s, false, false, 32768, 32768)) delete2DSound(i, s);
                }
            } else {
                for (size_t i = 0; i < audiostate.voices2d; ++i) {
                    struct audiosound* s = &audiostate.sounds2d.data[audiostate.sounds2dorder.data[i]];
                    struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
                    if (!e->paused && !mixsound(s, false, false, 32768, 32768)) delete2DSound(i, s);
                }
                for (size_t i = audiostate.voices2d; i < tmp; ++i) {
                    struct audiosound* s = &audiostate.sounds2d.data[audiostate.sounds2dorder.data[i]];
                    struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
                    if (!e->paused && !mixsound(s, true, false, 32768, 32768)) delete2DSound(i, s);
                }
            }
        }
    }

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
    vec3_calctrig(audiostate.cam.rot, audiostate.cam.sin, audiostate.cam.cos); // TODO: copy from renderer instead of recalc
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
    audiostate.soundspeedmul = (float)outspec.freq / 343.0f;
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
