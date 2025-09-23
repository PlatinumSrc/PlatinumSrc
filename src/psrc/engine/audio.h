#ifndef PSRC_ENGINE_AUDIO_H
#define PSRC_ENGINE_AUDIO_H

#include "../attribs.h"
#include "../resource.h"
#include "../threading.h"
#include "../platform.h"
#include "../vlb.h"

#include "../common/world.h"

#ifdef PSRC_USESTBVORBIS
    #include "../../stb/stb_vorbis.h"
#endif
#ifdef PSRC_USEMINIMP3
    #include "../../minimp3/minimp3_ex.h"
#endif

#include "../incsdl.h"

#include <stdint.h>
#include <stdbool.h>

// writes a pointer to a buffer of interleaved samples to *bufptr
// when done, the callback will be called with bufptr being NULL
typedef void (*audiocb)(void* ctx, long loop, long pos, long* start, long* end, int16_t** bufptr);

enum audioopt {
    AUDIOOPT_END,
    AUDIOOPT_VOL,       // double
    AUDIOOPT_FREQ,      // unsigned
    AUDIOOPT_CHANNELS,  // unsigned
    AUDIOOPT_3DVOICES,  // unsigned
    AUDIOOPT_2DVOICES   // unsigned
};

#define AUDIOPRIO_DEFAULT (-128)
#define AUDIOPRIO_INVALID (-128)
#define AUDIOPRIO_MIN (-127)
#define AUDIOPRIO_NORMAL (0)
#define AUDIOPRIO_MAX (127)

#define AUDIOFXMASK_TOFF (1U << 0)
#define AUDIOFXMASK_SPEED (1U << 1)
#define AUDIOFXMASK_VOL_L (1U << 2)
#define AUDIOFXMASK_VOL_R (1U << 3)
#define AUDIOFXMASK_VOL (AUDIOFXMASK_VOL_L | AUDIOFXMASK_VOL_R)
#define AUDIOFXMASK_LPFILT_L (1U << 4)
#define AUDIOFXMASK_LPFILT_R (1U << 5)
#define AUDIOFXMASK_LPFILT (AUDIOFXMASK_LPFILT_L | AUDIOFXMASK_LPFILT_R)
#define AUDIOFXMASK_HPFILT_L (1U << 6)
#define AUDIOFXMASK_HPFILT_R (1U << 7)
#define AUDIOFXMASK_HPFILT (AUDIOFXMASK_HPFILT_L | AUDIOFXMASK_HPFILT_R)
#define AUDIOFXMASK_ALL (-1U)
struct audiofx {
    int64_t toff;
    float speed;
    float vol[2];
    float lpfilt[2];
    float hpfilt[2];
};

#define AUDIO3DFXMASK_POS (1U << 0)
#define AUDIO3DFXMASK_RANGE (1U << 1)
#define AUDIO3DFXMASK_RADIUS (1U << 2)
#define AUDIO3DFXMASK_VOLDAMP (1U << 3)
#define AUDIO3DFXMASK_FREQDAMP (1U << 4)
#define AUDIO3DFXMASK_NODOPPLER (1U << 5)
#define AUDIO3DFXMASK_RELPOS (1U << 6)
#define AUDIO3DFXMASK_RELROT (1U << 7)
#define AUDIO3DFXMASK_ALL (-1U)
struct audio3dfx {
    struct worldcoord pos;
    float range;
    float radius[3];
    float voldamp;
    float freqdamp;
    uint8_t nodoppler : 1;
    uint8_t relpos : 1;
    uint8_t relrot : 1;
};

struct audiocalcfx {
    long posoff; // position offset in output freq samples
    int speedmul; // speed mult in units of 256
    int volmul[2]; // volume mult in units of 32768
    unsigned lpfiltmul[2]; // from 0 to output freq
    unsigned hpfiltmul[2]; // from 0 to output freq
};

