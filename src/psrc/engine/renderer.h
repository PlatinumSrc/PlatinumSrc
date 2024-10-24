#ifndef PSRC_ENGINE_RENDERER_H
#define PSRC_ENGINE_RENDERER_H

#include "../platform.h"
#include "../debug.h"

#if DEBUG(1)
    #include "../common/profiling.h"
#endif
#include "../common/resource.h"

#if PLATFORM == PLAT_NXDK || PLATFORM == PLAT_GDK
    #include <SDL.h>
#elif defined(PSRC_USESDL1)
    #include <SDL/SDL.h>
#else
    #include <SDL2/SDL.h>
#endif

#include <stdbool.h>
#include <stdint.h>

enum rendapi {
    RENDAPI__INVALID = -1,

    #ifdef PSRC_ENGINE_RENDERER_USESR
    RENDAPI_SW,
    #endif

    #ifdef PSRC_ENGINE_RENDERER_USEGL
    #ifdef PSRC_ENGINE_RENDERER_GL_USEGL11
    RENDAPI_GL11,
    #endif
    #ifdef PSRC_ENGINE_RENDERER_GL_USEGL33
    RENDAPI_GL33,
    #endif
    #ifdef PSRC_ENGINE_RENDERER_GL_USEGLES30
    RENDAPI_GLES30,
    #endif
    #endif

    #ifdef PSRC_ENGINE_RENDERER_USEXGU
    RENDAPI_XGU,
    #endif

    RENDAPI__COUNT
};

enum rendmode {
    RENDMODE_WINDOWED,
    RENDMODE_BORDERLESS,
    RENDMODE_FULLSCREEN,
};

struct rendres {
    int width, height;
};

enum rendlighting {
    RENDLIGHTING_LOW,
    RENDLIGHTING_MEDIUM,
    RENDLIGHTING_HIGH,
};

struct rendstate {
    #ifndef PSRC_USESDL1
    SDL_Window* window;
    #endif
    char* icon;
    enum rendapi api;
    enum rendmode mode;
    uint8_t vsync : 1;
    uint8_t borderless : 1;
    int fps;
    float fov;
    float aspect;
    float campos[3];
    float camrot[3];
    struct {
        struct rendres current;
        struct rendres windowed, fullscr;
    } res;
    #ifdef PSRC_USESDL1
    unsigned flags;
    uint8_t bpp;
    #endif
    enum rcopt_texture_qlt texqlt;
    enum rendlighting lighting;
    #if DEBUG(1)
    struct profile* dbgprof;
    #endif
};

extern struct rendstate rendstate;

enum rendopt {
    RENDOPT_END,
    RENDOPT_ICON, // char*
    RENDOPT_API, // enum rendapi
    RENDOPT_FULLSCREEN, // int
    RENDOPT_BORDERLESS, // bool
    RENDOPT_VSYNC, // bool
    RENDOPT_FOV, // float
    RENDOPT_RES, // struct rendres*
    RENDOPT_LIGHTING, // enum rendlighting
    RENDOPT_TEXTUREQLT, // enum rcopt_texture_qlt
};

bool initRenderer(void);
bool startRenderer(void);
bool updateRendererConfig(enum rendopt, ...);
void lockRendererConfig(void);
void unlockRendererConfig(void);
bool restartRenderer(void);
void stopRenderer(void);
void quitRenderer(void);
extern void (*render)(void);
extern void (*display)(void);
extern void* (*takeScreenshot)(int* w, int* h, int* sz);

extern const char* const* rendapi_names[RENDAPI__COUNT];

#endif
