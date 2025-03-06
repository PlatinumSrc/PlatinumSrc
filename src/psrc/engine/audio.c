#include "audio.h"

#include "../debug.h"
#include "../attribs.h"
#include "../logging.h"
#include "../common.h"
#include "../common/config.h"
#include "../common/math/vec3.h"

#include <stdlib.h>
#include <limits.h>

struct audiostate audiostate;

static void mixsound_cb_wav(struct audiosound* s, long loop, long pos, long* start, long* end, int16_t** buf) {
    (void)loop;
    if (!buf) {
        if (s->rc->is8bit) free(s->wav.cvtbuf);
        unlockRc(s->rc);
        return;
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
        for (register long i = 0, i2 = pos * s->rc->channels; i < tmpend; ++i, ++i2) {
            register int tmp = datain[i2];
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
        return;
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
        return;
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

int new3DAudioEmitter(int8_t prio, unsigned maxsnd, unsigned f, unsigned fxmask, const struct audiofx* fx, unsigned fx3dmask, const struct audio3dfx* fx3d) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    int ei;
    if (!audiostate.valid) {ei = -1; goto ret;}
    struct audioemitter3d* e;
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
    e->maxsounds = maxsnd;
    e->prio = prio;
    e->flags = f;
    e->fxch = (uint8_t)AUDIOFXMASK_ALL;
    e->fxchimm = (uint8_t)AUDIOFXMASK_ALL;
    if (fx) {
        e->fx.toff = (fxmask & AUDIOFXMASK_TOFF) ? fx->toff : 0;
        e->fx.speed = (fxmask & AUDIOFXMASK_SPEED) ? fx->speed : 1.0f;
        e->fx.vol[0] = (fxmask & AUDIOFXMASK_VOL_L) ? fx->vol[0] : 1.0f;
        e->fx.vol[1] = (fxmask & AUDIOFXMASK_VOL_R) ? fx->vol[1] : 1.0f;
        e->fx.lpfilt[0] = (fxmask & AUDIOFXMASK_LPFILT_L) ? fx->lpfilt[0] : 0.0f;
        e->fx.lpfilt[1] = (fxmask & AUDIOFXMASK_LPFILT_R) ? fx->lpfilt[1] : 0.0f;
        e->fx.hpfilt[0] = (fxmask & AUDIOFXMASK_HPFILT_L) ? fx->hpfilt[0] : 0.0f;
        e->fx.hpfilt[1] = (fxmask & AUDIOFXMASK_HPFILT_R) ? fx->hpfilt[1] : 0.0f;
    } else {
        e->fx = (struct audiofx){
            .vol = {1.0f, 1.0f},
            .speed = 1.0f,
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
        e->fx3d.range = (fx3dmask & AUDIO3DFXMASK_RANGE) ? fx3d->range : 25.0f;
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
            .range = 40.0f
        };
    }
    ret:;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
    return ei;
}
int new2DAudioEmitter(int8_t prio, unsigned maxsnd, unsigned f, unsigned fxmask, const struct audiofx* fx) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    int ei;
    if (!audiostate.valid) {ei = -1; goto ret;}
    struct audioemitter2d* e;
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
    e->maxsounds = maxsnd;
    e->prio = prio;
    e->flags = f;
    e->fxch = (uint8_t)AUDIOFXMASK_ALL;
    e->fxchimm = (uint8_t)AUDIOFXMASK_ALL;
    if (fx) {
        e->fx.toff = (fxmask & AUDIOFXMASK_TOFF) ? fx->toff : 0;
        e->fx.speed = (fxmask & AUDIOFXMASK_SPEED) ? fx->speed : 1.0f;
        e->fx.vol[0] = (fxmask & AUDIOFXMASK_VOL_L) ? fx->vol[0] : 1.0f;
        e->fx.vol[1] = (fxmask & AUDIOFXMASK_VOL_R) ? fx->vol[1] : 1.0f;
        e->fx.lpfilt[0] = (fxmask & AUDIOFXMASK_LPFILT_L) ? fx->lpfilt[0] : 0.0f;
        e->fx.lpfilt[1] = (fxmask & AUDIOFXMASK_LPFILT_R) ? fx->lpfilt[1] : 0.0f;
        e->fx.hpfilt[0] = (fxmask & AUDIOFXMASK_HPFILT_L) ? fx->hpfilt[0] : 0.0f;
        e->fx.hpfilt[1] = (fxmask & AUDIOFXMASK_HPFILT_R) ? fx->hpfilt[1] : 0.0f;
    } else {
        e->fx = (struct audiofx){
            .vol = {1.0f, 1.0f},
            .speed = 1.0f,
        };
    }
    ret:;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
    return ei;
}

void edit3DAudioEmitter(int ei, unsigned fen, unsigned fdis, unsigned fxmask, const struct audiofx* fx, unsigned fx3dmask, const struct audio3dfx* fx3d, unsigned imm) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) goto ret;
    struct audioemitter3d* e = &audiostate.emitters3d.data[ei];
    e->flags |= fen;
    e->flags &= ~fdis;
    e->fxch |= fxmask;
    e->fxchimm |= imm;
    if (fx) {
        if (fxmask & AUDIOFXMASK_TOFF) e->fx.toff = fx->toff;
        if (fxmask & AUDIOFXMASK_SPEED) e->fx.speed = fx->speed;
        if (fxmask & AUDIOFXMASK_VOL_L) e->fx.vol[0] = fx->vol[0];
        if (fxmask & AUDIOFXMASK_VOL_R) e->fx.vol[1] = fx->vol[1];
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
        if (fx3dmask & AUDIO3DFXMASK_RANGE) e->fx3d.range = fx3d->range;
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
    ret:;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void edit2DAudioEmitter(int ei, unsigned fen, unsigned fdis, unsigned fxmask, const struct audiofx* fx, unsigned imm) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) goto ret;
    struct audioemitter2d* e = &audiostate.emitters2d.data[ei];
    e->flags |= fen;
    e->flags &= ~fdis;
    e->fxch |= fxmask;
    e->fxchimm |= imm;
    if (fx) {
        if (fxmask & AUDIOFXMASK_TOFF) e->fx.toff = fx->toff;
        if (fxmask & AUDIOFXMASK_SPEED) e->fx.speed = fx->speed;
        if (fxmask & AUDIOFXMASK_VOL_L) e->fx.vol[0] = fx->vol[0];
        if (fxmask & AUDIOFXMASK_VOL_R) e->fx.vol[1] = fx->vol[1];
        if (fxmask & AUDIOFXMASK_LPFILT_L) e->fx.lpfilt[0] = fx->lpfilt[0];
        if (fxmask & AUDIOFXMASK_LPFILT_R) e->fx.lpfilt[1] = fx->lpfilt[1];
        if (fxmask & AUDIOFXMASK_HPFILT_L) e->fx.hpfilt[0] = fx->hpfilt[0];
        if (fxmask & AUDIOFXMASK_HPFILT_R) e->fx.hpfilt[1] = fx->hpfilt[1];
    }
    ret:;
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
    if (audiostate.valid) stop3DAudioEmitter_internal(ei, &audiostate.emitters3d.data[ei]);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void stop2DAudioEmitter(int ei) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (audiostate.valid) stop2DAudioEmitter_internal(ei, &audiostate.emitters2d.data[ei]);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}

void delete3DAudioEmitter(int ei) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (audiostate.valid) {
        struct audioemitter3d* e = &audiostate.emitters3d.data[ei];
        stop3DAudioEmitter_internal(ei, e);
        e->prio = AUDIOPRIO_INVALID;
    }
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void delete2DAudioEmitter(int ei) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (audiostate.valid) {
        struct audioemitter2d* e = &audiostate.emitters2d.data[ei];
        stop2DAudioEmitter_internal(ei, e);
        e->prio = AUDIOPRIO_INVALID;
    }
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}