#define AUDIOEMITTER3DFLAG_PAUSED (1U << 0)
#define AUDIOEMITTER3DFLAG_NOENV (1U << 1)
struct audioemitter3d {
    unsigned player;
    struct audiofx fx;
    struct audio3dfx fx3d;
    struct {
        long posoff;
        float volmul[2];
        float lpfiltmul[2];
        float hpfiltmul[2];
    } fx3dout;
    unsigned cursounds;
    unsigned maxsounds;
    int8_t prio;
    uint8_t flags;
    uint8_t fxch;
    uint8_t fxchimm;
    float dist;
};

#define AUDIOEMITTER2DFLAG_PAUSED (1U << 0)
#define AUDIOEMITTER2DFLAG_APPLYENV (1U << 1)
struct audioemitter2d {
    unsigned player;
    struct audiofx fx;
    unsigned cursounds;
    unsigned maxsounds;
    int8_t prio;
    uint8_t flags;
    uint8_t fxch;
    uint8_t fxchimm;
};

#define SOUNDFLAG_LOOP (1U << 0)
#define SOUNDFLAG_WRAP (1U << 1)
#define SOUNDIFLAG_USESCB (1U << 0)
#define SOUNDIFLAG_NEEDSINIT (1U << 1)
struct audiosound {
    int emitter;
    long loop;
    long pos;
    long frac;
    int8_t prio;
    uint8_t flags;
    uint8_t iflags;
    uint8_t fxch;
    uint8_t fxi;
    struct audiofx fx;
    struct audiocalcfx calcfx[2];
    int lplastout[2];
    int hplastout[2];
    int hplastin[2];
    union {
        struct {
            struct rc_sound* rc;
            union {
                #ifdef PSRC_USESTBVORBIS
                struct {
                    stb_vorbis* state;
                    int16_t* decbuf; // interleaved
                    long decbufhead;
                    long decbuflen;
                } vorbis;
                #endif
                #ifdef PSRC_USEMINIMP3
                struct {
                    mp3dec_ex_t* state;
                    int16_t* decbuf; // interleaved
                    long decbufhead;
                    long decbuflen;
                } mp3;
                #endif
                struct {
                    int16_t* cvtbuf; // interleaved
                    //long cvtbufhead;
                    //long cvtbuflen;
                } wav;
            };
        };
        struct {
            audiocb cb;
            void* ctx;
            long len;
            unsigned freq;
            unsigned ch;
        } cb;
    };
};

#define AUDIOENVMASK_PANNING (1U << 0)
#define AUDIOENVMASK_LPFILT (1U << 1)
#define AUDIOENVMASK_HPFILT (1U << 2)
#define AUDIOENVMASK_REVERB_DELAY (1U << 3)
#define AUDIOENVMASK_REVERB_MIX (1U << 4)
#define AUDIOENVMASK_REVERB_FEEDBACK (1U << 5)
#define AUDIOENVMASK_REVERB_MERGE (1U << 6)
#define AUDIOENVMASK_REVERB_LPFILT (1U << 7)
#define AUDIOENVMASK_REVERB_HPFILT (1U << 8)
#define AUDIOENVMASK_REVERB (AUDIOENVMASK_REVERB_DELAY | AUDIOENVMASK_REVERB_FEEDBACK | AUDIOENVMASK_REVERB_MIX | \
                            AUDIOENVMASK_REVERB_MERGE | AUDIOENVMASK_REVERB_LPFILT | AUDIOENVMASK_REVERB_HPFILT)
#define AUDIOENVMASK_ALL (-1U)
struct audioenv {
    float panning;
    float lpfilt;
    float hpfilt;
    struct {
        float delay;
        float mix;
        float feedback;
        float merge;
        float lpfilt;
        float hpfilt;
    } reverb;
};

struct audioreverbstate {
    int16_t* buf[2];
    unsigned len;
    unsigned size;
    unsigned head;
    unsigned tail;
    int lplastout[2];
    int hplastout[2];
    int hplastin[2];
    uint8_t parami;
    int mix[2];
    int feedback[2];
    int merge[2];
    unsigned lpfilt[2];
    unsigned hpfilt[2];
    unsigned filtdiv;
};

