#ifndef ENGINE_AUDIO_H
#define ENGINE_AUDIO_H

#include "../platform.h"

#if PLATFORM != PLAT_XBOX
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

#include <stdbool.h>

struct audiostate {
    bool valid;
    SDL_AudioDeviceID output;
    SDL_AudioSpec outspec;
};

bool initAudio(struct audiostate*);
bool startAudio(struct audiostate*);

#endif