static void initSound(struct audiosound* s, struct rc_sound* rc, unsigned fxmask, const struct audiofx* fx) {
    s->loop = 0;
    s->pos = 0;
    s->frac = 0;
    s->iflags = SOUNDIFLAG_NEEDSINIT;
    s->fxi = 0;
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
    s->calcfx[0].posoff = 0;
    s->calcfx[1].posoff = 0;
    s->lplastout[0] = 0;
    s->lplastout[1] = 0;
    s->hplastout[0] = 0;
    s->hplastout[1] = 0;
    s->hplastin[0] = 0;
    s->hplastin[1] = 0;
    lockRc(rc);
    s->rc = rc;
    switch (s->rc->format) {
        /*case RC_SOUND_FRMT_WAV*/ default:
            if (s->rc->is8bit) s->wav.cvtbuf = rcmgr_malloc(audiostate.decbuflen * s->rc->channels * sizeof(*s->wav.cvtbuf));
            break;
        #ifdef PSRC_USESTBVORBIS
        case RC_SOUND_FRMT_VORBIS:
            s->vorbis.state = stb_vorbis_open_memory(s->rc->data, s->rc->size, NULL, NULL);
            s->vorbis.decbuf = rcmgr_malloc(audiostate.decbuflen * sizeof(*s->vorbis.decbuf));
            s->vorbis.decbufhead = 0;
            s->vorbis.decbuflen = 0;
            break;
        #endif
        #ifdef PSRC_USEMINIMP3
        case RC_SOUND_FRMT_MP3:
            s->mp3.state = rcmgr_malloc(sizeof(*s->mp3.state));
            mp3dec_ex_open_buf(s->mp3.state, s->rc->data, s->rc->size, MP3D_SEEK_TO_SAMPLE);
            s->mp3.decbuf = rcmgr_malloc(audiostate.decbuflen * sizeof(*s->mp3.decbuf));
            s->mp3.decbufhead = 0;
            s->mp3.decbuflen = 0;
            break;
        #endif
    }
}
bool play3DSound(int ei, struct rc_sound* rc, int8_t prio, uint8_t flags, unsigned fxmask, const struct audiofx* fx) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) goto retfalse;
    struct audioemitter3d* e = &audiostate.emitters3d.data[ei];
    if (e->cursounds == e->maxsounds) goto retfalse;
    struct audiosound* s;
    {
        size_t i = 0;
        while (1) {
            if (i == audiostate.sounds3d.len) {
                if (audiostate.sounds3d.len == (size_t)audiostate.max3dsounds) goto retfalse;
                VLB_NEXTPTR(audiostate.sounds3d, s, 2, 1, goto retfalse;);
                break;
            }
            struct audiosound* tmps = &audiostate.sounds3d.data[i];
            if (tmps->prio == AUDIOPRIO_INVALID) {
                s = tmps;
                break;
            }
            ++i;
        }
        VLB_ADD(audiostate.sounds3dorder, i, 2, 1, goto retfalse;);
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
    if (!audiostate.valid) goto retfalse;
    struct audioemitter2d* e = &audiostate.emitters2d.data[ei];
    if (e->cursounds == e->maxsounds) goto retfalse;
    struct audiosound* s;
    {
        size_t i = 0;
        while (1) {
            if (i == audiostate.sounds2d.len) {
                if (audiostate.sounds2d.len == (size_t)audiostate.max2dsounds) goto retfalse;
                VLB_NEXTPTR(audiostate.sounds2d, s, 2, 1, goto retfalse;);
                break;
            }
            struct audiosound* tmps = &audiostate.sounds2d.data[i];
            if (tmps->prio == AUDIOPRIO_INVALID) {
                s = tmps;
                break;
            }
            ++i;
        }
        VLB_ADD(audiostate.sounds2dorder, i, 2, 1, goto retfalse;);
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

void setAudioEnv(unsigned mask, struct audioenv* env, unsigned imm) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) goto ret;
    audiostate.env.envch |= mask;
    audiostate.env.envchimm |= imm;
    if (mask & AUDIOENVMASK_LPFILT) audiostate.env.lpfilt.amount = env->lpfilt;
    if (mask & AUDIOENVMASK_HPFILT) audiostate.env.hpfilt.amount = env->hpfilt;
    if (mask & AUDIOENVMASK_REVERB_DELAY) audiostate.env.reverb.delay = env->reverb.delay;
    if (mask & AUDIOENVMASK_REVERB_MIX) audiostate.env.reverb.mix = env->reverb.mix;
    if (mask & AUDIOENVMASK_REVERB_FEEDBACK) audiostate.env.reverb.feedback = env->reverb.feedback;
    if (mask & AUDIOENVMASK_REVERB_MERGE) audiostate.env.reverb.merge = env->reverb.merge;
    if (mask & AUDIOENVMASK_REVERB_LPFILT) audiostate.env.reverb.lpfilt = env->reverb.lpfilt;
    if (mask & AUDIOENVMASK_REVERB_HPFILT) audiostate.env.reverb.hpfilt = env->reverb.hpfilt;
    ret:;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
}

