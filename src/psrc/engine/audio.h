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

#if PLATFORM == PLAT_NXDK || PLATFORM == PLAT_GDK
    #include <SDL.h>
#elif defined(PSRC_USESDL1)
    #include <SDL/SDL.h>
#else
    #include <SDL2/SDL.h>
#endif

#include <stdint.h>
#include <stdbool.h>

#include "../attribs.h"

struct audioemitter {
    int max;
    int uses;
    uint8_t bg : 1;
    uint8_t paused : 1;
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
#pragma pack(push, 1)
struct audiosound_fx {
    int posoff; // position offset in output freq samples (based on the dist between campos and pos)
    int speedmul; // position mult in units of 256 (based on speed)
    int volmul[3]; // volume mult in units of 32768 (based on vol, camrot, and the dist between campos and pos)
};
#pragma pack(pop)
struct audiosound {
    struct rc_sound* rc;
    union {
        stb_vorbis* vorbis;
        #ifdef PSRC_USEMINIMP3
        mp3dec_ex_t* mp3;
        #endif
    };
    struct audiosound_audbuf audbuf;
    long offset;
    int frac;
};
struct audiosound_alert {
    struct audiosound data;
    int8_t prio;
};
struct audiosound_3d {
    struct audiosound data;
    int emitter;
    uint8_t fxchanged : 1;
    uint8_t flags : 7;
    int fxoff;
    float vol[2];
    float speed;
    float pos[3];
    float range;
    struct audiosound_fx fx[2];
    int maxvol;
};

struct audiovoicegroup_world {
    struct audiosound_3d* data;
    int* sortdata;
    int len;
    int size;
    int playcount;
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
        struct audiosound ui;
        struct {
            struct audiosound_alert* data;
            int count;
            int next;
        } alerts;
        struct audiovoicegroup_world world;
        struct audiovoicegroup_world worldbg;
        struct {
            struct rc_sound* queue;
            struct audiosound data[2];
            float fade;
            float oldfade;
            uint8_t index;
        } ambience;
        struct {
            struct rc_sound* queue;
            struct audiosound data;
            float vol;
            float oldvol;
        } music;
    } voices;
    struct {
        uint8_t master;
        uint8_t ui;
        uint8_t alerts;
        uint8_t world;
        uint8_t worldbg;
        uint8_t ambience;
        uint8_t music;
        uint8_t voice;
    } vol;
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

enum audioopt {
    AUDIOOPT_END,
    AUDIOOPT_FREQ, // int
    AUDIOOPT_VOICES // int
};

bool initAudio(void);
bool startAudio(void);
void updateAudioConfig(enum audioopt, ...);
void stopAudio(void);
bool restartAudio(void);
void quitAudio(void);

#define SOUNDFLAG_LOOP (1 << 0)
#define SOUNDFLAG_WRAP (1 << 1)
#define SOUNDFLAG_NODOPPLER (1 << 2)

enum soundfx {
    SOUNDFX_END,
    SOUNDFX_VOL, // double, double
    SOUNDFX_SPEED, // double
    SOUNDFX_POS, // double, double, double
    SOUNDFX_RANGE, // double
};

int newAudioEmitter(int max, bool bg, ... /*soundfx*/);
void editAudioEmitter(int, bool immediate, ... /*soundfx*/);
void pauseAudioEmitter(int, bool);
void stopAudioEmitter(int);
void deleteAudioEmitter(int);

void playSound(int emitter, struct rc_sound* rc, uint8_t flags, ... /*soundfx*/);
void playUISound(struct rc_sound* rc);
void playAlertSound(struct rc_sound* rc); // TODO: maybe add speed?
void setAmbientSound(struct rc_sound* rc);
void setMusic(struct rc_sound* rc); // TODO: make rc_music once PTM is added
void setMusicStyle(const char*);

void updateSounds(float framemult);

#endif
