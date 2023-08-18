#ifndef ENGINE_AUDIO_H
#define ENGINE_AUDIO_H

#include "../platform.h"

#include "../psrc_game/resource.h"
#include "../psrc_aux/threading.h"

#include "../stb/stb_vorbis.h"

#if PLATFORM != PLAT_XBOX
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

#include <stdint.h>
#include <stdbool.h>

struct __attribute__((packed)) audiosound_vorbisbuf {
    int off;
    int16_t len;
    int16_t* data[2];
};
struct __attribute__((packed)) audiosound_fx {
    int posoff; // position offset in milliseconds (based on the distance between campos and pos)
    uint16_t speedmul; // position multiplier in units of 256 (based on speed)
    int volmul[2]; // volume multiplier in units of 65536 (based on vol and the distance between campos and pos)
};
struct __attribute__((packed)) audiosound {
    uint64_t id;
    struct rc_sound* rc;
    stb_vorbis* vorbis;
    struct audiosound_vorbisbuf vorbisbuf;
    int ptr;
    uint8_t done : 1;
    uint8_t paused : 1;
    uint8_t uninterruptible : 1;
    uint8_t forcemono : 1;
    uint8_t poseffect : 1;
    float vol;
    float speed;
    float pos[3];
    struct audiosound_fx computed[2];
};

struct audiostate {
    mutex_t lock;
    bool valid;
    int freq;
    int channels;
    struct {
        int len;
        int* data[2];
    } audbuf;
    int sounds;
    struct audiosound* sounddata;
    int soundptr;
    uint64_t lastid;
    float campos[3]; // for position effect
};

enum audioopt {
    AUDIOOPT_END,
    AUDIOOPT_CAMPOS,
};

bool initAudio(struct audiostate*);
bool startAudio(struct audiostate*);
void updateAudioConfig(struct audiostate*, ...);
void lockAudioConfig(struct audiostate*);
void unlockAudioConfig(struct audiostate*);
void stopAudio(struct audiostate*);
bool restartAudio(struct audiostate*);
void termAudio(struct audiostate*);

#endif