static void doLPFilter_internal(int mul, int div, int* lastoutp, unsigned len, int* buf) {
    register int lastout = *lastoutp;
    for (register unsigned i = 0; i < len; ++i) {
        register int s = buf[i];
        buf[i] = lastout = lastout + (s - lastout) * mul / div;
    }
    *lastoutp = lastout;
}
static ALWAYSINLINE void doLPFilter(unsigned mul, unsigned div, int* lastoutp, unsigned len, int* buf) {
    if (mul == div) {
        register int s = buf[len - 1];
        *lastoutp += (s - *lastoutp);
    } else {
        doLPFilter_internal(mul, div, lastoutp, len, buf);
    }
}
static void doHPFilter(int mul, int div, int* lastinp, int* lastoutp, unsigned len, int* buf) {
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
static void doLPFilter_interp_internal(int mul1, int mul2, int div, int* lastoutp, int len, int* buf) {
    register int lastout = *lastoutp;
    for (register int i = 0, ii = len; i < len; ++i, --ii) {
        register int s = buf[i];
        volatile int mul = ((mul1 * ii + mul2 * i) / len);
        buf[i] = lastout = lastout + (s - lastout) * mul / div;
    }
    *lastoutp = lastout;
}
static ALWAYSINLINE void doLPFilter_interp(unsigned mul1, unsigned mul2, unsigned div, int* lastoutp, unsigned len, int* buf) {
    if (mul1 == div && mul2 == div) {
        register int s = buf[len - 1];
        *lastoutp += (s - *lastoutp);
    } else {
        doLPFilter_interp_internal(mul1, mul2, div, lastoutp, len, buf);
    }
}
static void doHPFilter_interp(int mul1, int mul2, int div, int* lastinp, int* lastoutp, int len, int* buf) {
    register int lastin = *lastinp;
    register int lastout = *lastoutp;
    for (register int i = 0, ii = len; i < len; ++i, --ii) {
        register int s = buf[i];
        buf[i] = lastout = (lastout + s - lastin) * ((mul1 * ii + mul2 * i) / len) / div;
        lastin = s;
    }
    *lastinp = lastin;
    *lastoutp = lastout;
}
static unsigned adjfilters = 44100;
#define ADJLPFILTMUL(a, m, f) do {\
    if ((f) != audiostate.freq && m < audiostate.freq) {\
        unsigned m2 = (unsigned)roundf((a) * (a) * (f));\
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
    if ((f) != audiostate.freq && m > 0) {\
        unsigned m2 = (unsigned)roundf((a) * (a) * (f));\
        if (m2 > 0) {\
            m += m2;\
            m /= 2;\
            if (m > audiostate.freq) m = audiostate.freq;\
        }\
    }\
} while (0)

static void doReverb(struct audioreverbstate* r, unsigned len, int* bufl, int* bufr) {
    register unsigned head = r->head;
    register unsigned tail = r->tail;
    int mix = r->mix[r->parami];
    int feedback = r->feedback[r->parami];
    int merge = r->merge[r->parami];
    int imerge = 256 - merge;
    int lpmul = r->lpfilt[r->parami];
    int hpmul = r->hpfilt[r->parami];
    int filtdiv = r->filtdiv;
    int lplastoutl = r->lplastout[0];
    int lplastoutr = r->lplastout[1];
    int hplastoutl = r->hplastout[0];
    int hplastoutr = r->hplastout[1];
    int hplastinl = r->hplastin[0];
    int hplastinr = r->hplastin[1];
    for (register unsigned i = 0; i < len; ++i) {
        int in[2] = {bufl[i], bufr[i]};
        int out[2] = {r->buf[0][tail], r->buf[1][tail]};
        int tmp = (out[0] + out[1]) / 2;
        out[0] = (out[0] * imerge + tmp * merge) / 256;
        out[1] = (out[1] * imerge + tmp * merge) / 256;
        hplastoutl = (hplastoutl + out[0] - hplastinl) * hpmul / filtdiv;
        hplastoutr = (hplastoutr + out[1] - hplastinr) * hpmul / filtdiv;
        hplastinl = out[0];
        hplastinr = out[1];
        out[0] = lplastoutl = lplastoutl + (hplastoutl - lplastoutl) * lpmul / filtdiv;
        out[1] = lplastoutr = lplastoutr + (hplastoutr - lplastoutr) * lpmul / filtdiv;
        bufl[i] += out[0] * mix / 256;
        bufr[i] += out[1] * mix / 256;
        in[0] += out[0] * feedback / 256;
        in[1] += out[1] * feedback / 256;
        if (in[0] < -32768) in[0] = -32768;
        else if (in[0] > 32767) in[0] = 32767;
        if (in[1] < -32768) in[1] = -32768;
        else if (in[1] > 32767) in[1] = 32767;
        r->buf[0][head] = in[0];
        r->buf[1][head] = in[1];
        head = (head + 1) % r->size;
        tail = (tail + 1) % r->size;
    }
    r->head = head;
    r->tail = tail;
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
    r->lplastout[0] = lplastoutl;
    r->lplastout[1] = lplastoutr;
    r->hplastout[0] = hplastoutl;
    r->hplastout[1] = hplastoutr;
    r->hplastin[0] = hplastinl;
    r->hplastin[1] = hplastinr;
}
static void doReverb_interp(struct audioreverbstate* r, unsigned len, int* bufl, int* bufr) {
    register unsigned head = r->head;
    register unsigned tail = r->tail;
    uint_fast8_t curparami = r->parami;
    uint_fast8_t newparami = (curparami + 1) % 2;
    int curmix = r->mix[curparami];
    int newmix = r->mix[newparami];
    int curfeedback = r->feedback[curparami];
    int newfeedback = r->feedback[newparami];
    int curmerge = r->merge[curparami];
    int newmerge = r->merge[newparami];
    int curlpmul = r->lpfilt[curparami];
    int newlpmul = r->lpfilt[newparami];
    int curhpmul = r->hpfilt[curparami];
    int newhpmul = r->hpfilt[newparami];
    int filtdiv = r->filtdiv;
    int lplastoutl = r->lplastout[0];
    int lplastoutr = r->lplastout[1];
    int hplastoutl = r->hplastout[0];
    int hplastoutr = r->hplastout[1];
    int hplastinl = r->hplastin[0];
    int hplastinr = r->hplastin[1];
    for (register unsigned i = 0, ii = len; i < len; ++i, --ii) {
        int mix = (curmix * ii + newmix * i) / len;
        int feedback = (curfeedback * ii + newfeedback * i) / len;
        int merge = (curmerge * ii + newmerge * i) / len;
        int imerge = 256 - merge;
        int lpmul = (curlpmul * ii + newlpmul * i) / len;
        int hpmul = (curhpmul * ii + newhpmul * i) / len;
        int in[2] = {bufl[i], bufr[i]};
        int out[2] = {r->buf[0][tail], r->buf[1][tail]};
        int tmp = (out[0] + out[1]) / 2;
        out[0] = (out[0] * imerge + tmp * merge) / 256;
        out[1] = (out[1] * imerge + tmp * merge) / 256;
        hplastoutl = (hplastoutl + out[0] - hplastinl) * hpmul / filtdiv;
        hplastoutr = (hplastoutr + out[1] - hplastinr) * hpmul / filtdiv;
        hplastinl = out[0];
        hplastinr = out[1];
        out[0] = lplastoutl = lplastoutl + (hplastoutl - lplastoutl) * lpmul / filtdiv;
        out[1] = lplastoutr = lplastoutr + (hplastoutr - lplastoutr) * lpmul / filtdiv;
        bufl[i] += out[0] * mix / 256;
        bufr[i] += out[1] * mix / 256;
        in[0] += out[0] * feedback / 256;
        in[1] += out[1] * feedback / 256;
        if (in[0] < -32768) in[0] = -32768;
        else if (in[0] > 32767) in[0] = 32767;
        if (in[1] < -32768) in[1] = -32768;
        else if (in[1] > 32767) in[1] = 32767;
        r->buf[0][head] = in[0];
        r->buf[1][head] = in[1];
        head = (head + 1) % r->size;
        tail = (tail + 1) % r->size;
    }
    r->head = head;
    r->tail = tail;
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
    r->lplastout[0] = lplastoutl;
    r->lplastout[1] = lplastoutr;
    r->hplastout[0] = hplastoutl;
    r->hplastout[1] = hplastoutr;
    r->hplastin[0] = hplastinl;
    r->hplastin[1] = hplastinr;
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
    if (!e->fx3d.nodoppler) {
        long posoff = roundf(dist * -audiostate.soundspeedmul);
        if (posoff != e->fx3dout.posoff) {
            e->fx3dout.posoff = posoff;
            e->fxch |= AUDIOFXMASK_TOFF;
        }
    } else if (e->fx3dout.posoff) {
        e->fx3dout.posoff = 0;
        e->fxch |= AUDIOFXMASK_TOFF;
    }
    float vol[2];
    float lpfilt[2];
    float hpfilt[2];
    {
        float range = e->fx3d.range * (1.0f - e->fx3d.voldamp);
        if (dist < range) {
            float tmp = 1.0f - dist / range;
            vol[1] = vol[0] = tmp * tmp * tmp;
        } else {
            vol[1] = vol[0] = 0.0f;
            lpfilt[0] = 1.0f;
            lpfilt[1] = 1.0f;
            hpfilt[0] = 1.0f;
            hpfilt[1] = 1.0f;
            goto skipcalc;
        }
    }
    if (!e->fx3d.relrot) vec3_trigrotate(pos, audiostate.cam.sin, audiostate.cam.cos, pos);
    if (pos[2] >= 0.0f) {
        pos[0] *= 1.0f + 0.4f * pos[2];
        if (pos[0] < -1.0f) pos[0] = -1.0f;
        else if (pos[0] > 1.0f) pos[0] = 1.0f;
        lpfilt[0] = 0.0f;
        lpfilt[1] = 0.0f;
        hpfilt[0] = 0.0f;
        hpfilt[1] = 0.0f;
    } else {
        pos[0] *= 1.0f - 1.25f * pos[2];
        if (pos[0] < -1.1f) pos[0] = -1.1f;
        else if (pos[0] > 1.1f) pos[0] = 1.1f;
        float tmp = pos[2] * pos[2];
        hpfilt[0] = tmp * 0.15f;
        hpfilt[1] = tmp * 0.15f;
        tmp = 1.0f + pos[2];
        tmp = 1.0f - tmp * tmp;
        lpfilt[0] = pos[2] * -0.2f;
        lpfilt[1] = pos[2] * -0.2f;
        vol[0] *= 1.0f - 0.1f * tmp;
        vol[1] *= 1.0f - 0.1f * tmp;
    }
    if (pos[1] >= 0.0f) {
        pos[0] *= 1.0f - 0.2f * pos[1];
        vol[0] *= 1.0f - 0.1f * pos[1];
        vol[1] *= 1.0f - 0.1f * pos[1];
        float tmp = pos[1] * pos[1] * pos[1] * pos[1];
        hpfilt[0] += tmp * 0.25f;
        hpfilt[1] += tmp * 0.25f;
    } else {
        pos[0] *= 1.0f - 0.65f * pos[1];
        lpfilt[0] -= pos[1] * 0.15f;
        lpfilt[1] -= pos[1] * 0.15f;
    }
    if (pos[0] > 0.0f) {
        //vol[1] *= 1.0f + pos[0] * 0.1f;
        vol[0] *= 1.0f - pos[0] * 0.5f;
        hpfilt[0] += pos[0] * 0.2f;
    } else if (pos[0] < 0.0f) {
        //vol[0] *= 1.0f - pos[0] * 0.1f;
        vol[1] *= 1.0f + pos[0] * 0.5f;
        hpfilt[1] -= pos[0] * 0.2f;
    }
    {
        float tmp = 0.5f - pos[2] * 0.4f;
        hpfilt[0] *= tmp;
        hpfilt[1] *= tmp;
    }
    skipcalc:;
    if (vol[0] != e->fx3dout.volmul[0] || vol[1] != e->fx3dout.volmul[1]) {
        e->fx3dout.volmul[0] = vol[0];
        e->fx3dout.volmul[1] = vol[1];
        e->fxch |= AUDIOFXMASK_VOL;
    }
    lpfilt[0] = 1.0f - (1.0f - lpfilt[0]) * (1.0f - e->fx3d.freqdamp);
    if (lpfilt[0] != e->fx3dout.lpfiltmul[0] || lpfilt[1] != e->fx3dout.lpfiltmul[1]) {
        e->fx3dout.lpfiltmul[0] = lpfilt[0];
        e->fx3dout.lpfiltmul[1] = lpfilt[1];
        e->fxch |= AUDIOFXMASK_LPFILT;
    }
    if (hpfilt[0] != e->fx3dout.hpfiltmul[0] || hpfilt[1] != e->fx3dout.hpfiltmul[1]) {
        e->fx3dout.hpfiltmul[0] = hpfilt[0];
        e->fx3dout.hpfiltmul[1] = hpfilt[1];
        e->fxch |= AUDIOFXMASK_HPFILT;
    }
}
static inline void calc3DSoundFx(struct audiosound* s, struct audioemitter3d* e) {
    uint8_t fxch, imm;
    if (s->iflags & SOUNDIFLAG_NEEDSINIT) {
        fxch = (uint8_t)AUDIOFXMASK_ALL;
        imm = (uint8_t)AUDIOFXMASK_ALL;
        s->iflags &= ~SOUNDIFLAG_NEEDSINIT;
    } else {
        fxch = e->fxch;
        if (!fxch) return;
        imm = e->fxchimm;
    }
    uint8_t curfxi = s->fxi;
    uint8_t newfxi = (curfxi + 1) % 2;
    if (fxch & AUDIOFXMASK_TOFF) {
        int64_t toff = (e->fx.toff + s->fx.toff) * (int64_t)audiostate.freq / 1000000 + e->fx3dout.posoff;
        if (imm & AUDIOFXMASK_TOFF) {
            long oldposoff = s->calcfx[curfxi].posoff;
            s->calcfx[curfxi].posoff = s->calcfx[newfxi].posoff = toff;
            toff -= oldposoff;
            toff *= ((!(s->iflags & SOUNDIFLAG_USESCB)) ? s->rc->freq : s->cb.freq);
            long loop = s->loop;
            long pos = s->pos;
            long frac = s->frac;
            pos += toff / (long)audiostate.freq;
            frac += (toff % (long)audiostate.freq) * 256;
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
            s->fxch |= AUDIOFXMASK_TOFF;
        }
    } else {
        s->calcfx[newfxi].posoff = s->calcfx[curfxi].posoff;
    }
    if (fxch & AUDIOFXMASK_SPEED) {
        s->calcfx[newfxi].speedmul = roundf(s->fx.speed * e->fx.speed * 256.0f);
        if (imm & AUDIOFXMASK_SPEED) s->calcfx[curfxi].speedmul = s->calcfx[newfxi].speedmul;
        else s->fxch |= AUDIOFXMASK_SPEED;
    } else {
        s->calcfx[newfxi].speedmul = s->calcfx[curfxi].speedmul;
    }
    if (fxch & AUDIOFXMASK_VOL) {
        s->calcfx[newfxi].volmul[0] = roundf(s->fx.vol[0] * e->fx3dout.volmul[0] * e->fx.vol[0] * 32768.0f);
        s->calcfx[newfxi].volmul[1] = roundf(s->fx.vol[1] * e->fx3dout.volmul[1] * e->fx.vol[1] * 32768.0f);
        if (imm & AUDIOFXMASK_VOL) {
            if (imm & AUDIOFXMASK_VOL_L) s->calcfx[curfxi].volmul[0] = s->calcfx[newfxi].volmul[0];
            if (imm & AUDIOFXMASK_VOL_R) s->calcfx[curfxi].volmul[1] = s->calcfx[newfxi].volmul[1];
        } else {
            s->fxch |= AUDIOFXMASK_VOL;
        }
    } else {
        s->calcfx[newfxi].volmul[0] = s->calcfx[curfxi].volmul[0];
        s->calcfx[newfxi].volmul[1] = s->calcfx[curfxi].volmul[1];
    }
    if (fxch & AUDIOFXMASK_LPFILT) {
        float tmpf = (1.0f - s->fx.lpfilt[0]) * (1.0f - e->fx3dout.lpfiltmul[0]) * (1.0f - e->fx.lpfilt[0]);
        unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters) ADJLPFILTMUL(tmpf, tmpu, adjfilters);
        if (tmpu > audiostate.freq) tmpu = audiostate.freq;
        s->calcfx[newfxi].lpfiltmul[0] = tmpu;
        tmpf = (1.0f - s->fx.lpfilt[1]) * (1.0f - e->fx3dout.lpfiltmul[1]) * (1.0f - e->fx.lpfilt[1]);
        tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters) ADJLPFILTMUL(tmpf, tmpu, adjfilters);
        if (tmpu > audiostate.freq) tmpu = audiostate.freq;
        s->calcfx[newfxi].lpfiltmul[1] = tmpu;
        if (imm & AUDIOFXMASK_LPFILT) {
            s->calcfx[curfxi].lpfiltmul[0] = s->calcfx[newfxi].lpfiltmul[0];
            s->calcfx[curfxi].lpfiltmul[1] = s->calcfx[newfxi].lpfiltmul[1];
        } else {
            s->fxch |= AUDIOFXMASK_LPFILT;
        }
    } else {
        s->calcfx[newfxi].lpfiltmul[0] = s->calcfx[curfxi].lpfiltmul[0];
        s->calcfx[newfxi].lpfiltmul[1] = s->calcfx[curfxi].lpfiltmul[1];
    }
    if (fxch & AUDIOFXMASK_HPFILT) {
        float tmpf = 1.0f - (1.0f - s->fx.hpfilt[0]) * (1.0f - e->fx3dout.hpfiltmul[0]) * (1.0f - e->fx.hpfilt[0]);
        unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters) ADJHPFILTMUL(tmpf, tmpu, adjfilters);
        if (tmpu >= audiostate.freq) tmpu = 0;
        else tmpu = audiostate.freq - tmpu;
        s->calcfx[newfxi].hpfiltmul[0] = tmpu;
        tmpf = 1.0f - (1.0f - s->fx.hpfilt[1]) * (1.0f - e->fx3dout.hpfiltmul[1]) * (1.0f - e->fx.hpfilt[1]);
        tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters) ADJHPFILTMUL(tmpf, tmpu, adjfilters);
        if (tmpu >= audiostate.freq) tmpu = 0;
        else tmpu = audiostate.freq - tmpu;
        s->calcfx[newfxi].hpfiltmul[1] = tmpu;
        if (imm & AUDIOFXMASK_HPFILT) {
            s->calcfx[curfxi].hpfiltmul[0] = s->calcfx[newfxi].hpfiltmul[0];
            s->calcfx[curfxi].hpfiltmul[1] = s->calcfx[newfxi].hpfiltmul[1];
        } else {
            s->fxch |= AUDIOFXMASK_HPFILT;
        }
    } else {
        s->calcfx[newfxi].hpfiltmul[0] = s->calcfx[curfxi].hpfiltmul[0];
        s->calcfx[newfxi].hpfiltmul[1] = s->calcfx[curfxi].hpfiltmul[1];
    }
}

