#include "audio.h"

#include "../debug.h"
#include "../attribs.h"
#include "../logging.h"
#include "../common.h"
#include "../util.h"

#include "client.h"

#include "../common/config.h"
#include "../common/math.h"

#include <stdlib.h>
#include <limits.h>

struct audiostate audiostate;

static int16_t* mixsound_cb_wav(struct audiosound* s, long loop, long pos, long* start, long* end) {
    (void)loop;
    if (!end) {
        if (s->rc->is8bit) free(s->wav.cvtbuf);
        unlockRc(s->rc);
        return NULL;
    }
    if (!s->rc->is8bit) {
        *start = 0;
        *end = s->rc->len - 1;
        return (int16_t*)s->rc->data;
    }
    int16_t* dataout = s->wav.cvtbuf;
    long tmpend = pos + audiostate.decbuflen;
    if (tmpend > s->rc->len) tmpend = s->rc->len;
    *start = pos;
    *end = tmpend - 1;
    tmpend -= pos;
    tmpend *= (long)s->rc->channels;
    uint8_t* datain = s->rc->data;
    for (register long i = 0, i2 = pos * (long)s->rc->channels; i < tmpend; ++i, ++i2) {
        register int tmp = datain[i2];
        //printf("[%lu, %lu] %lu %lu %lu\n", *start, *end, pos, i, i2);
        dataout[i] = (tmp - 128) * 256 + tmp;
    }
    return dataout;
}
#ifdef PSRC_USESTBVORBIS
static int16_t* mixsound_cb_vorbis(struct audiosound* s, long loop, long pos, long* start, long* end) {
    (void)loop;
    if (!end) {
        stb_vorbis_close(s->vorbis.state);
        free(s->vorbis.decbuf);
        unlockRc(s->rc);
        return NULL;
    }
    if (pos < s->vorbis.decbufhead) {
        pos -= audiostate.decbuflen - 1;
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
    return s->vorbis.decbuf;
}
#endif
#ifdef PSRC_USEMINIMP3
static int16_t* mixsound_cb_mp3(struct audiosound* s, long loop, long pos, long* start, long* end) {
    (void)loop;
    if (!end) {
        mp3dec_ex_close(s->mp3.state);
        free(s->mp3.decbuf);
        unlockRc(s->rc);
        return NULL;
    }
    if (pos < s->mp3.decbufhead) {
        pos -= audiostate.decbuflen - 1;
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
    return s->mp3.decbuf;
}
#endif

static inline void deleteSound(struct audiosound* s) {
    audiocb cb;
    void* ctx;
    if (!(s->iflags & SOUNDIFLAG_USESCB)) {
        switch (s->rc->format) {
            DEFAULTCASE(RC_SOUND_FRMT_WAV): cb = (audiocb)mixsound_cb_wav; break;
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
    cb(ctx, 0, 0, NULL, NULL);
}
static void delete3DSound(size_t si, struct audiosound* s) {
    deleteSound(s);
    s->prio = AUDIOPRIO_INVALID;
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
    s->prio = AUDIOPRIO_INVALID;
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

uint32_t new3DAudioEmitter(uint32_t pl, int8_t prio, unsigned maxsnd, unsigned f, unsigned fxmask, const struct audiofx* fx, unsigned fx3dmask, const struct audio3dfx* fx3d) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    uint32_t ei;
    if (!audiostate.valid) {ei = -1; goto ret;}
    struct audioemitter3d* e;
    {
        uint32_t i = 0;
        while (1) {
            if (i == audiostate.emitters3d.len) {
                if (audiostate.emitters3d.len == UINT32_MAX) {ei = -1; goto ret;}
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
    e->player = pl;
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
            e->fx3d.pos = fx3d->pos;
        } else {
            e->fx3d.pos = WORLDCOORD(0, 0, 0, 0.0f, 0.0f, 0.0f);
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
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
    return ei;
}
uint32_t new2DAudioEmitter(uint32_t pl, int8_t prio, unsigned maxsnd, unsigned f, unsigned fxmask, const struct audiofx* fx) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    uint32_t ei;
    if (!audiostate.valid) {ei = -1; goto ret;}
    struct audioemitter2d* e;
    {
        uint32_t i = 0;
        while (1) {
            if (i == audiostate.emitters2d.len) {
                if (audiostate.emitters2d.len == UINT32_MAX) {ei = -1; goto ret;}
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
    e->player = pl;
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
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
    return ei;
}

void edit3DAudioEmitter(uint32_t ei, unsigned fen, unsigned fdis, unsigned fxmask, const struct audiofx* fx, unsigned fx3dmask, const struct audio3dfx* fx3d, unsigned imm) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid || ei >= audiostate.emitters3d.len) goto ret;
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
        if (fx3dmask & AUDIO3DFXMASK_POS) e->fx3d.pos = fx3d->pos;
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
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void edit2DAudioEmitter(uint32_t ei, unsigned fen, unsigned fdis, unsigned fxmask, const struct audiofx* fx, unsigned imm) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid || ei >= audiostate.emitters2d.len) goto ret;
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
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
}

static void stop3DAudioEmitter_internal(uint32_t ei, struct audioemitter3d* e) {
    for (uint32_t i = 0; i < audiostate.sounds3d.len; ++i) {
        struct audiosound* s = &audiostate.sounds3d.data[i];
        if (s->prio != AUDIOPRIO_INVALID && s->emitter == ei) {
            delete3DSound(i, s);
            --e->cursounds;
        }
    }
}
static void stop2DAudioEmitter_internal(uint32_t ei, struct audioemitter2d* e) {
    for (uint32_t i = 0; i < audiostate.sounds2d.len; ++i) {
        struct audiosound* s = &audiostate.sounds2d.data[i];
        if (s->prio != AUDIOPRIO_INVALID && s->emitter == ei) {
            delete2DSound(i, s);
            --e->cursounds;
        }
    }
}

void stop3DAudioEmitter(uint32_t ei) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (audiostate.valid && ei < audiostate.emitters3d.len) stop3DAudioEmitter_internal(ei, &audiostate.emitters3d.data[ei]);
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void stop2DAudioEmitter(uint32_t ei) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (audiostate.valid && ei < audiostate.emitters2d.len) stop2DAudioEmitter_internal(ei, &audiostate.emitters2d.data[ei]);
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
}

void delete3DAudioEmitter(uint32_t ei) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (audiostate.valid && ei < audiostate.emitters3d.len) {
        struct audioemitter3d* e = &audiostate.emitters3d.data[ei];
        if (e->prio != AUDIOPRIO_INVALID) {
            stop3DAudioEmitter_internal(ei, e);
            e->prio = AUDIOPRIO_INVALID;
        }
    }
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
}
void delete2DAudioEmitter(uint32_t ei) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (audiostate.valid && ei < audiostate.emitters2d.len) {
        struct audioemitter2d* e = &audiostate.emitters2d.data[ei];
        if (e->prio != AUDIOPRIO_INVALID) {
            stop2DAudioEmitter_internal(ei, e);
            e->prio = AUDIOPRIO_INVALID;
        }
    }
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
}

static void initSound(struct audiosound* s, unsigned fxmask, const struct audiofx* fx) {
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
}
static inline bool initSoundRc(struct audiosound* s, struct rc_sound* rc) {
    switch (rc->format) {
        DEFAULTCASE(RC_SOUND_FRMT_WAV):
            if (rc->is8bit && !(s->wav.cvtbuf = rcmgr_malloc(audiostate.decbuflen * rc->channels * sizeof(*s->wav.cvtbuf)))) return false;
            //s->wav.cvtbufhead = rc->len;
            //s->wav.cvtbuflen = 0;
            break;
        #ifdef PSRC_USESTBVORBIS
        case RC_SOUND_FRMT_VORBIS:
            if (!(s->vorbis.decbuf = rcmgr_malloc(audiostate.decbuflen * rc->channels * sizeof(*s->vorbis.decbuf)))) return false;
            if (!(s->vorbis.state = stb_vorbis_open_memory(rc->data, rc->size, NULL, NULL))) {
                free(s->vorbis.decbuf);
                return false;
            }
            s->vorbis.decbufhead = rc->len;
            s->vorbis.decbuflen = 0;
            break;
        #endif
        #ifdef PSRC_USEMINIMP3
        case RC_SOUND_FRMT_MP3:
            if (!(s->mp3.state = rcmgr_malloc(sizeof(*s->mp3.state)))) return false;
            if (!(s->mp3.decbuf = rcmgr_malloc(audiostate.decbuflen * rc->channels * sizeof(*s->mp3.decbuf)))) {
                free(s->mp3.state);
                return false;
            }
            if (mp3dec_ex_open_buf(s->mp3.state, rc->data, rc->size, MP3D_SEEK_TO_SAMPLE)) {
                free(s->mp3.decbuf);
                free(s->mp3.state);
                return false;
            }
            s->mp3.decbufhead = rc->len;
            s->mp3.decbuflen = 0;
            break;
        #endif
    }
    lockRc(rc);
    s->rc = rc;
    return true;
}
static inline struct audiosound* new3DSound(void) {
    struct audiosound* s;
    size_t i = 0;
    while (1) {
        if (i == audiostate.sounds3d.len) {
            if (audiostate.sounds3d.len == (size_t)audiostate.max3dsounds) return NULL;
            VLB_NEXTPTR(audiostate.sounds3d, s, 2, 1, return NULL;);
            break;
        }
        struct audiosound* tmps = &audiostate.sounds3d.data[i];
        if (tmps->prio == AUDIOPRIO_INVALID) {
            s = tmps;
            break;
        }
        ++i;
    }
    VLB_ADD(audiostate.sounds3dorder, i, 2, 1, return NULL;);
    return s;
}
bool play3DSound(uint32_t ei, struct rc_sound* rc, int8_t prio, uint8_t flags, unsigned fxmask, const struct audiofx* fx) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid || ei >= audiostate.emitters3d.len) goto retfalse;
    struct audioemitter3d* e = &audiostate.emitters3d.data[ei];
    if (e->cursounds == e->maxsounds) goto retfalse;
    struct audiosound* s = new3DSound();
    if (!s) goto retfalse;
    if (!initSoundRc(s, rc)) {
        --audiostate.sounds3dorder.len;
        s->prio = AUDIOPRIO_INVALID;
        goto retfalse;
    }
    ++e->cursounds;
    if (prio == AUDIOPRIO_DEFAULT) prio = audiostate.emitters3d.data[ei].prio;
    s->emitter = ei;
    s->prio = prio;
    s->flags = flags;
    initSound(s, fxmask, fx);
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
    return true;
    retfalse:;
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
    return false;
}
bool play3DSoundCB(uint32_t ei, struct audiosoundcb* cb, int8_t prio, uint8_t flags, unsigned fxmask, const struct audiofx* fx) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid || ei >= audiostate.emitters3d.len) goto retfalse;
    struct audioemitter3d* e = &audiostate.emitters3d.data[ei];
    if (e->cursounds == e->maxsounds) goto retfalse;
    struct audiosound* s = new3DSound();
    if (!s) goto retfalse;
    ++e->cursounds;
    if (prio == AUDIOPRIO_DEFAULT) prio = audiostate.emitters3d.data[ei].prio;
    s->emitter = ei;
    s->prio = prio;
    s->flags = flags;
    s->cb = *cb;
    initSound(s, fxmask, fx);
    s->iflags |= SOUNDIFLAG_USESCB;
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
    return true;
    retfalse:;
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
    return false;
}
static inline struct audiosound* new2DSound(void) {
    struct audiosound* s;
    size_t i = 0;
    while (1) {
        if (i == audiostate.sounds2d.len) {
            if (audiostate.sounds2d.len == (size_t)audiostate.max2dsounds) return NULL;
            VLB_NEXTPTR(audiostate.sounds2d, s, 2, 1, return NULL;);
            break;
        }
        struct audiosound* tmps = &audiostate.sounds2d.data[i];
        if (tmps->prio == AUDIOPRIO_INVALID) {
            s = tmps;
            break;
        }
        ++i;
    }
    VLB_ADD(audiostate.sounds2dorder, i, 2, 1, return NULL;);
    return s;
}
bool play2DSound(uint32_t ei, struct rc_sound* rc, int8_t prio, uint8_t flags, unsigned fxmask, const struct audiofx* fx) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid || ei >= audiostate.emitters2d.len) goto retfalse;
    struct audioemitter2d* e = &audiostate.emitters2d.data[ei];
    if (e->cursounds == e->maxsounds) goto retfalse;
    struct audiosound* s = new2DSound();
    if (!s) goto retfalse;
    if (!initSoundRc(s, rc)) {
        --audiostate.sounds2dorder.len;
        s->prio = AUDIOPRIO_INVALID;
        goto retfalse;
    }
    ++e->cursounds;
    if (prio == AUDIOPRIO_DEFAULT) prio = audiostate.emitters2d.data[ei].prio;
    s->emitter = ei;
    s->prio = prio;
    s->flags = flags;
    initSound(s, fxmask, fx);
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
    return true;
    retfalse:;
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
    return false;
}
bool play2DSoundCB(uint32_t ei, struct audiosoundcb* cb, int8_t prio, uint8_t flags, unsigned fxmask, const struct audiofx* fx) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid || ei >= audiostate.emitters2d.len) goto retfalse;
    struct audioemitter2d* e = &audiostate.emitters2d.data[ei];
    if (e->cursounds == e->maxsounds) goto retfalse;
    struct audiosound* s = new2DSound();
    if (!s) goto retfalse;
    ++e->cursounds;
    if (prio == AUDIOPRIO_DEFAULT) prio = audiostate.emitters2d.data[ei].prio;
    s->emitter = ei;
    s->prio = prio;
    s->flags = flags;
    s->cb = *cb;
    initSound(s, fxmask, fx);
    s->iflags |= SOUNDIFLAG_USESCB;
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
    return true;
    retfalse:;
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
    return false;
}

