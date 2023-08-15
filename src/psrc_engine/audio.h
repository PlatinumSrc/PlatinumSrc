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
    struct rc_sound* rc;
    int ptr;
    uint8_t playing : 1;
    uint8_t forcemono : 1;
    uint8_t uninterruptible : 1;
    float vol;
    float pos[3];
    uint16_t pvol[2]; // processed volume
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
    float campos[3]; // for position effect
};

bool initAudio(struct audiostate*);
bool startAudio(struct audiostate*);

#endif