struct audioenvstate {
    uint16_t envch;
    uint16_t envchimm;
    float panning;
    struct {
        float amount;
        uint8_t muli;
        unsigned mul[2];
        int lastout[2];
    } lpfilt;
    struct {
        float amount;
        uint8_t muli;
        unsigned mul[2];
        int lastout[2];
        int lastin[2];
    } hpfilt;
    struct {
        float delay;
        float mix;
        float feedback;
        float merge;
        float lpfilt;
        float hpfilt;
        struct audioreverbstate state;
    } reverb;
};

struct audioplayerdata {
    bool valid;
    struct worldcoord pos;
    float rotsin[3];
    float rotcos[3];
    struct audioenvstate env;
};

extern struct audiostate {
    #ifndef PSRC_NOMT
    struct accesslock lock;
    #endif
    volatile bool valid;
    bool usecallback;
    #ifndef PSRC_USESDL1
    SDL_AudioDeviceID output;
    #endif
    unsigned vol;
    unsigned freq;
    unsigned channels;
    struct rcopt_sound soundrcopt;
    float soundspeedmul;
    unsigned buflen;
    int* fxbuf[2]; // l/r
    int* envbuf[2]; // l/r
    int* mixbuf[2]; // l/r
    unsigned decbuflen;
    unsigned outsize;
    unsigned outqueue;
    int16_t* outbuf[2]; // old/new, interleaved
    unsigned outbufi;
    unsigned mixoutbufi;
    unsigned max3dsounds;
    unsigned voices3d;
    struct VLB(struct audioemitter3d) emitters3d;
    struct VLB(struct audiosound) sounds3d;
    struct VLB(size_t) sounds3dorder;
    unsigned max2dsounds;
    unsigned voices2d;
    struct VLB(struct audioemitter2d) emitters2d;
    struct VLB(struct audiosound) sounds2d;
    struct VLB(size_t) sounds2dorder;
    struct {
        struct audioplayerdata* data;
        unsigned len;
        unsigned size;
    } playerdata;
} audiostate;

bool initAudio(void);
bool startAudio(void);
void updateAudioConfig(enum audioopt, ...);
void updateAudio(float framemult); // assumes playerdata has already been locked by the caller
void stopAudio(void);
bool restartAudio(void);
void quitAudio(bool quick);

int new3DAudioEmitter(unsigned pl, int8_t prio, unsigned maxsounds, unsigned flags, unsigned fxmask, const struct audiofx*, unsigned fx3dmask, const struct audio3dfx*);
void edit3DAudioEmitter(int, unsigned fenable, unsigned fdisable, unsigned fxmask, const struct audiofx*, unsigned fx3dmask, const struct audio3dfx*, unsigned immfxmask);
void stop3DAudioEmitter(int);
void delete3DAudioEmitter(int);
bool play3DSound(int e, struct rc_sound* rc, int8_t prio, uint8_t flags, unsigned fxmask, const struct audiofx*);
//bool play3DSoundCB(...);

int new2DAudioEmitter(unsigned pl, int8_t prio, unsigned maxsounds, unsigned flags, unsigned fxmask, const struct audiofx*);
void edit2DAudioEmitter(int, unsigned fenable, unsigned fdisable, unsigned fxmask, const struct audiofx*, unsigned immfxmask);
void stop2DAudioEmitter(int);
void delete2DAudioEmitter(int);
bool play2DSound(int e, struct rc_sound* rc, int8_t prio, uint8_t flags, unsigned fxmask, const struct audiofx*);
//bool play2DSoundCB(...);

void setAudioEnv(unsigned pl, unsigned mask, struct audioenv*, unsigned immmask);

void setMusic(struct rc_sound* rc); // TODO: make rc_music once PTM is added
void setMusicStyle(const char*);

#endif
