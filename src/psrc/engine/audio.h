#ifndef PSRC_ENGINE_AUDIO_H
#define PSRC_ENGINE_AUDIO_H

#include "../platform.h"

#include "../resource.h"
#include "../threading.h"

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
    uint8_t paused : 1;
    uint8_t bg : 1;
    float vol[2];
    float speed;
    float pos[3];
    float range;
    float lpfilt;
    float hpfilt;
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
struct audiosound_fx {
    int posoff; // position offset in output freq samples (based on the dist between campos and pos)
    int speedmul; // speed mult in units of 32 (based on speed)
    int volmul[3]; // volume mult in units of 32768 (based on vol, camrot, and the dist between campos and pos)
    int lpfiltmul; // from 0 to output freq
    int hpfiltmul; // from 0 to output freq
};
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
    uint8_t flags : 7;
    uint8_t fxchanged : 1;
    int fxoff;
    float vol[2];
    float speed;
    float pos[3];
    float range;
    float lpfilt;
    float hpfilt;
    struct audiosound_fx fx[2];
    int maxvol;
    int16_t lplastout;
    int16_t hplastout;
    int16_t hplastin;
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
        uint8_t filterchanged : 1;
        uint8_t reverbchanged : 1;
        struct {
            float amount[2];
            int lastout[2];
        } lpfilt;
        struct {
            float amount[2];
            int lastout[2];
            int lastin[2];
        } hpfilt;
        struct {
            float delay[2];
            float feedback[2];
            float mix[2];
            int16_t* buf[2];
            unsigned len;
            unsigned size;
            unsigned head;
            struct {
                float amount[2];
                int16_t lastout[2];
            } lpfilt;
            struct {
                float amount[2];
                int16_t lastout[2];
                int16_t lastin[2];
            } hpfilt;
        } reverb;
    } env;
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
    SOUNDFXENUM__END,
    SOUNDFXENUM_VOL, // double, double
    SOUNDFXENUM_SPEED, // double
    SOUNDFXENUM_POS, // double, double, double
    SOUNDFXENUM_RANGE, // double
    SOUNDFXENUM_LPFILT, // double
    SOUNDFXENUM_HPFILT // double
};
#define SOUNDFX_VOL(l, r) SOUNDFXENUM_VOL, (double)(l), (double)(r)
#define SOUNDFX_SPEED(m) SOUNDFXENUM_SPEED, (double)(m)
#define SOUNDFX_POS(x, y, z) SOUNDFXENUM_POS, (double)(x), (double)(y), (double)(z)
#define SOUNDFX_RANGE(a) SOUNDFXENUM_RANGE, (double)(a)
#define SOUNDFX_LPFILT(a) SOUNDFXENUM_LPFILT, (double)(a)
#define SOUNDFX_HPFILT(a) SOUNDFXENUM_HPFILT, (double)(a)

enum soundenv {
    SOUNDENVENUM__END,
    SOUNDENVENUM_LPFILT, // double
    SOUNDENVENUM_HPFILT, // double
    SOUNDENVENUM_REVERB_DELAY, // double
    SOUNDENVENUM_REVERB_FEEDBACK, // double
    SOUNDENVENUM_REVERB_MIX, // double
    SOUNDENVENUM_REVERB_LPFILT, // double
    SOUNDENVENUM_REVERB_HPFILT, // double
};
#define SOUNDENV_LPFILT(a) SOUNDENVENUM_LPFILT, (double)(a)
#define SOUNDENV_HPFILT(a) SOUNDENVENUM_HPFILT, (double)(a)
#define SOUNDENV_REVERB(d, f, m, lp, hp) SOUNDENVENUM_REVERB_DELAY, (double)(d), SOUNDENVENUM_REVERB_FEEDBACK, (double)(f),\
    SOUNDENVENUM_REVERB_MIX, (double)(m), SOUNDENVENUM_REVERB_LPFILT, (double)(lp), SOUNDENVENUM_REVERB_HPFILT, (double)(hp)
#define SOUNDENV_REVERB_DELAY(d) SOUNDENVENUM_REVERB_DELAY, (double)(d)
#define SOUNDENV_REVERB_FEEDBACK(f) SOUNDENVENUM_REVERB_FEEDBACK, (double)(f)
#define SOUNDENV_REVERB_MIX(m) SOUNDENVENUM_REVERB_MIX, (double)(m)
#define SOUNDENV_REVERB_LPFILT(lp) SOUNDENVENUM_REVERB_LPFILT, (double)(lp)
#define SOUNDENV_REVERB_HPFILT(hp) SOUNDENVENUM_REVERB_HPFILT, (double)(hp)

int newAudioEmitter(int max, unsigned /*bool*/ bg, ... /*soundfx*/);
#define newAudioEmitter(...) newAudioEmitter(__VA_ARGS__, SOUNDFXENUM__END)
void editAudioEmitter(int, unsigned /*bool*/ immediate, ... /*soundfx*/);
#define editAudioEmitter(...) editAudioEmitter(__VA_ARGS__, SOUNDFXENUM__END)
void pauseAudioEmitter(int, bool);
void stopAudioEmitter(int);
void deleteAudioEmitter(int);

void playSound(int emitter, struct rc_sound* rc, unsigned /*uint8_t*/ flags, ... /*soundfx*/);
#define playSound(...) playSound(__VA_ARGS__, SOUNDFXENUM__END)
void playUISound(struct rc_sound* rc);
void playAlertSound(struct rc_sound* rc); // TODO: maybe add speed?
void setAmbientSound(struct rc_sound* rc);
void setMusic(struct rc_sound* rc); // TODO: make rc_music once PTM is added
void setMusicStyle(const char*);

void editSoundEnv(enum soundenv, ...);
#define editSoundEnv(...) editSoundEnv(__VA_ARGS__, SOUNDENVENUM__END)

void updateSounds(float framemult);

#endif