static inline void calc2DSoundFx(struct audiosound* s, struct audioemitter2d* e) {
    uint8_t fxch, imm;
    if (s->iflags & SOUNDIFLAG_NEEDSINIT) {
        fxch = (uint8_t)AUDIOFXMASK_ALL;
        imm = (uint8_t)AUDIOFXMASK_ALL;
        s->iflags &= ~SOUNDIFLAG_NEEDSINIT;
    } else {
        fxch = e->fxch;
        if (!fxch) return;
        imm = e->fxchimm;
    }
    uint8_t curfxi = s->fxi;
    uint8_t newfxi = (curfxi + 1) % 2;
    if (fxch & AUDIOFXMASK_TOFF) {
        int64_t toff = (e->fx.toff + s->fx.toff) * (int64_t)audiostate.freq / 1000000;
        if (imm & AUDIOFXMASK_TOFF) {
            long oldposoff = s->calcfx[curfxi].posoff;
            s->calcfx[curfxi].posoff = s->calcfx[newfxi].posoff = toff;
            toff -= oldposoff;
            toff *= ((!(s->iflags & SOUNDIFLAG_USESCB)) ? s->rc->freq : s->cb.freq);
            long loop = s->loop;
            long pos = s->pos;
            long frac = s->frac;
            pos += toff / (long)audiostate.freq;
            frac += (toff % (long)audiostate.freq) * 256;
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
            s->fxch |= AUDIOFXMASK_TOFF;
        }
    } else {
        s->calcfx[newfxi].posoff = s->calcfx[curfxi].posoff;
    }
    if (fxch & AUDIOFXMASK_SPEED) {
        s->calcfx[newfxi].speedmul = roundf(s->fx.speed * e->fx.speed * 256.0f);
        if (imm & AUDIOFXMASK_SPEED) s->calcfx[curfxi].speedmul = s->calcfx[newfxi].speedmul;
        else s->fxch |= AUDIOFXMASK_SPEED;
    } else {
        s->calcfx[newfxi].speedmul = s->calcfx[curfxi].speedmul;
    }
    if (fxch & AUDIOFXMASK_VOL) {
        s->calcfx[newfxi].volmul[0] = roundf(s->fx.vol[0] * e->fx.vol[0] * 32768.0f);
        s->calcfx[newfxi].volmul[1] = roundf(s->fx.vol[1] * e->fx.vol[1] * 32768.0f);
        if (imm & AUDIOFXMASK_VOL) {
            if (imm & AUDIOFXMASK_VOL_L) s->calcfx[curfxi].volmul[0] = s->calcfx[newfxi].volmul[0];
            if (imm & AUDIOFXMASK_VOL_R) s->calcfx[curfxi].volmul[1] = s->calcfx[newfxi].volmul[1];
        } else {
            s->fxch |= AUDIOFXMASK_VOL;
        }
    } else {
        s->calcfx[newfxi].volmul[0] = s->calcfx[curfxi].volmul[0];
        s->calcfx[newfxi].volmul[1] = s->calcfx[curfxi].volmul[1];
    }
    if (fxch & AUDIOFXMASK_LPFILT) {
        float tmpf = (1.0f - s->fx.lpfilt[0]) * (1.0f - e->fx.lpfilt[0]);
        unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters) ADJLPFILTMUL(tmpf, tmpu, adjfilters);
        if (tmpu > audiostate.freq) tmpu = audiostate.freq;
        s->calcfx[newfxi].lpfiltmul[0] = tmpu;
        tmpf = (1.0f - s->fx.lpfilt[1]) * (1.0f - e->fx.lpfilt[1]);
        tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters) ADJLPFILTMUL(tmpf, tmpu, adjfilters);
        if (tmpu > audiostate.freq) tmpu = audiostate.freq;
        s->calcfx[newfxi].lpfiltmul[1] = tmpu;
        if (imm & AUDIOFXMASK_LPFILT) {
            s->calcfx[curfxi].lpfiltmul[0] = s->calcfx[newfxi].lpfiltmul[0];
            s->calcfx[curfxi].lpfiltmul[1] = s->calcfx[newfxi].lpfiltmul[1];
        } else {
            s->fxch |= AUDIOFXMASK_LPFILT;
        }
    } else {
        s->calcfx[newfxi].lpfiltmul[0] = s->calcfx[curfxi].lpfiltmul[0];
        s->calcfx[newfxi].lpfiltmul[1] = s->calcfx[curfxi].lpfiltmul[1];
    }
    if (fxch & AUDIOFXMASK_HPFILT) {
        float tmpf = 1.0f - (1.0f - s->fx.hpfilt[0]) * (1.0f - e->fx.hpfilt[0]);
        unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters) ADJHPFILTMUL(tmpf, tmpu, adjfilters);
        if (tmpu >= audiostate.freq) tmpu = 0;
        else tmpu = audiostate.freq - tmpu;
        s->calcfx[newfxi].hpfiltmul[0] = tmpu;
        tmpf = 1.0f - (1.0f - s->fx.hpfilt[1]) * (1.0f - e->fx.hpfilt[1]);
        tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters) ADJHPFILTMUL(tmpf, tmpu, adjfilters);
        if (tmpu >= audiostate.freq) tmpu = 0;
        else tmpu = audiostate.freq - tmpu;
        s->calcfx[newfxi].hpfiltmul[1] = tmpu;
        if (imm & AUDIOFXMASK_HPFILT) {
            s->calcfx[curfxi].hpfiltmul[0] = s->calcfx[newfxi].hpfiltmul[0];
            s->calcfx[curfxi].hpfiltmul[1] = s->calcfx[newfxi].hpfiltmul[1];
        } else {
            s->fxch |= AUDIOFXMASK_HPFILT;
        }
    } else {
        s->calcfx[newfxi].hpfiltmul[0] = s->calcfx[curfxi].hpfiltmul[0];
        s->calcfx[newfxi].hpfiltmul[1] = s->calcfx[curfxi].hpfiltmul[1];
    }
}

