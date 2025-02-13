#ifndef PSRC_ENGINE_AUDIO_H
#define PSRC_ENGINE_AUDIO_H

#include "../attribs.h"
#include "../resource.h"
#include "../threading.h"
#include "../platform.h"
#include "../vlb.h"

#ifdef PSRC_USESTBVORBIS
    #include "../../stb/stb_vorbis.h"
#endif
#ifdef PSRC_USEMINIMP3
    #include "../../minimp3/minimp3_ex.h"
#endif

#if PLATFORM == PLAT_NXDK || PLATFORM == PLAT_GDK
    #include <SDL.h>
#elif defined(PSRC_USESDL1)
    #include <SDL/SDL.h>
#else
    #include <SDL2/SDL.h>
#endif

#include <stdint.h>
#include <stdbool.h>

// writes a pointer to a buffer of interleaved samples to *bufptr
// when done, the callback will be called with bufptr being NULL
typedef void (*audiocb)(void* ctx, long loop, long pos, long* start, long* end, int16_t** bufptr);

enum audioopt {
    AUDIOOPT_END,
    AUDIOOPT_FREQ,      // unsigned
    AUDIOOPT_3DVOICES,  // unsigned
    AUDIOOPT_2DVOICES   // unsigned
};

#define AUDIOPRIO_DEFAULT (-128)
#define AUDIOPRIO_MIN (-127)
#define AUDIOPRIO_NORMAL (0)
#define AUDIOPRIO_MAX (127)

#define AUDIOFXMASK_VOL_L (1 << 0)
#define AUDIOFXMASK_VOL_R (1 << 1)
#define AUDIOFXMASK_VOL (AUDIOFXMASK_VOL_L | AUDIOFXMASK_VOL_R)
#define AUDIOFXMASK_SPEED (1 << 2)
#define AUDIOFXMASK_LPFILT (1 << 3)
#define AUDIOFXMASK_HPFILT (1 << 4)
#define AUDIOFXMASK_ALL (-1U)
struct audiofx {
    float vol[2];
    float speed; 
    float lpfilt;
    float hpfilt;
};

#define AUDIO3DFXMASK_POS (1 << 0)
#define AUDIO3DFXMASK_SIZE (1 << 1)
#define AUDIO3DFXMASK_VOLDAMP (1 << 2)
#define AUDIO3DFXMASK_FREQDAMP (1 << 3)
#define AUDIO3DFXMASK_NODOPPLER (1 << 4)
#define AUDIO3DFXMASK_RELPOS (1 << 5)
#define AUDIO3DFXMASK_RELROT (1 << 6)
#define AUDIO3DFXMASK_ALL (-1U)
struct audio3dfx {
    float pos[3];
    float size[3];
    float voldamp;
    float freqdamp;
    uint8_t nodoppler : 1;
    uint8_t relpos : 1;
    uint8_t relrot : 1;
};

#define AUDIOENVMASK_LPFILT (1 << 0)
#define AUDIOENVMASK_HPFILT (1 << 1)
#define AUDIOENVMASK_REVERB_DELAY (1 << 2)
#define AUDIOENVMASK_REVERB_FEEDBACK (1 << 3)
#define AUDIOENVMASK_REVERB_MIX (1 << 4)
#define AUDIOENVMASK_REVERB_LPFILT (1 << 5)
#define AUDIOENVMASK_REVERB_HPFILT (1 << 6)
#define AUDIOENVMASK_REVERB (AUDIOENVMASK_REVERB_DELAY | AUDIOENVMASK_REVERB_FEEDBACK | AUDIOENVMASK_REVERB_MIX | \
                            AUDIOENVMASK_REVERB_LPFILT | AUDIOENVMASK_REVERB_HPFILT)
#define AUDIOENVMASK_ALL (-1U)
struct audioenv {
    float lpfilt;
    float hpfilt;
    struct {
        float delay;
        float feedback;
        float mix;
        float lpfilt;
        float hpfilt;
    } reverb;
};

struct audioemitter3d {
    struct audiofx fx;
    struct audio3dfx fx3d;
    int8_t prio;
};

struct audioemitter2d {
    struct audiofx fx;
    int8_t prio;
};