void setAudioEnv(uint32_t pl, unsigned mask, struct audioenv* in, unsigned imm) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid || pl >= audiostate.playerdata.len || !audiostate.playerdata.data[pl].valid) goto ret;
    struct audioenvstate* env = &audiostate.playerdata.data[pl].env;
    env->envch |= mask;
    env->envchimm |= imm;
    if (mask & AUDIOENVMASK_PANNING) env->panning = in->panning;
    if (mask & AUDIOENVMASK_LPFILT) env->lpfilt.amount = in->lpfilt;
    if (mask & AUDIOENVMASK_HPFILT) env->hpfilt.amount = in->hpfilt;
    if (mask & AUDIOENVMASK_REVERB_DELAY) env->reverb.delay = in->reverb.delay;
    if (mask & AUDIOENVMASK_REVERB_MIX) env->reverb.mix = in->reverb.mix;
    if (mask & AUDIOENVMASK_REVERB_FEEDBACK) env->reverb.feedback = in->reverb.feedback;
    if (mask & AUDIOENVMASK_REVERB_MERGE) env->reverb.merge = in->reverb.merge;
    if (mask & AUDIOENVMASK_REVERB_LPFILT) env->reverb.lpfilt = in->reverb.lpfilt;
    if (mask & AUDIOENVMASK_REVERB_HPFILT) env->reverb.hpfilt = in->reverb.hpfilt;
    ret:;
    #if PSRC_MTLVL >= 2
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
        out[0] = out[0] * feedback / 256 + in[0];
        out[1] = out[1] * feedback / 256 + in[1];
        if (out[0] < -32768) out[0] = -32768;
        else if (out[0] > 32767) out[0] = 32767;
        if (out[1] < -32768) out[1] = -32768;
        else if (out[1] > 32767) out[1] = 32767;
        r->buf[0][head] = out[0];
        r->buf[1][head] = out[1];
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
        out[0] = out[0] * feedback / 256 + in[0];
        out[1] = out[1] * feedback / 256 + in[1];
        if (out[0] < -32768) out[0] = -32768;
        else if (out[0] > 32767) out[0] = 32767;
        if (out[1] < -32768) out[1] = -32768;
        else if (out[1] > 32767) out[1] = 32767;
        r->buf[0][head] = out[0];
        r->buf[1][head] = out[1];
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
    //if (!e->cursounds) return;
    struct audioplayerdata* pldata = &audiostate.playerdata.data[e->player];
    if (!pldata->valid) return;
    float pos[3];
    float mincoord[3];
    float maxcoord[3];
    if (!e->fx3d.relpos) {
        wc_diff(&pldata->pos, &e->fx3d.pos, pos);
        mincoord[0] = pos[0] - e->fx3d.radius[0];
        mincoord[1] = pos[1] - e->fx3d.radius[1];
        mincoord[2] = pos[2] - e->fx3d.radius[2];
        maxcoord[0] = pos[0] + e->fx3d.radius[0];
        maxcoord[1] = pos[1] + e->fx3d.radius[1];
        maxcoord[2] = pos[2] + e->fx3d.radius[2];
    } else {
        mincoord[0] = e->fx3d.pos.pos[0] - e->fx3d.radius[0];
        mincoord[1] = e->fx3d.pos.pos[1] - e->fx3d.radius[1];
        mincoord[2] = e->fx3d.pos.pos[2] - e->fx3d.radius[2];
        maxcoord[0] = e->fx3d.pos.pos[0] + e->fx3d.radius[0];
        maxcoord[1] = e->fx3d.pos.pos[1] + e->fx3d.radius[1];
        maxcoord[2] = e->fx3d.pos.pos[2] + e->fx3d.radius[2];
    }
    pos[0] = (maxcoord[0] > 0.0f) ? ((mincoord[0] < 0.0f) ? 0.0f : mincoord[0]) : maxcoord[0];
    pos[1] = (maxcoord[1] > 0.0f) ? ((mincoord[1] < 0.0f) ? 0.0f : mincoord[1]) : maxcoord[1];
    pos[2] = (maxcoord[2] > 0.0f) ? ((mincoord[2] < 0.0f) ? 0.0f : mincoord[2]) : maxcoord[2];
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
    if (!e->fx3d.relrot) {
        float mat[3][3];
        //printf("[%p]:\n", (void*)e);
        mat3_createrotmat(pldata->rotsin, pldata->rotcos, mat);
        //printf("[%.3f, %.3f, %.3f]\n", (double)mat[0][0], (double)mat[0][1], (double)mat[0][2]);
        //printf("[%.3f, %.3f, %.3f]\n", (double)mat[1][0], (double)mat[1][1], (double)mat[1][2]);
        //printf("[%.3f, %.3f, %.3f]\n", (double)mat[2][0], (double)mat[2][1], (double)mat[2][2]);
        //printf("[%.3f, %.3f, %.3f] -> ", (double)pos[0], (double)pos[1], (double)pos[2]);
        mat3_mul_vec3(mat, pos, pos);
        //printf(" [%.3f, %.3f, %.3f]\n", (double)pos[0], (double)pos[1], (double)pos[2]);
        //vec3_trigrotate(pos, pldata->rotsin, pldata->rotcos, pos);
    }
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
        int64_t toff = -(e->fx.toff + s->fx.toff) * (int64_t)audiostate.freq / 1000000 + e->fx3dout.posoff;
        if (imm & AUDIOFXMASK_TOFF) {
            long oldposoff = s->calcfx[curfxi].posoff;
            s->calcfx[curfxi].posoff = s->calcfx[newfxi].posoff = toff;
            toff -= oldposoff;
            toff *= ((!(s->iflags & SOUNDIFLAG_USESCB)) ? s->rc->freq : s->cb.freq);
            long loop = s->loop;
            long pos = s->pos;
            long frac = s->frac;
            pos += toff / (int64_t)audiostate.freq;
            frac += (toff % (int64_t)audiostate.freq) * 256;
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
        int64_t toff = -(e->fx.toff + s->fx.toff) * (int64_t)audiostate.freq / 1000000;
        if (imm & AUDIOFXMASK_TOFF) {
            long oldposoff = s->calcfx[curfxi].posoff;
            s->calcfx[curfxi].posoff = s->calcfx[newfxi].posoff = toff;
            toff -= oldposoff;
            toff *= ((!(s->iflags & SOUNDIFLAG_USESCB)) ? s->rc->freq : s->cb.freq);
            long loop = s->loop;
            long pos = s->pos;
            long frac = s->frac;
            pos += toff / (int64_t)audiostate.freq;
            frac += (toff % (int64_t)audiostate.freq) * 256;
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

static inline void applyAudioEnv(uint32_t pl, int** inp, int** outp) {
    struct audioenvstate* env = &audiostate.playerdata.data[pl].env;
    if (env->envch & AUDIOENVMASK_REVERB_DELAY) {
        unsigned oldlen = env->reverb.state.len;
        unsigned newlen;
        {
            float tmp = env->reverb.delay;
            if (tmp < 0.0f) tmp = 0.0f;
            else if (tmp > 5.0f) tmp = 5.0f;
            newlen = roundf(tmp * audiostate.freq);
        }
        env->reverb.state.len = newlen;
        if (!newlen) {
            env->reverb.state.size = 0;
            free(env->reverb.state.buf[0]);
            free(env->reverb.state.buf[1]);
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
                unsigned oldsize = env->reverb.state.size;
                unsigned minlen = (newlen <= oldlen) ? newlen : oldlen;
                unsigned pos = (env->reverb.state.head + (oldsize - minlen)) % oldsize;
                for (unsigned i = 0; i < minlen; ++i) {
                    newbuf[0][i] = env->reverb.state.buf[0][pos];
                    newbuf[1][i] = env->reverb.state.buf[1][pos];
                    pos = (pos + 1) % oldsize;
                }
                for (unsigned i = minlen; i < newsize; ++i) {
                    newbuf[0][i] = 0;
                    newbuf[1][i] = 0;
                }
                free(env->reverb.state.buf[0]);
                free(env->reverb.state.buf[1]);
                env->reverb.state.buf[0] = newbuf[0];
                env->reverb.state.buf[1] = newbuf[1];
                env->reverb.state.size = newsize;
                env->reverb.state.head = 0;
                env->reverb.state.tail = newsize - newlen;
            } else {
                env->reverb.state.buf[0] = rcmgr_calloc(newsize, sizeof(**env->reverb.state.buf));
                env->reverb.state.buf[1] = rcmgr_calloc(newsize, sizeof(**env->reverb.state.buf));
                env->reverb.state.size = newsize;
                env->reverb.state.head = 0;
                env->reverb.state.tail = newsize - newlen;
            }
        }
    }
    if (env->envch & (AUDIOENVMASK_REVERB & ~AUDIOENVMASK_REVERB_DELAY)) {
        uint_fast8_t curparami = env->reverb.state.parami;
        uint_fast8_t newparami = (curparami + 1) % 2;
        if (env->envch & AUDIOENVMASK_REVERB_MIX) {
            env->reverb.state.mix[newparami] = roundf(env->reverb.mix * 256.0f);
            if (env->envchimm & AUDIOENVMASK_REVERB_MIX) {
                env->reverb.state.mix[curparami] = env->reverb.state.mix[newparami];
            }
        } else {
            env->reverb.state.mix[newparami] = env->reverb.state.mix[curparami];
        }
        if (env->envch & AUDIOENVMASK_REVERB_FEEDBACK) {
            env->reverb.state.feedback[newparami] = roundf(env->reverb.feedback * 256.0f);
            if (env->envchimm & AUDIOENVMASK_REVERB_FEEDBACK) {
                env->reverb.state.feedback[curparami] = env->reverb.state.feedback[newparami];
            }
        } else {
            env->reverb.state.feedback[newparami] = env->reverb.state.feedback[curparami];
        }
        if (env->envch & AUDIOENVMASK_REVERB_MERGE) {
            env->reverb.state.merge[newparami] = roundf(env->reverb.merge * 256.0f);
            if (env->envchimm & AUDIOENVMASK_REVERB_MERGE) {
                env->reverb.state.merge[curparami] = env->reverb.state.merge[newparami];
            }
        } else {
            env->reverb.state.merge[newparami] = env->reverb.state.merge[curparami];
        }
        if (env->envch & AUDIOENVMASK_REVERB_LPFILT) {
            float tmpf = 1.0f - env->reverb.lpfilt;
            unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
            if (adjfilters) ADJLPFILTMUL(tmpf, tmpu, adjfilters);
            if (tmpu > audiostate.freq) tmpu = audiostate.freq;
            env->reverb.state.lpfilt[newparami] = tmpu;
            if (env->envch & AUDIOENVMASK_REVERB_LPFILT) {
                env->reverb.state.lpfilt[curparami] = tmpu;
            }
        } else {
            env->reverb.state.lpfilt[newparami] = env->reverb.state.lpfilt[curparami];
        }
        if (env->envch & AUDIOENVMASK_REVERB_HPFILT) {
            float tmpf = env->reverb.hpfilt;
            unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
            if (adjfilters) ADJHPFILTMUL(tmpf, tmpu, adjfilters);
            if (tmpu >= audiostate.freq) tmpu = 0;
            else tmpu = audiostate.freq - tmpu;
            env->reverb.state.hpfilt[newparami] = tmpu;
            if (env->envch & AUDIOENVMASK_REVERB_HPFILT) {
                env->reverb.state.hpfilt[curparami] = tmpu;
            }
        } else {
            env->reverb.state.hpfilt[newparami] = env->reverb.state.hpfilt[curparami];
        }
        if (env->reverb.state.len) {
            if (env->envch & ~env->envchimm) {
			    doReverb_interp(&env->reverb.state, audiostate.buflen, inp[0], inp[1]);
            } else {
                doReverb(&env->reverb.state, audiostate.buflen, inp[0], inp[1]);
		    }
	    }
        env->reverb.state.parami = newparami;
    } else if (env->reverb.state.len) {
        doReverb(&env->reverb.state, audiostate.buflen, inp[0], inp[1]);
    }
    if (env->envch & AUDIOENVMASK_HPFILT) {
        uint_fast8_t curmuli = env->hpfilt.muli;
        uint_fast8_t newmuli = (curmuli + 1) % 2;
        env->hpfilt.muli = newmuli;
        float tmpf = env->hpfilt.amount;
        unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters) ADJHPFILTMUL(tmpf, tmpu, adjfilters);
        if (tmpu >= audiostate.freq) tmpu = 0;
        else tmpu = audiostate.freq - tmpu;
        env->hpfilt.mul[newmuli] = tmpu;
        if (env->envchimm & AUDIOENVMASK_HPFILT) {
            env->hpfilt.mul[curmuli] = tmpu;
            doHPFilter(
                tmpu, audiostate.freq,
                &env->hpfilt.lastin[0], &env->hpfilt.lastout[0], audiostate.buflen, inp[0]
            );
            doHPFilter(
                tmpu, audiostate.freq,
                &env->hpfilt.lastin[1], &env->hpfilt.lastout[1], audiostate.buflen, inp[1]
            );
        } else {
            doHPFilter_interp(
                env->hpfilt.mul[curmuli], tmpu, audiostate.freq,
                &env->hpfilt.lastin[0], &env->hpfilt.lastout[0], audiostate.buflen, inp[0]
            );
            doHPFilter_interp(
                env->hpfilt.mul[curmuli], tmpu, audiostate.freq,
                &env->hpfilt.lastin[1], &env->hpfilt.lastout[1], audiostate.buflen, inp[1]
            );
        }
    } else {
        doHPFilter(
            env->hpfilt.mul[env->hpfilt.muli], audiostate.freq,
            &env->hpfilt.lastin[0], &env->hpfilt.lastout[0], audiostate.buflen, inp[0]
        );
        doHPFilter(
            env->hpfilt.mul[env->hpfilt.muli], audiostate.freq,
            &env->hpfilt.lastin[1], &env->hpfilt.lastout[1], audiostate.buflen, inp[1]
        );
    }
    if (env->envch & AUDIOENVMASK_LPFILT) {
        uint_fast8_t curmuli = env->lpfilt.muli;
        uint_fast8_t newmuli = (curmuli + 1) % 2;
        env->lpfilt.muli = newmuli;
        float tmpf = 1.0f - env->lpfilt.amount;
        unsigned tmpu = roundf(tmpf * tmpf * audiostate.freq);
        if (adjfilters) ADJLPFILTMUL(tmpf, tmpu, adjfilters);
        if (tmpu > audiostate.freq) tmpu = audiostate.freq;
        env->lpfilt.mul[newmuli] = tmpu;
        if (env->envchimm & AUDIOENVMASK_LPFILT) {
            env->lpfilt.mul[curmuli] = tmpu;
            doLPFilter(
                tmpu, audiostate.freq,
                &env->lpfilt.lastout[0], audiostate.buflen, inp[0]
            );
            doLPFilter(
                tmpu, audiostate.freq,
                &env->lpfilt.lastout[1], audiostate.buflen, inp[1]
            );
        } else {
            doLPFilter_interp(
                env->lpfilt.mul[curmuli], tmpu, audiostate.freq,
                &env->lpfilt.lastout[0], audiostate.buflen, inp[0]
            );
            doLPFilter_interp(
                env->lpfilt.mul[curmuli], tmpu, audiostate.freq,
                &env->lpfilt.lastout[1], audiostate.buflen, inp[1]
            );
        }
    } else {
        doLPFilter(
            env->lpfilt.mul[env->lpfilt.muli], audiostate.freq,
            &env->lpfilt.lastout[0], audiostate.buflen, inp[0]
        );
        doLPFilter(
            env->lpfilt.mul[env->lpfilt.muli], audiostate.freq,
            &env->lpfilt.lastout[1], audiostate.buflen, inp[1]
        );
    }
    int* in[2] = {inp[0], inp[1]};
    int* out[2] = {outp[0], outp[1]};
    if (env->panning == 0.0f) {
        for (register unsigned i = 0; i < audiostate.buflen; ++i) {
            out[0][i] = in[0][i];
            out[1][i] = in[1][i];
        }
    } else {
        // TODO
        for (register unsigned i = 0; i < audiostate.buflen; ++i) {
            out[0][i] = in[0][i];
            out[1][i] = in[1][i];
        }
    }
    env->envch = 0;
    env->envchimm = 0;
}

