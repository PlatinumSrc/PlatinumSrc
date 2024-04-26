#ifndef PSRC_ENGINE_AUDIO_H
#define PSRC_ENGINE_AUDIO_H

#include "../platform.h"

#include "../common/resource.h"
#include "../common/threading.h"

#include "../../stb/stb_vorbis.h"
#ifdef PSRC_USEMINIMP3
    #include "../../minimp3/minimp3_ex.h"
#endif

#include "../platform.h"

#if PLATFORM == PLAT_NXDK
    #include <SDL.h>
#elif defined(PSRC_USESDL1)
    #include <SDL/SDL.h>
#else
    #include <SDL2/SDL.h>
#endif

#include <stdint.h>
#include <stdbool.h>

struct audioemitter {
    int max;
    int uses;
    uint8_t flags;
    struct {
        uint8_t paused : 1;
    } state;
    float vol[2];
    float speed;
    float pos[3];
    float range;
};

struct audiosound_audbuf {
    int off;
    int len;
    union {
        int16_t* data[2];
        #ifdef PSRC_USEMINIMP3
        mp3d_sample_t* data_mp3;
        #endif
    };
};
struct __attribute__((packed)) audiosound_fx {
    int posoff; // position offset in output freq samples (based on the dist between campos and pos)
    int speedmul; // position mult in units of 1000 (based on speed)
    int volmul[2]; // volume mult in units of 65536 (based on vol, camrot, and the dist between campos and pos)
};
struct audiosound {
    struct rc_sound* rc;
    int emitter;
    union {
        stb_vorbis* vorbis;
        #ifdef PSRC_USEMINIMP3
        mp3dec_ex_t* mp3;
        #endif
    };
    uint8_t flags;
    struct {
        uint8_t fxchanged : 1;
    } state;
    struct audiosound_audbuf audbuf;
    long offset;
    int fxoff;
    int frac;
    float vol[2];
    float speed;
    float pos[3];
    struct audiosound_fx fx[2];
};

struct audiocachenode {
    struct audiocachenode* next;
    struct audiocachenode* prev;
    struct rc_sound* data;
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
    int freq;
    int channels;
    volatile int8_t audbufindex;
    volatile int8_t mixaudbufindex;
    int audbuflen;
    unsigned outbufcount;
    struct rcopt_sound soundrcopt;
    struct {
        int16_t* out[2];
        unsigned outsize;
        int* data[2][2];
        int len;
    } audbuf;
    struct {
        struct audioemitter* data;
        int len;
        int size;
    } emitters;
    struct {
        struct audiosound* data;
        int count;
        int next;
    } voices;
    struct {
        struct audiocachenode* data;
        struct audiocachenode* head;
        struct audiocachenode* tail;
        int size;
    } cache;
    float campos[3]; // for position effect
    float camrot[3];
    float rotradx, rotrady, rotradz;
    float sinx, cosx;
    float siny, cosy;
    float sinz, cosz;
};

extern struct audiostate audiostate;

enum audioopt {
    AUDIOOPT_END,
    AUDIOOPT_FREQ, // int
    AUDIOOPT_VOICES, // int
};

bool initAudio(void);
bool startAudio(void);
void updateAudioConfig(enum audioopt, ...);
void stopAudio(void);
bool restartAudio(void);
void quitAudio(void);

#define EMITTERFLAG_FORCEMONO (1 << 0)
#define EMITTERFLAG_POSEFFECT (1 << 1)
#define EMITTERFLAG_RELPOS (1 << 2)
#define EMITTERFLAG_NODOPPLER (1 << 3)

#define SOUNDFLAG_UNINTERRUPTIBLE (1 << 0)
#define SOUNDFLAG_LOOP (1 << 1)
#define SOUNDFLAG_WRAP (1 << 2)

enum soundfx {
    SOUNDFX_END,
    SOUNDFX_VOL, // float, float
    SOUNDFX_SPEED, // float
    SOUNDFX_POS, // float, float, float
    SOUNDFX_RANGE, // float
};

int newAudioEmitter(int max, uint8_t flags, ... /*soundfx*/);
void editAudioEmitter(int, bool immediate, unsigned enable, unsigned disable, ... /*soundfx*/);
void pauseAudioEmitter(int, bool);
void stopAudioEmitter(int);
void deleteAudioEmitter(int);
void playSound(int emitter, struct rc_sound* rc, uint8_t flags, ... /*soundfx*/);
void updateSounds(void);

#endif
