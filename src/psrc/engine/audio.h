#ifndef PSRC_ENGINE_AUDIO_H
#define PSRC_ENGINE_AUDIO_H

#include "../platform.h"

#include "../game/resource.h"
#include "../aux/threading.h"

#include "../../stb/stb_vorbis.h"
#include "../../minimp3/minimp3_ex.h"

#include "../platform.h"

#if PLATFORM != PLAT_XBOX
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

#include <stdint.h>
#include <stdbool.h>

struct audiosound_audbuf {
    int off;
    int len;
    union {
        int16_t* data[2];
        mp3d_sample_t* data_mp3;
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
        mp3dec_ex_t* mp3;
    };
    struct audiosound_audbuf audbuf;
    struct __attribute__((packed)) {
        int64_t offset; // amount of samples passed in output sample rate
        uint8_t flags;
        struct {
            uint8_t paused : 1;
            uint8_t fxchanged : 1;
        } state;
        float vol[2];
        float speed;
        float pos[3];
        float range;
        struct audiosound_fx fx[2];
    };
};

struct audiostate {
    struct accesslock lock;
    volatile bool valid;
    thread_t mixthread;
    cond_t mixcond;
    SDL_AudioDeviceID output;
    int freq;
    int channels;
    volatile int audbufindex;
    volatile int mixaudbufindex;
    uint64_t buftime;
    int audbuflen;
    struct rcopt_sound soundrcopt;
    struct {
        int len;
        int* data[2][2];
    } audbuf;
    int voices;
    struct audiosound* voicedata;
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
void termAudio(void);

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

#endif