#define MIXSOUND__DOMIXING(l) do {\
    if (!oob) {\
        register int sample_l, sample_r;\
        if (pos > bufend || pos < bufstart) {\
            /*fputs("NORM ", stdout);*/\
            if (!(buf = cb(ctx, loop, pos, &bufstart, &bufend))) {\
                oob = true;\
                goto cbfail_##l;\
            }\
        }\
        register long bufpos = (pos - bufstart) * (long)ch;\
        /*printf("%ld %ld %ld %ld %ld %ld\n", bufstart, bufend, pos, pos - bufstart, (long)ch, bufpos);*/\
        sample_l = buf[bufpos];\
        sample_r = buf[bufpos + (ch != 1)];\
        if (frac) {\
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
                /*fputs("FRAC ", stdout);*/\
                if (!(buf = cb(ctx, loop2, pos2, &bufstart, &bufend))) goto skipinterp_##l;\
            }\
            int mix = frac / outfreq;\
            int imix = 256 - mix;\
            sample_l *= imix;\
            sample_r *= imix;\
            bufpos = (pos2 - bufstart) * (long)ch;\
            sample_l += buf[bufpos] * mix;\
            sample_r += buf[bufpos + (ch != 1)] * mix;\
            sample_l /= 256;\
            sample_r /= 256;\
            skipinterp_##l:;\
        }\
        audiostate.fxbuf[0][i] = sample_l;\
        audiostate.fxbuf[1][i] = sample_r;\
    } else {\
        cbfail_##l:;\
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
static bool mixsound(struct audiosound* s, int** outp) {
    audiocb cb;
    void* ctx;
    long len;
    long freq;
    unsigned ch;
    if (!(s->iflags & SOUNDIFLAG_USESCB)) {
        switch (s->rc->format) {
            DEFAULTCASE(RC_SOUND_FRMT_WAV): cb = (audiocb)mixsound_cb_wav; break;
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
    long div = outfreq * 256;

    uint_fast8_t curfxi = s->fxi;
    uint_fast8_t newfxi = (curfxi + 1) % 2;

    int vol[2][2];
    if (outp) {
        if (!(s->fxch & AUDIOFXMASK_VOL)) {
            vol[0][0] = s->calcfx[curfxi].volmul[0];
            vol[0][1] = s->calcfx[curfxi].volmul[1];
            if (!vol[0][0] && !vol[0][1]) outp = NULL;
            //printf("VOL %p: [%d, %d]\n", (void*)s, vol[0][0], vol[0][1]);
        } else {
            vol[0][0] = s->calcfx[curfxi].volmul[0];
            vol[0][1] = s->calcfx[curfxi].volmul[1];
            vol[1][0] = s->calcfx[newfxi].volmul[0];
            vol[1][1] = s->calcfx[newfxi].volmul[1];
            if (!vol[0][0] && !vol[0][1] && !vol[1][0] && !vol[1][1]) outp = NULL;
            //printf("VOL %p: [%d, %d], [%d, %d]\n", (void*)s, vol[0][0], vol[0][1], vol[1][0], vol[1][1]);
        }
    }

    int16_t* buf;
    long bufstart = 0, bufend = -1;

    if (!(s->fxch & (AUDIOFXMASK_TOFF | AUDIOFXMASK_SPEED))) {
        long speedmul = s->calcfx[curfxi].speedmul;
        long change = freq * speedmul;
        register unsigned i = 0;
        if (outp) {
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
        if (outp) {
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
                MIXSOUND_DOPOSMATH(++i; if (i == (long)audiostate.buflen) goto brkloop_2; --ii; goto skipoobchk_2;);
                ++i;
                if (i == (long)audiostate.buflen) break;
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
                if (i == (long)audiostate.buflen) break;
                --ii;
            }
        }
        posoff[1] -= posoff[0];
        pos += posoff[1] / outfreq;
        posoff[1] %= outfreq;
        frac += posoff[1] * 256;
        MIXSOUND_DOPOSMATH();
    }
    if (outp) {
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
        int* out[2] = {outp[0], outp[1]};
        if (!(s->fxch & AUDIOFXMASK_VOL)) {
            for (register unsigned i = 0; i < audiostate.buflen; ++i) {
                out[0][i] += audiostate.fxbuf[0][i] * vol[0][0] / 32768;
                out[1][i] += audiostate.fxbuf[1][i] * vol[0][1] / 32768;
            }
        } else {
            for (register unsigned i = 0, ii = audiostate.buflen; i < audiostate.buflen; ++i, --ii) {
                int vol_l = (vol[0][0] * ii + vol[1][0] * i) / audiostate.buflen;
                int vol_r = (vol[0][1] * ii + vol[1][1] * i) / audiostate.buflen;
                out[0][i] += audiostate.fxbuf[0][i] * vol_l / 32768;
                out[1][i] += audiostate.fxbuf[1][i] * vol_r / 32768;
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

    if (audiostate.sounds3dorder.len > 1) qsort(
        audiostate.sounds3dorder.data, audiostate.sounds3dorder.len, sizeof(*audiostate.sounds3dorder.data),
        mixsounds_3dsortcb
    );
    if (audiostate.sounds2dorder.len > 1) qsort(
        audiostate.sounds2dorder.data, audiostate.sounds2dorder.len, sizeof(*audiostate.sounds2dorder.data),
        mixsounds_2dsortcb
    );

    memset(audiostate.mixbuf[0], 0, audiostate.buflen * sizeof(**audiostate.mixbuf));
    memset(audiostate.mixbuf[1], 0, audiostate.buflen * sizeof(**audiostate.mixbuf));

    for (uint32_t pli = 0; pli < audiostate.playerdata.len; ++pli) {
        if (!audiostate.playerdata.data[pli].valid) continue;
        memset(audiostate.envbuf[0], 0, audiostate.buflen * sizeof(**audiostate.envbuf));
        memset(audiostate.envbuf[1], 0, audiostate.buflen * sizeof(**audiostate.envbuf));
        size_t tmp = (audiostate.sounds3dorder.len <= audiostate.voices3d) ? audiostate.sounds3dorder.len : audiostate.voices3d;
        for (size_t i = 0; i < tmp; ++i) {
            size_t si = audiostate.sounds3dorder.data[i];
            struct audiosound* s = &audiostate.sounds3d.data[si];
            struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
            if (!(e->flags & AUDIOEMITTER3DFLAG_PAUSED) && !(e->flags & AUDIOEMITTER3DFLAG_NOENV)) {
                if (!mixsound(s, audiostate.envbuf)) delete3DSound(si, s);
            }
        }
        tmp = (audiostate.sounds2dorder.len <= audiostate.voices2d) ? audiostate.sounds2dorder.len : audiostate.voices2d;
        for (size_t i = 0; i < tmp; ++i) {
            size_t si = audiostate.sounds2dorder.data[i];
            struct audiosound* s = &audiostate.sounds2d.data[si];
            struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
            if (!(e->flags & AUDIOEMITTER2DFLAG_PAUSED) && (e->flags & AUDIOEMITTER2DFLAG_APPLYENV)) {
                if (!mixsound(s, audiostate.envbuf)) delete2DSound(si, s);
            }
        }
        applyAudioEnv(pli, audiostate.envbuf, audiostate.mixbuf);
    }
    {
        size_t tmp = audiostate.sounds3dorder.len;
        for (size_t i = audiostate.voices3d; i < tmp; ++i) {
            size_t si = audiostate.sounds3dorder.data[i];
            struct audiosound* s = &audiostate.sounds3d.data[si];
            struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
            if (!(e->flags & AUDIOEMITTER3DFLAG_PAUSED)) {
                if (!mixsound(s, NULL)) delete3DSound(si, s);
            }
        }
        if (tmp > audiostate.voices3d) tmp = audiostate.voices3d;
        for (size_t i = 0; i < tmp; ++i) {
            size_t si = audiostate.sounds3dorder.data[i];
            struct audiosound* s = &audiostate.sounds3d.data[si];
            struct audioemitter3d* e = &audiostate.emitters3d.data[s->emitter];
            if (!(e->flags & AUDIOEMITTER3DFLAG_PAUSED) && (e->flags & AUDIOEMITTER3DFLAG_NOENV)) {
                if (!mixsound(s, audiostate.mixbuf)) delete3DSound(si, s);
            }
        }
        tmp = audiostate.sounds2dorder.len;
        for (size_t i = audiostate.voices2d; i < tmp; ++i) {
            size_t si = audiostate.sounds2dorder.data[i];
            struct audiosound* s = &audiostate.sounds2d.data[si];
            struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
            if (!(e->flags & AUDIOEMITTER2DFLAG_PAUSED)) {
                if (!mixsound(s, NULL)) delete2DSound(si, s);
            }
        }
        if (tmp > audiostate.voices2d) tmp = audiostate.voices2d;
        for (size_t i = 0; i < tmp; ++i) {
            size_t si = audiostate.sounds2dorder.data[i];
            struct audiosound* s = &audiostate.sounds2d.data[si];
            struct audioemitter2d* e = &audiostate.emitters2d.data[s->emitter];
            if (!(e->flags & AUDIOEMITTER2DFLAG_PAUSED) && !(e->flags & AUDIOEMITTER2DFLAG_APPLYENV)) {
                if (!mixsound(s, audiostate.mixbuf)) delete2DSound(si, s);
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

static inline void initAudioPlayerData(struct audioplayerdata* pldata) {
    pldata->valid = true;
    pldata->env.envch = (uint16_t)AUDIOENVMASK_ALL;
    pldata->env.envchimm = (uint16_t)AUDIOENVMASK_ALL;
    pldata->env.panning = 0.0f;
    pldata->env.lpfilt.amount = 0.0f;
    pldata->env.lpfilt.muli = 0;
    pldata->env.lpfilt.lastout[0] = 0;
    pldata->env.lpfilt.lastout[1] = 0;
    pldata->env.hpfilt.amount = 0.0f;
    pldata->env.hpfilt.muli = 0;
    pldata->env.hpfilt.lastin[0] = 0;
    pldata->env.hpfilt.lastin[1] = 0;
    pldata->env.hpfilt.lastout[0] = 0;
    pldata->env.hpfilt.lastout[1] = 0;
    pldata->env.reverb.delay = 0.0f;
    pldata->env.reverb.mix = 0.0f;
    pldata->env.reverb.feedback = 0.0f;
    pldata->env.reverb.merge = 0.5f;
    pldata->env.reverb.lpfilt = 0.0f;
    pldata->env.reverb.hpfilt = 0.0f;
    pldata->env.reverb.state = (struct audioreverbstate){.filtdiv = audiostate.freq};
}
static inline void freeAudioPlayerData(struct audioplayerdata* pldata) {
    free(pldata->env.reverb.state.buf[0]);
    free(pldata->env.reverb.state.buf[1]);
    pldata->valid = false;
}
static inline void syncAudioPlayerData(struct player* pl, struct audioplayerdata* out) {
    out->pos = pl->common.cameracalc.pos;
    out->rotsin[0] = pl->common.cameracalc.isin[0];
    out->rotsin[1] = pl->common.cameracalc.isin[1];
    out->rotsin[2] = pl->common.cameracalc.isin[2];
    out->rotcos[0] = pl->common.cameracalc.icos[0];
    out->rotcos[1] = pl->common.cameracalc.icos[1];
    out->rotcos[2] = pl->common.cameracalc.icos[2];
}

static inline void updateAudioPlayerData(void) {
    unsigned syncmax;
    if (playerdata.len > audiostate.playerdata.len) {
        syncmax = audiostate.playerdata.len;
        VLB_EXPANDTO(audiostate.playerdata, playerdata.len, 3, 2, VLB_OOM_NOP);
        for (unsigned i = syncmax; i < audiostate.playerdata.len; ++i) {
            initAudioPlayerData(&audiostate.playerdata.data[i]);
            syncAudioPlayerData(&playerdata.data[i], &audiostate.playerdata.data[i]);
        }
    } else if (playerdata.len < audiostate.playerdata.len) {
        syncmax = playerdata.len;
        for (unsigned i = playerdata.len; i < audiostate.playerdata.len; ++i) {
            for (size_t ei = 0; ei < audiostate.emitters3d.len; ++ei) {
                struct audioemitter3d* e = &audiostate.emitters3d.data[ei];
                if (e->prio != AUDIOPRIO_INVALID && e->player == i) {
                    stop3DAudioEmitter_internal(ei, e);
                    e->prio = AUDIOPRIO_INVALID;
                }
            }
            for (size_t ei = 0; ei < audiostate.emitters2d.len; ++ei) {
                struct audioemitter2d* e = &audiostate.emitters2d.data[ei];
                if (e->prio != AUDIOPRIO_INVALID && e->player == i) {
                    stop2DAudioEmitter_internal(ei, e);
                    e->prio = AUDIOPRIO_INVALID;
                }
            }
            freeAudioPlayerData(&audiostate.playerdata.data[i]);
        }
        audiostate.playerdata.len = playerdata.len;
        VLB_SHRINK(audiostate.playerdata, VLB_OOM_NOP);
    } else {
        syncmax = audiostate.playerdata.len;
    }
    for (unsigned i = 0; i < syncmax; ++i) {
        if (playerdata.data[i].priv.username) {
            if (!audiostate.playerdata.data[i].valid) initAudioPlayerData(&audiostate.playerdata.data[i]);
            syncAudioPlayerData(&playerdata.data[i], &audiostate.playerdata.data[i]);
        } else {
            if (audiostate.playerdata.data[i].valid) {
                for (size_t ei = 0; ei < audiostate.emitters3d.len; ++ei) {
                    struct audioemitter3d* e = &audiostate.emitters3d.data[ei];
                    if (e->prio != AUDIOPRIO_INVALID && e->player == i) {
                        stop3DAudioEmitter_internal(ei, e);
                        e->prio = AUDIOPRIO_INVALID;
                    }
                }
                for (size_t ei = 0; ei < audiostate.emitters2d.len; ++ei) {
                    struct audioemitter2d* e = &audiostate.emitters2d.data[ei];
                    if (e->prio != AUDIOPRIO_INVALID && e->player == i) {
                        stop2DAudioEmitter_internal(ei, e);
                        e->prio = AUDIOPRIO_INVALID;
                    }
                }
                freeAudioPlayerData(&audiostate.playerdata.data[i]);
            }
        }
    }
}

void updateAudio_unlocked(float framemult) {
    (void)framemult;
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) goto ret;
    updateAudioPlayerData();
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
        #if SDL_VERSION_ATLEAST(2, 0, 4)
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
    #if PSRC_MTLVL >= 2
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
        plog(LL_INFO | LF_DEBUG, "Mixer is behind!");
        #endif
        memset(stream, 0, len);
    }
}

bool startAudio(void) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    char* tmp = cfg_getvar(&config, "Audio", "disable");
    if (tmp) {
        bool disable = strbool(tmp, false);
        free(tmp);
        if (disable) {
            audiostate.valid = false;
            #if PSRC_MTLVL >= 2
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
    #if SDL_VERSION_ATLEAST(2, 0, 4)
    #if PSRC_MTLVL >= 2
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
        inspec.samples = inspec.freq * 4 / 11025 * 48;
        if (inspec.samples < 1) inspec.samples = 1;
        register unsigned samples = inspec.samples - 1;
        samples |= samples >> 1;
        samples |= samples >> 2;
        samples |= samples >> 4;
        samples |= samples >> 8;
        samples |= samples >> 16;
        inspec.samples = samples + 1;
        #if !defined(PSRC_USESDL1) && (PLATFLAGS & PLATFLAG_UNIXLIKE) && defined(SDL_AUDIO_ALLOW_SAMPLES_CHANGE)
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
        #if PSRC_MTLVL >= 2
        releaseWriteAccess(&audiostate.lock);
        #endif
        return true;
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
        audiostate.vol = roundf(atof(tmp) * 32768.0);
        free(tmp);
    } else {
        audiostate.vol = 32768;
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
    audiostate.envbuf[0] = malloc(outspec.samples * sizeof(**audiostate.envbuf));
    audiostate.envbuf[1] = malloc(outspec.samples * sizeof(**audiostate.envbuf));
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
    VLB_INIT(audiostate.playerdata, 1, VLB_OOM_NOP);
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
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&playerdata.lock);
    #endif
    updateAudioPlayerData();
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&playerdata.lock);
    #endif
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
    return true;
}

void stopAudio(void) {
    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&audiostate.lock);
    #endif
    if (!audiostate.valid) goto ret;
    for (size_t i = 0; i < audiostate.sounds3d.len; ++i) {
        if (audiostate.sounds3d.data[i].prio != AUDIOPRIO_INVALID) deleteSound(&audiostate.sounds3d.data[i]);
    }
    for (size_t i = 0; i < audiostate.sounds2d.len; ++i) {
        if (audiostate.sounds2d.data[i].prio != AUDIOPRIO_INVALID) deleteSound(&audiostate.sounds2d.data[i]);
    }
    for (uint32_t i = 0; i < audiostate.playerdata.len; ++i) {
        struct audioplayerdata* pl = &audiostate.playerdata.data[i];
        if (pl->valid) freeAudioPlayerData(pl);
    }
    audiostate.emitters3d.len = 0;
    VLB_FREE(audiostate.emitters3d);
    audiostate.sounds3d.len = 0;
    VLB_FREE(audiostate.sounds3d);
    audiostate.sounds3dorder.len = 0;
    VLB_FREE(audiostate.sounds3dorder);
    audiostate.emitters2d.len = 0;
    VLB_FREE(audiostate.emitters2d);
    audiostate.sounds2d.len = 0;
    VLB_FREE(audiostate.sounds2d);
    audiostate.sounds2dorder.len = 0;
    VLB_FREE(audiostate.sounds2dorder);
    free(audiostate.fxbuf[0]);
    free(audiostate.fxbuf[1]);
    free(audiostate.envbuf[0]);
    free(audiostate.envbuf[1]);
    free(audiostate.mixbuf[0]);
    free(audiostate.mixbuf[1]);
    free(audiostate.outbuf[0]);
    if (audiostate.usecallback) free(audiostate.outbuf[1]);
    audiostate.valid = false;
    ret:;
    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&audiostate.lock);
    #endif
}

bool restartAudio(void) {
    stopAudio();
    return startAudio();
}

bool initAudio(void) {
    #if PSRC_MTLVL >= 2
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
    #if PSRC_MTLVL >= 2
    destroyAccessLock(&audiostate.lock);
    #endif
}
