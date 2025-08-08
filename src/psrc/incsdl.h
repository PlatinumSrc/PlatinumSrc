#ifndef PSRC_INCSDL_H
#define PSRC_INCSDL_H

#include "platform.h"

#if PLATFORM == PLAT_ANDROID || PLATFORM == PLAT_NXDK || PLATFORM == PLAT_GDK
    #include <SDL.h>
#elif defined(PSRC_USESDL1)
    #include <SDL/SDL.h>
#else
    #include <SDL2/SDL.h>
#endif

#endif
