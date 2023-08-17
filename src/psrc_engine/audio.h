#ifndef ENGINE_AUDIO_H
#define ENGINE_AUDIO_H

#include "../platform.h"

#include "../psrc_game/resource.h"
#include "../psrc_aux/threading.h"

#if PLATFORM != PLAT_XBOX
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

#include <stdint.h>
#include <stdbool.h>

struct __attribute__((packed)) audiosound {
    uint64_t id;
    struct rc_sound* rc;
    int ptr;
    uint8_t done : 1;
    uint8_t playing : 1;
    uint8_t uninterruptible : 1;
    uint8_t forcemono : 1;
    uint8_t poseffect : 1;
    float vol;
    float speed;
    float pos[3];
    int posoff; // position offset in milliseconds (based on the distance between campos and pos)
    uint16_t speedmul; // position multiplier in units of 256 (based on speed)
    int volmul[2]; // volume multiplier in units of 65536 (based on vol and the distance between campos and pos)
};

struct audiostate {
    mutex_t lock;
    bool valid;
    int freq;
    int channels;
    int* audbuf[2];
    int16_t* audbuf16[2];
    int audbuflen;
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

#endif