#define SOUNDFLAG_LOOP (1 << 0)
#define SOUNDFLAG_WRAP (1 << 1)

#define SOUNDIFLAG_USESCB (1 << 0)
#define SOUNDIFLAG_FXCH_VOL (1 << 1)
#define SOUNDIFLAG_FXCH_FILT (1 << 2)
#define SOUNDIFLAG_FXCH_OTHER (1 << 3)
#define SOUNDIFLAG_FXCH (SOUNDIFLAG_FXCH_VOL | SOUNDIFLAG_FXCH_FILT | SOUNDIFLAG_FXCH_OTHER)

struct audiosound_fx {
    long posoff; // position offset in output freq samples (based on the dist between campos and pos)
    int speedmul; // speed mult in units of 256 (based on speed)
    int volmul[2]; // volume mult in units of 32768 (based on vol, camrot, and the dist between campos and pos)
    unsigned lpfiltmul; // from 0 to output freq
    unsigned hpfiltmul; // from 0 to output freq
};
struct audiosound {
    int emitter;
    long loop;
    long pos;
    long frac;
    int8_t prio;
    uint8_t flags;
    uint8_t iflags;
    uint8_t fxi;
    struct audiofx fx;
    struct audiosound_fx calcfx[2];
    int16_t lplastout[2];
    int16_t hplastout[2];
    int16_t hplastin[2];
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

struct audiostate {
    #ifndef PSRC_NOMT
    struct accesslock lock;
    #endif
    volatile bool valid;
    bool usecallback;
    #ifndef PSRC_USESDL1
    SDL_AudioDeviceID output;
    #endif
    unsigned freq;
    unsigned channels;
    struct rcopt_sound soundrcopt;
    unsigned buflen;
    int* fxbuf[2]; // l/r
    int* mixbuf[2]; // l/r
    int16_t* outbuf[2]; // old/new, interleaved
    unsigned outbufi;
    unsigned mixoutbufi;
    unsigned decbuflen;
    unsigned outsize;
    unsigned outqueue;
    unsigned max3dsounds;
    unsigned voices3d;
    struct VLB(struct audioemitter3d) emitters3d;
    struct VLB(struct audiosound) sounds3d;
    unsigned max2dsounds;
    unsigned voices2d;
    struct VLB(struct audioemitter2d) emitters2d;
    struct VLB(struct audiosound) sounds2d;
    struct {
        float pos[3];
        float rot[3];
        float rotradx, rotrady, rotradz;
        float sinx, cosx;
        float siny, cosy;
        float sinz, cosz;
    } cam;
};

extern struct audiostate audiostate;

bool initAudio(void);
bool startAudio(void);
void updateAudioConfig(enum audioopt, ...);
void updateAudio(float framemult);
void stopAudio(void);
bool restartAudio(void);
void quitAudio(bool quick);

int new3DAudioEmitter(int8_t prio, unsigned max, unsigned fxmask, const struct audiofx*, unsigned fx3dmask, const struct audio3dfx*);
void edit3DAudioEmitter(int, unsigned fxmask, const struct audiofx*, unsigned fx3dmask, const struct audio3dfx*);
void pause3DAudioEmitter(int, bool);
void stop3DAudioEmitter(int);
void delete3DAudioEmitter(int);
bool play3DSound(int e, struct rc_sound* rc, int8_t prio, uint8_t flags, int16_t toff, unsigned fxmask, const struct audiofx*);
//bool play3DSoundCB(...);

int new2DAudioEmitter(int8_t prio, unsigned max, unsigned fxmask, const struct audiofx*);
void edit2DAudioEmitter(int, unsigned fxmask, const struct audiofx*);
void pause2DAudioEmitter(int, bool);
void stop2DAudioEmitter(int);
void delete2DAudioEmitter(int);
bool play2DSound(int e, struct rc_sound* rc, int8_t prio, uint8_t flags, int16_t toff, unsigned fxmask, const struct audiofx*);
//bool play2DSoundCB(...);

void setAudioEnv(unsigned mask, struct audioenv*);

void setMusic(struct rc_sound* rc); // TODO: make rc_music once PTM is added
void setMusicStyle(const char*);

#endif
