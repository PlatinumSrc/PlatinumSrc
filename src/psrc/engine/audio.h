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
#elif PLATFORM == PLAT_DREAMCAST
    #include <SDL/SDL.h>
#else
    #include <SDL2/SDL.h>
#endif

#include <stdint.h>
#include <stdbool.h>

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
    int posoff; // position offset in source freq samples (based on the dist between campos and pos)
    int speedmul; // position mult in units of 1000 (based on speed)
    int volmul[2]; // volume mult in units of 65536 (based on vol, camrot, and the dist between campos and pos)
};
struct audiosound {
    int64_t id;
    struct rc_sound* rc;
    union {
        stb_vorbis* vorbis;
        #ifdef PSRC_USEMINIMP3
        mp3dec_ex_t* mp3;
        #endif
    };
    struct audiosound_audbuf audbuf;
    struct __attribute__((packed)) {
        int64_t offset; // amount of samples passed in output sample rate
        uint8_t flags;
        struct {
            uint8_t paused : 1;
            uint8_t fxchanged : 1;
            //uint8_t updatefx : 1;
        } state;
        float vol[2];
        float speed;
        float pos[3];
        float range;
        struct audiosound_fx fx[2];
    };
};

struct audiostate {
    #ifndef PSRC_NOMT
    struct accesslock lock;
    #endif
    volatile bool valid;
    bool usecallback;
    SDL_AudioDeviceID output;
    int freq;
    int channels;
    volatile int8_t audbufindex;
    volatile int8_t mixaudbufindex;
    uint64_t buftime;
    int audbuflen;
    unsigned outbufcount;
    struct rcopt_sound soundrcopt;
    struct {
        int16_t* out[2];
        unsigned outsize;
        int* data[2][2];
        int len;
    } audbuf;
    int voicecount;
    struct audiosound* voices;
    int64_t nextid;
    float campos[3]; // for position effect
    float camrot[3];
};

extern struct audiostate audiostate;

enum audioopt {
    AUDIOOPT_END,
    AUDIOOPT_CAMPOS, // float, float, float
    AUDIOOPT_CAMROT, // float, float, float
    AUDIOOPT_FREQ, // int
    AUDIOOPT_VOICES, // int
};

bool initAudio(void);
bool startAudio(void);
void updateAudioConfig(void);
void stopAudio(void);
bool restartAudio(void);
void quitAudio(void);

#define SOUNDFLAG_UNINTERRUPTIBLE (1 << 0)
#define SOUNDFLAG_LOOP (1 << 1)
#define SOUNDFLAG_WRAP (1 << 2)
#define SOUNDFLAG_FORCEMONO (1 << 3)
#define SOUNDFLAG_POSEFFECT (1 << 4)
#define SOUNDFLAG_RELPOS (1 << 5)
#define SOUNDFLAG_NODOPPLER (1 << 6)

enum soundfx {
    SOUNDFX_END,
    SOUNDFX_VOL, // float, float
    SOUNDFX_SPEED, // float
    SOUNDFX_POS, // float, float, float
    SOUNDFX_RANGE, // float
};

int64_t playSound(bool paused, struct rc_sound* rc, unsigned flags, ... /*soundfx*/);
void changeSoundFX(int64_t, int /*(bool)*/ immediate, ...);
void changeSoundFlags(int64_t, unsigned disable, unsigned enable);
void stopSound(int64_t);
void pauseSound(int64_t, bool);
void updateSounds(void);

#endif