static inline void applyAudioEnv(void) {
    if (audiostate.env.envch & AUDIOENVMASK_REVERB_DELAY) {
        unsigned oldlen = audiostate.env.reverb.state.len;
        unsigned newlen;
        {
            float tmp = audiostate.env.reverb.delay;
            if (tmp < 0.0f) tmp = 0.0f;
            else if (tmp > 5.0f) tmp = 5.0f;
            newlen = roundf(tmp * audiostate.freq);
        }
        audiostate.env.reverb.state.len = newlen;
        if (!newlen) {
            audiostate.env.reverb.state.size = 0;
            free(audiostate.env.reverb.state.buf[0]);
            free(audiostate.env.reverb.state.buf[1]);
        } else if (newlen != oldlen) {
            register unsigned newsize = newlen - 1;
            newsize |= newsize >> 1;
            newsize |= newsize >> 2;
            newsize |= newsize >> 4;
            newsize |= newsize >> 8;
            newsize |= newsize >> 16;
            ++newsize;
            if (oldlen) {
                int16_t* newbuf[2];
                newbuf[0] = rcmgr_malloc(newsize * sizeof(**newbuf));
                newbuf[1] = rcmgr_malloc(newsize * sizeof(**newbuf));
                unsigned oldsize = audiostate.env.reverb.state.size;
                unsigned minlen = (newlen <= oldlen) ? newlen : oldlen;
                unsigned pos = (audiostate.env.reverb.state.head + (oldsize - minlen)) % oldsize;
                for (unsigned i = 0; i < minlen; ++i) {
                    newbuf[0][i] = audiostate.env.reverb.state.buf[0][pos];
                    newbuf[1][i] = audiostate.env.reverb.state.buf[1][pos];
                    pos = (pos + 1) % oldsize;
                }
                for (unsigned i = minlen; i < newsize; ++i) {
                    newbuf[0][i] = 0;
                    newbuf[1][i] = 0;
                }
                audiostate.env.reverb.state.buf[0] = newbuf[0];
                audiostate.env.reverb.state.buf[1] = newbuf[1];
                audiostate.env.reverb.state.size = newsize;
                audiostate.env.reverb.state.head = 0;
                audiostate.env.reverb.state.tail = newsize - newlen;
            } else {
                audiostate.env.reverb.state.buf[0] = rcmgr_calloc(newsize, sizeof(**audiostate.env.reverb.state.buf));
                audiostate.env.reverb.state.buf[1] = rcmgr_calloc(newsize, sizeof(**audiostate.env.reverb.state.buf));
                audiostate.env.reverb.state.size = newsize;
                audiostate.env.reverb.state.head = 0;
                audiostate.env.reverb.state.tail = newsize - newlen;
            }
        }
    }
    if (audiostate.env.envch & (AUDIOENVMASK_REVERB & ~AUDIOENVMASK_REVERB_DELAY)) {
        uint_fast8_t curparami = audiostate.env.reverb.state.parami;
        uint_fast8_t newparami = (curparami + 1) % 2;
        if (audiostate.env.envch & AUDIOENVMASK_REVERB_MIX) {
            audiostate.env.reverb.state.mix[newparami] = roundf(audiostate.env.reverb.mix * 256.0f);
            if (audiostate.env.envchimm & AUDIOENVMASK_REVERB_MIX) {
                audiostate.env.reverb.state.mix[curparami] = audiostate.env.reverb.state.mix[newparami];
            }
        } else {
            audiostate.env.reverb.state.mix[newparami] = audiostate.env.reverb.state.mix[curparami];
        }
        if (audiostate.env.envch & AUDIOENVMASK_REVERB_FEEDBACK) {
            audiostate.env.reverb.state.feedback[newparami] = roundf(audiostate.env.reverb.feedback * 256.0f);
            if (audiostate.env.envchimm & AUDIOENVMASK_REVERB_FEEDBACK) {
                audiostate.env.reverb.state.feedback[curparami] = audiostate.env.reverb.state.feedback[newparami];
            }
        } else {
            audiostate.env.reverb.state.feedback[newparami] = audiostate.env.reverb.state.feedback[curparami];
        }
        if (audiostate.env.envch & AUDIOENVMASK_REVERB_MERGE) {
            audiostate.env.reverb.state.merge[newparami] = roundf(audiostate.env.reverb.merge * 256.0f);
            if (audiostate.env.envchimm & AUDIOENVMASK_REVERB_MERGE) {
                audiostate.env.reverb.state.merge[curparami] = audiostate.env.reverb.state.merge[newparami];
            }
        } else {
            audiostate.env.reverb.state.merge[newparami] = audiostate.env.reverb.state.merge[curparami];
        }
        if (audiostate.env.envch & AUDIOENVMASK_REVERB_LPFILT) {
            float tmpf = 1.0f - audiostate.env.reverb.lpfilt;
            unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
            if (adjfilters) ADJLPFILTMUL(tmpf, tmpu, adjfilters);
            if (tmpu > audiostate.freq) tmpu = audiostate.freq;
            audiostate.env.reverb.state.lpfilt[newparami] = tmpu;
            if (audiostate.env.envch & AUDIOENVMASK_REVERB_LPFILT) {
                audiostate.env.reverb.state.lpfilt[curparami] = tmpu;
            }
        } else {
            audiostate.env.reverb.state.lpfilt[newparami] = audiostate.env.reverb.state.lpfilt[curparami];
        }
        if (audiostate.env.envch & AUDIOENVMASK_REVERB_HPFILT) {
            float tmpf = audiostate.env.reverb.hpfilt;
            unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
            if (adjfilters) ADJHPFILTMUL(tmpf, tmpu, adjfilters);
            if (tmpu >= audiostate.freq) tmpu = 0;
            else tmpu = audiostate.freq - tmpu;
            audiostate.env.reverb.state.hpfilt[newparami] = tmpu;
            if (audiostate.env.envch & AUDIOENVMASK_REVERB_HPFILT) {
                audiostate.env.reverb.state.lpfilt[curparami] = tmpu;
            }
        } else {
            audiostate.env.reverb.state.hpfilt[newparami] = audiostate.env.reverb.state.hpfilt[curparami];
        }
        if (audiostate.env.reverb.state.len) {
            doReverb_interp(&audiostate.env.reverb.state, audiostate.buflen, audiostate.mixbuf[0], audiostate.mixbuf[1]);
        }
        audiostate.env.reverb.state.parami = newparami;
    } else if (audiostate.env.reverb.state.len) {
        doReverb(&audiostate.env.reverb.state, audiostate.buflen, audiostate.mixbuf[0], audiostate.mixbuf[1]);
    }
    if (audiostate.env.envch & AUDIOENVMASK_HPFILT) {
        uint_fast8_t curmuli = audiostate.env.hpfilt.muli;
        uint_fast8_t newmuli = (curmuli + 1) % 2;
        float tmpf = audiostate.env.hpfilt.amount;
        unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters) ADJHPFILTMUL(tmpf, tmpu, adjfilters);
        if (tmpu >= audiostate.freq) tmpu = 0;
        else tmpu = audiostate.freq - tmpu;
        audiostate.env.hpfilt.mul[newmuli] = tmpu;
        if (audiostate.env.envchimm & AUDIOENVMASK_HPFILT) {
            audiostate.env.hpfilt.mul[curmuli] = tmpu;
            doHPFilter(
                tmpu, audiostate.freq,
                &audiostate.env.hpfilt.lastin[0], &audiostate.env.hpfilt.lastout[0], audiostate.buflen, audiostate.mixbuf[0]
            );
            doHPFilter(
                tmpu, audiostate.freq,
                &audiostate.env.hpfilt.lastin[1], &audiostate.env.hpfilt.lastout[1], audiostate.buflen, audiostate.mixbuf[1]
            );
        } else {
            doHPFilter_interp(
                audiostate.env.hpfilt.mul[curmuli], tmpu, audiostate.freq,
                &audiostate.env.hpfilt.lastin[0], &audiostate.env.hpfilt.lastout[0], audiostate.buflen, audiostate.mixbuf[0]
            );
            doHPFilter_interp(
                audiostate.env.hpfilt.mul[curmuli], tmpu, audiostate.freq,
                &audiostate.env.hpfilt.lastin[1], &audiostate.env.hpfilt.lastout[1], audiostate.buflen, audiostate.mixbuf[1]
            );
        }
    } else {
        doHPFilter(
            audiostate.env.hpfilt.mul[audiostate.env.hpfilt.muli], audiostate.freq,
            &audiostate.env.hpfilt.lastin[0], &audiostate.env.hpfilt.lastout[0], audiostate.buflen, audiostate.mixbuf[0]
        );
        doHPFilter(
            audiostate.env.hpfilt.mul[audiostate.env.hpfilt.muli], audiostate.freq,
            &audiostate.env.hpfilt.lastin[1], &audiostate.env.hpfilt.lastout[1], audiostate.buflen, audiostate.mixbuf[1]
        );
    }
    if (audiostate.env.envch & AUDIOENVMASK_LPFILT) {
        uint_fast8_t curmuli = audiostate.env.lpfilt.muli;
        uint_fast8_t newmuli = (curmuli + 1) % 2;
        float tmpf = 1.0f - audiostate.env.lpfilt.amount;
        unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters) ADJLPFILTMUL(tmpf, tmpu, adjfilters);
        if (tmpu > audiostate.freq) tmpu = audiostate.freq;
        audiostate.env.lpfilt.mul[newmuli] = tmpu;
        if (audiostate.env.envchimm & AUDIOENVMASK_LPFILT) {
            audiostate.env.lpfilt.mul[curmuli] = tmpu;
            doLPFilter(
                tmpu, audiostate.freq,
                &audiostate.env.lpfilt.lastout[0], audiostate.buflen, audiostate.mixbuf[0]
            );
            doLPFilter(
                tmpu, audiostate.freq,
                &audiostate.env.lpfilt.lastout[1], audiostate.buflen, audiostate.mixbuf[1]
            );
        } else {
            doLPFilter_interp(
                audiostate.env.lpfilt.mul[curmuli], tmpu, audiostate.freq,
                &audiostate.env.lpfilt.lastout[0], audiostate.buflen, audiostate.mixbuf[0]
            );
            doLPFilter_interp(
                audiostate.env.lpfilt.mul[curmuli], tmpu, audiostate.freq,
                &audiostate.env.lpfilt.lastout[1], audiostate.buflen, audiostate.mixbuf[1]
            );
        }
    } else {
        doLPFilter(
            audiostate.env.lpfilt.mul[audiostate.env.lpfilt.muli], audiostate.freq,
            &audiostate.env.lpfilt.lastout[0], audiostate.buflen, audiostate.mixbuf[0]
        );
        doLPFilter(
            audiostate.env.lpfilt.mul[audiostate.env.lpfilt.muli], audiostate.freq,
            &audiostate.env.lpfilt.lastout[1], audiostate.buflen, audiostate.mixbuf[1]
        );
    }
    audiostate.env.envch = 0;
    audiostate.env.envchimm = 0;
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
static bool mixsound(struct audiosound* s, bool fake, bool mixmono) {
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
        if (!(s->fxch & AUDIOFXMASK_VOL)) {
            vol[0][0] = s->calcfx[curfxi].volmul[0];
            vol[0][1] = s->calcfx[curfxi].volmul[1];
            if (!vol[0][0] && !vol[0][1]) fake = true;
            //printf("VOL %p: [%d, %d]\n", (void*)s, vol[0][0], vol[0][1]);
        } else {
            vol[0][0] = s->calcfx[curfxi].volmul[0];
            vol[0][1] = s->calcfx[curfxi].volmul[1];
            vol[1][0] = s->calcfx[newfxi].volmul[0];
            vol[1][1] = s->calcfx[newfxi].volmul[1];
            if (!vol[0][0] && !vol[0][1] && !vol[1][0] && !vol[1][1]) fake = true;
            //printf("VOL %p: [%d, %d], [%d, %d]\n", (void*)s, vol[0][0], vol[0][1], vol[1][0], vol[1][1]);
        }
    }

    int16_t* buf;
    long bufstart = 0, bufend = -1;

    if (!(s->fxch & (AUDIOFXMASK_TOFF | AUDIOFXMASK_SPEED))) {
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
        long posoff[3];
        posoff[0] = 0;
        posoff[1] = (s->fxch & AUDIOFXMASK_TOFF) ? s->calcfx[newfxi].posoff - s->calcfx[curfxi].posoff : 0;
        long speedmul[2];
        speedmul[0] = s->calcfx[curfxi].speedmul;
        speedmul[1] = s->calcfx[newfxi].speedmul;
        register long i = 0, ii = audiostate.buflen;
        if (!fake) {
            bool oob;
            while (1) {
                MIXSOUND_OOBCHECK();
                skipoobchk_2:;
                MIXSOUND_DOMIXING();
                posoff[2] = posoff[1] * i / (long)audiostate.buflen;
                long tmpposoff = posoff[2] - posoff[0];
                posoff[0] = posoff[2];
                tmpposoff *= freq;
                pos += tmpposoff / outfreq;
                tmpposoff %= outfreq;
                frac += ((speedmul[0] * ii + speedmul[1] * i) / (long)audiostate.buflen) * freq + tmpposoff * 256;
                MIXSOUND_DOPOSMATH(++i; if (i == audiostate.buflen) goto brkloop_2; --ii; goto skipoobchk_2;);
                ++i;
                if (i == audiostate.buflen) break;
                --ii;
            }
            brkloop_2:;
        } else {
            while (1) {
                posoff[2] = posoff[1] * i / (long)audiostate.buflen;
                long tmpposoff = posoff[2] - posoff[0];
                posoff[0] = posoff[2];
                tmpposoff *= freq;
                pos += tmpposoff / outfreq;
                tmpposoff %= outfreq;
                frac += ((speedmul[0] * ii + speedmul[1] * i) / (long)audiostate.buflen) * freq + tmpposoff * 256;
                MIXSOUND_DOPOSMATH();
                ++i;
                if (i == audiostate.buflen) break;
                --ii;
            }
        }
        posoff[1] -= posoff[0];
        pos += posoff[1] / outfreq;
        posoff[1] %= outfreq;
        frac += posoff[1] * 256;
        MIXSOUND_DOPOSMATH();
    }
    if (!fake) {
        if (!(s->fxch & AUDIOFXMASK_HPFILT)) {
            unsigned tmp = s->calcfx[curfxi].hpfiltmul[0];
            //if (alwaysfilt || tmp != audiostate.freq)
                doHPFilter(tmp, audiostate.freq, &s->hplastin[0], &s->hplastout[0], audiostate.buflen, audiostate.fxbuf[0]);
            tmp = s->calcfx[curfxi].hpfiltmul[1];
            //if (alwaysfilt || tmp != audiostate.freq)
                doHPFilter(tmp, audiostate.freq, &s->hplastin[1], &s->hplastout[1], audiostate.buflen, audiostate.fxbuf[1]);
        } else {
            unsigned tmp1 = s->calcfx[curfxi].hpfiltmul[0];
            unsigned tmp2 = s->calcfx[newfxi].hpfiltmul[0];
            //if (alwaysfilt || tmp1 != audiostate.freq || tmp2 != audiostate.freq)
                doHPFilter_interp(tmp1, tmp2, audiostate.freq, &s->hplastin[0], &s->hplastout[0], audiostate.buflen, audiostate.fxbuf[0]);
            tmp1 = s->calcfx[curfxi].hpfiltmul[1];
            tmp2 = s->calcfx[newfxi].hpfiltmul[1];
            //if (alwaysfilt || tmp1 != audiostate.freq || tmp2 != audiostate.freq)
                doHPFilter_interp(tmp1, tmp2, audiostate.freq, &s->hplastin[1], &s->hplastout[1], audiostate.buflen, audiostate.fxbuf[1]);
        }
        if (!(s->fxch & AUDIOFXMASK_LPFILT)) {
            unsigned tmp = s->calcfx[curfxi].lpfiltmul[0];
            //if (alwaysfilt || tmp != audiostate.freq)
                doLPFilter(tmp, audiostate.freq, &s->lplastout[0], audiostate.buflen, audiostate.fxbuf[0]);
            tmp = s->calcfx[curfxi].lpfiltmul[1];
            //if (alwaysfilt || tmp != audiostate.freq)
                doLPFilter(tmp, audiostate.freq, &s->lplastout[1], audiostate.buflen, audiostate.fxbuf[1]);
        } else {
            unsigned tmp1 = s->calcfx[curfxi].lpfiltmul[0];
            unsigned tmp2 = s->calcfx[newfxi].lpfiltmul[0];
            //if (alwaysfilt || tmp1 != audiostate.freq || tmp2 != audiostate.freq)
                doLPFilter_interp(tmp1, tmp2, audiostate.freq, &s->lplastout[0], audiostate.buflen, audiostate.fxbuf[0]);
            tmp1 = s->calcfx[curfxi].lpfiltmul[1];
            tmp2 = s->calcfx[newfxi].lpfiltmul[1];
            //if (alwaysfilt || tmp1 != audiostate.freq || tmp2 != audiostate.freq)
                doLPFilter_interp(tmp1, tmp2, audiostate.freq, &s->lplastout[1], audiostate.buflen, audiostate.fxbuf[1]);
        }
        if (!(s->fxch & AUDIOFXMASK_VOL)) {
            for (register unsigned i = 0; i < audiostate.buflen; ++i) {
                audiostate.mixbuf[0][i] += audiostate.fxbuf[0][i] * vol[0][0] / 32768;
                audiostate.mixbuf[1][i] += audiostate.fxbuf[1][i] * vol[0][1] / 32768;
            }
        } else {
            for (register unsigned i = 0, ii = audiostate.buflen; i < audiostate.buflen; ++i, --ii) {
                int vol_l = (vol[0][0] * ii + vol[1][0] * i) / audiostate.buflen;
                int vol_r = (vol[0][1] * ii + vol[1][1] * i) / audiostate.buflen;
                audiostate.mixbuf[0][i] += audiostate.fxbuf[0][i] * vol_l / 32768;
                audiostate.mixbuf[1][i] += audiostate.fxbuf[1][i] * vol_r / 32768;
            }
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
    if (s->fxch) {
        s->fxch = 0;
        s->fxi = newfxi;
    }
    return true;

    retfalse:;
    return false;
}

static int mixsounds_3dsortcb(const void* a, const void* b) {
    struct audiosound* s1 = &audiostate.sounds3d.data[*(size_t*)a];
    struct audiosound* s2 = &audiostate.sounds3d.data[*(size_t*)b];
    struct audioemitter3d* e1 = &audiostate.emitters3d.data[s1->emitter];
    struct audioemitter3d* e2 = &audiostate.emitters3d.data[s2->emitter];
    if ((e1->flags & AUDIOEMITTER3DFLAG_PAUSED) != (e2->flags & AUDIOEMITTER3DFLAG_PAUSED)) {
        return (int)((e1->flags & AUDIOEMITTER3DFLAG_PAUSED) != 0) - (int)((e2->flags & AUDIOEMITTER3DFLAG_PAUSED) != 0);
    }
    if (s1->prio == s2->prio) {
        return (e1->dist != e2->dist) ? ((e1->dist > e2->dist) ? 1 : -1) : 0;
    }
    return (int)s2->prio - (int)s1->prio;
}
static int mixsounds_2dsortcb(const void* a, const void* b) {
    struct audiosound* s1 = &audiostate.sounds2d.data[*(size_t*)a];
    struct audiosound* s2 = &audiostate.sounds2d.data[*(size_t*)b];
    struct audioemitter2d* e1 = &audiostate.emitters2d.data[s1->emitter];
    struct audioemitter2d* e2 = &audiostate.emitters2d.data[s2->emitter];
    if ((e1->flags & AUDIOEMITTER2DFLAG_PAUSED) != (e2->flags & AUDIOEMITTER2DFLAG_PAUSED)) {
        return (int)((e1->flags & AUDIOEMITTER2DFLAG_PAUSED) != 0) - (int)((e2->flags & AUDIOEMITTER2DFLAG_PAUSED) != 0);
    }
    return (int)s2->prio - (int)s1->prio;
}
static void mixsounds(unsigned buf) {
    for (size_t i = 0; i < audiostate.emitters3d.len; ++i) {
        struct audioemitter3d* e = &audiostate.emitters3d.data[i];
        if (!(e->flags & AUDIOEMITTER3DFLAG_PAUSED)) {
            calc3DEmitterFx(e);
        }
    }
    for (size_t i = 0; i < audiostate.sounds3d.len; ++i) {
        struct audiosound* s = &audiostate.sounds3d.data[i];
        struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
        if (!(e->flags & AUDIOEMITTER3DFLAG_PAUSED)) {
            calc3DSoundFx(s, e);
        }
    }

    for (size_t i = 0; i < audiostate.sounds2d.len; ++i) {
        struct audiosound* s = &audiostate.sounds2d.data[i];
        struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
        if (!(e->flags & AUDIOEMITTER2DFLAG_PAUSED)) {
            calc2DSoundFx(s, e);
        }
    }

    for (size_t i = 0; i < audiostate.emitters3d.len; ++i) {
        struct audioemitter3d* e = &audiostate.emitters3d.data[i];
        if (e->fxch) e->fxch = 0;
        if (e->fxchimm) e->fxchimm = 0;
    }
    for (size_t i = 0; i < audiostate.emitters2d.len; ++i) {
        struct audioemitter2d* e = &audiostate.emitters2d.data[i];
        if (e->fxch) e->fxch = 0;
        if (e->fxchimm) e->fxchimm = 0;
    }

    memset(audiostate.mixbuf[0], 0, audiostate.buflen * sizeof(**audiostate.mixbuf));
    memset(audiostate.mixbuf[1], 0, audiostate.buflen * sizeof(**audiostate.mixbuf));

    {
        size_t tmp = audiostate.sounds3dorder.len;
        if (tmp) {
            if (tmp > 1) qsort(audiostate.sounds3dorder.data, tmp, sizeof(*audiostate.sounds3dorder.data), mixsounds_3dsortcb);
            if (tmp <= audiostate.voices3d) {
                for (size_t i = 0; i < tmp; ++i) {
                    size_t si = audiostate.sounds3dorder.data[i];
                    struct audiosound* s = &audiostate.sounds3d.data[si];
                    struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
                    if (!(e->flags & AUDIOEMITTER3DFLAG_PAUSED) && !(e->flags & AUDIOEMITTER3DFLAG_NOENV)) {
                        if (!mixsound(s, false, true)) delete3DSound(si, s);
                    }
                }
            } else {
                for (size_t i = 0; i < audiostate.voices3d; ++i) {
                    size_t si = audiostate.sounds3dorder.data[i];
                    struct audiosound* s = &audiostate.sounds3d.data[si];
                    struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
                    if (!(e->flags & AUDIOEMITTER3DFLAG_PAUSED) && !(e->flags & AUDIOEMITTER3DFLAG_NOENV)) {
                        if (!mixsound(s, false, true)) delete3DSound(si, s);
                    }
                }
                for (size_t i = audiostate.voices3d; i < tmp; ++i) {
                    size_t si = audiostate.sounds3dorder.data[i];
                    struct audiosound* s = &audiostate.sounds3d.data[si];
                    struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
                    if (!(e->flags & AUDIOEMITTER3DFLAG_PAUSED) && !(e->flags & AUDIOEMITTER3DFLAG_NOENV)) {
                        if (!mixsound(s, true, true)) delete3DSound(si, s);
                    }
                }
            }
        }
        tmp = audiostate.sounds2dorder.len;
        if (tmp) {
            if (tmp > 1) qsort(audiostate.sounds2dorder.data, tmp, sizeof(*audiostate.sounds2dorder.data), mixsounds_2dsortcb);
            if (tmp <= audiostate.voices2d) {
                for (size_t i = 0; i < tmp; ++i) {
                    size_t si = audiostate.sounds2dorder.data[i];
                    struct audiosound* s = &audiostate.sounds2d.data[si];
                    struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
                    if (!(e->flags & AUDIOEMITTER2DFLAG_PAUSED) && (e->flags & AUDIOEMITTER2DFLAG_APPLYENV)) {
                        if (!mixsound(s, false, false)) delete2DSound(si, s);
                    }
                }
            } else {
                for (size_t i = 0; i < audiostate.voices2d; ++i) {
                    size_t si = audiostate.sounds2dorder.data[i];
                    struct audiosound* s = &audiostate.sounds2d.data[si];
                    struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
                    if (!(e->flags & AUDIOEMITTER2DFLAG_PAUSED) && (e->flags & AUDIOEMITTER2DFLAG_APPLYENV)) {
                        if (!mixsound(s, false, false)) delete2DSound(si, s);
                    }
                }
                for (size_t i = audiostate.voices2d; i < tmp; ++i) {
                    size_t si = audiostate.sounds2dorder.data[i];
                    struct audiosound* s = &audiostate.sounds2d.data[si];
                    struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
                    if (!(e->flags & AUDIOEMITTER2DFLAG_PAUSED) && (e->flags & AUDIOEMITTER2DFLAG_APPLYENV)) {
                        if (!mixsound(s, true, false)) delete2DSound(si, s);
                    }
                }
            }
        }
        applyAudioEnv();
        tmp = audiostate.sounds3dorder.len;
        if (tmp) {
            if (tmp > 1) qsort(audiostate.sounds3dorder.data, tmp, sizeof(*audiostate.sounds3dorder.data), mixsounds_3dsortcb);
            if (tmp <= audiostate.voices3d) {
                for (size_t i = 0; i < tmp; ++i) {
                    size_t si = audiostate.sounds3dorder.data[i];
                    struct audiosound* s = &audiostate.sounds3d.data[si];
                    struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
                    if (!(e->flags & AUDIOEMITTER3DFLAG_PAUSED) && (e->flags & AUDIOEMITTER3DFLAG_NOENV)) {
                        if (!mixsound(s, false, true)) delete3DSound(si, s);
                    }
                }
            } else {
                for (size_t i = 0; i < audiostate.voices3d; ++i) {
                    size_t si = audiostate.sounds3dorder.data[i];
                    struct audiosound* s = &audiostate.sounds3d.data[si];
                    struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
                    if (!(e->flags & AUDIOEMITTER3DFLAG_PAUSED) && (e->flags & AUDIOEMITTER3DFLAG_NOENV)) {
                        if (!mixsound(s, false, true)) delete3DSound(si, s);
                    }
                }
                for (size_t i = audiostate.voices3d; i < tmp; ++i) {
                    size_t si = audiostate.sounds3dorder.data[i];
                    struct audiosound* s = &audiostate.sounds3d.data[si];
                    struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
                    if (!(e->flags & AUDIOEMITTER3DFLAG_PAUSED) && (e->flags & AUDIOEMITTER3DFLAG_NOENV)) {
                        if (!mixsound(s, true, true)) delete3DSound(si, s);
                    }
                }
            }
        }
        tmp = audiostate.sounds2dorder.len;
        if (tmp) {
            if (tmp > 1) qsort(audiostate.sounds2dorder.data, tmp, sizeof(*audiostate.sounds2dorder.data), mixsounds_2dsortcb);
            if (tmp <= audiostate.voices2d) {
                for (size_t i = 0; i < tmp; ++i) {
                    size_t si = audiostate.sounds2dorder.data[i];
                    struct audiosound* s = &audiostate.sounds2d.data[si];
                    struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
                    if (!(e->flags & AUDIOEMITTER2DFLAG_PAUSED) && !(e->flags & AUDIOEMITTER2DFLAG_APPLYENV)) {
                        if (!mixsound(s, false, false)) delete2DSound(si, s);
                    }
                }
            } else {
                for (size_t i = 0; i < audiostate.voices2d; ++i) {
                    size_t si = audiostate.sounds2dorder.data[i];
                    struct audiosound* s = &audiostate.sounds2d.data[si];
                    struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
                    if (!(e->flags & AUDIOEMITTER2DFLAG_PAUSED) && !(e->flags & AUDIOEMITTER2DFLAG_APPLYENV)) {
                        if (!mixsound(s, false, false)) delete2DSound(si, s);
                    }
                }
                for (size_t i = audiostate.voices2d; i < tmp; ++i) {
                    size_t si = audiostate.sounds2dorder.data[i];
                    struct audiosound* s = &audiostate.sounds2d.data[si];
                    struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
                    if (!(e->flags & AUDIOEMITTER2DFLAG_PAUSED) && !(e->flags & AUDIOEMITTER2DFLAG_APPLYENV)) {
                        if (!mixsound(s, true, false)) delete2DSound(si, s);
                    }
                }
            }
        }
    }

    int16_t* out = audiostate.outbuf[buf];
    if (audiostate.channels < 2) {
        for (register unsigned i = 0; i < audiostate.buflen; ++i) {
            register int sample = (audiostate.mixbuf[0][i] + audiostate.mixbuf[1][i]) / 2;
            if (sample > 32767) sample = 32767;
            else if (sample < -32768) sample = -32768;
            sample *= audiostate.vol;
            sample /= 32768;
            out[i] = sample;
        }
    } else {
        for (register unsigned c = 0; c < 2; ++c) {
            for (register unsigned i = 0; i < audiostate.buflen; ++i) {
                register int sample = audiostate.mixbuf[c][i];
                if (sample > 32767) sample = 32767;
                else if (sample < -32768) sample = -32768;
                sample *= audiostate.vol;
                sample /= 32768;
                out[i * audiostate.channels + c] = sample;
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
    if (!audiostate.valid) goto ret;
    {
        float tmp[3];
        tmp[0] = -audiostate.cam.rot[0];
        tmp[1] = -audiostate.cam.rot[1];
        tmp[2] = -audiostate.cam.rot[2];
        vec3_calctrig(tmp, audiostate.cam.sin, audiostate.cam.cos); // TODO: copy from renderer instead of recalc
    }
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
        unsigned queuesz = SDL_GetQueuedAudioSize(audiostate.output);
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
    ret:;
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
    tmp = cfg_getvar(&config, "Audio", "volume");
    if (tmp) {
        audiostate.vol = roundf(atof(tmp) * 32678.0);
        free(tmp);
    } else {
        audiostate.vol = 32678;
    }
    audiostate.freq = outspec.freq;
    audiostate.channels = outspec.channels;
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
    //audiostate.soundspeedmul = (float)outspec.freq / 343.0f;
    audiostate.soundspeedmul = (float)outspec.freq / 400.0f;
    audiostate.buflen = outspec.samples;
    audiostate.fxbuf[0] = malloc(outspec.samples * sizeof(**audiostate.fxbuf));
    audiostate.fxbuf[1] = malloc(outspec.samples * sizeof(**audiostate.fxbuf));
    audiostate.mixbuf[0] = malloc(outspec.samples * sizeof(**audiostate.mixbuf));
    audiostate.mixbuf[1] = malloc(outspec.samples * sizeof(**audiostate.mixbuf));
    tmp = cfg_getvar(&config, "Audio", "decodebuf");
    if (tmp) {
        audiostate.decbuflen = atoi(tmp);
        free(tmp);
    } else {
        audiostate.decbuflen = 4096;
    }
    audiostate.outsize = outspec.samples * sizeof(**audiostate.outbuf) * outspec.channels;
    tmp = cfg_getvar(&config, "Audio", "outqueue");
    if (tmp) {
        audiostate.outqueue = atoi(tmp);
        free(tmp);
    } else {
        audiostate.outqueue = 2;
    }
    audiostate.outbuf[0] = malloc(audiostate.outsize);
    if (audiostate.usecallback) audiostate.outbuf[1] = malloc(audiostate.outsize);
    audiostate.outbufi = 0;
    audiostate.mixoutbufi = -1;
    tmp = cfg_getvar(&config, "Audio", "max3dsounds");
    if (tmp) {
        audiostate.max3dsounds = strtoul(tmp, NULL, 10);
        free(tmp);
    } else {
        audiostate.max3dsounds = 1024;
    }
    tmp = cfg_getvar(&config, "Audio", "3dvoices");
    if (tmp) {
        audiostate.voices3d = strtoul(tmp, NULL, 10);
        free(tmp);
    } else {
        audiostate.voices3d = 256;
    }
    {
        unsigned tmpu = audiostate.voices3d / 4;
        if (!tmpu) tmpu = 1;
        else if (tmpu > 16) tmpu = 16;
        VLB_INIT(audiostate.emitters3d, tmpu, VLB_OOM_NOP);
        VLB_INIT(audiostate.sounds3d, tmpu, VLB_OOM_NOP);
        tmpu = audiostate.voices3d / 2;
        if (!tmpu) tmpu = 1;
        else if (tmpu > 256) tmpu = 256;
        VLB_INIT(audiostate.sounds3dorder, tmpu, VLB_OOM_NOP);
    }
    tmp = cfg_getvar(&config, "Audio", "max2dsounds");
    if (tmp) {
        audiostate.max2dsounds = strtoul(tmp, NULL, 10);
        free(tmp);
    } else {
        audiostate.max2dsounds = 1024;
    }
    tmp = cfg_getvar(&config, "Audio", "2dvoices");
    if (tmp) {
        audiostate.voices2d = strtoul(tmp, NULL, 10);
        free(tmp);
    } else {
        audiostate.voices2d = 256;
    }
    {
        unsigned tmpu = audiostate.voices2d / 4;
        if (!tmpu) tmpu = 1;
        else if (tmpu > 16) tmpu = 16;
        VLB_INIT(audiostate.emitters2d, tmpu, VLB_OOM_NOP);
        VLB_INIT(audiostate.sounds2d, tmpu, VLB_OOM_NOP);
        tmpu = audiostate.voices2d / 2;
        if (!tmpu) tmpu = 1;
        else if (tmpu > 256) tmpu = 256;
        VLB_INIT(audiostate.sounds2dorder, tmpu, VLB_OOM_NOP);
    }
    audiostate.env.envch = (uint8_t)AUDIOENVMASK_ALL;
    audiostate.env.envchimm = (uint8_t)AUDIOENVMASK_ALL;
    audiostate.env.lpfilt.amount = 0.0f;
    audiostate.env.lpfilt.muli = 0;
    audiostate.env.lpfilt.lastout[0] = 0;
    audiostate.env.lpfilt.lastout[1] = 0;
    audiostate.env.hpfilt.amount = 0.0f;
    audiostate.env.hpfilt.muli = 0;
    audiostate.env.hpfilt.lastin[0] = 0;
    audiostate.env.hpfilt.lastin[1] = 0;
    audiostate.env.hpfilt.lastout[0] = 0;
    audiostate.env.hpfilt.lastout[1] = 0;
    audiostate.env.reverb.delay = 0.0f;
    audiostate.env.reverb.mix = 0.0f;
    audiostate.env.reverb.feedback = 0.0f;
    audiostate.env.reverb.merge = 0.5f;
    audiostate.env.reverb.lpfilt = 0.0f;
    audiostate.env.reverb.hpfilt = 0.0f;
    audiostate.env.reverb.state = (struct audioreverbstate){.filtdiv = outspec.freq};
    tmp = cfg_getvar(&config, "Audio", "adjfilters");
    if (tmp) {
        adjfilters = strtoul(tmp, NULL, 10);
        free(tmp);
    }
    audiostate.valid = true;
    #ifndef PSRC_USESDL1
    SDL_PauseAudioDevice(output, 0);
    #else
    SDL_PauseAudio(0);
    #endif
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
    return true;
}

void stopAudio(void) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) goto ret;
    audiostate.valid = false;
    ret:;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&audiostate.lock);
    #endif
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
