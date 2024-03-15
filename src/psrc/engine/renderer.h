#ifndef PSRC_ENGINE_RENDERER_H
#define PSRC_ENGINE_RENDERER_H

#include "../platform.h"
#include "../common/resource.h"

#include "../../cglm/cglm.h"

#if PLATFORM == PLAT_NXDK
    #include <SDL.h>
#elif defined(PSRC_USESDL1)
    #include <SDL/SDL.h>
#else
    #include <SDL2/SDL.h>
#endif

#include <stdbool.h>
#include <stdint.h>

enum rendapi {
    RENDAPI__INVAL = -1,
    #ifdef PSRC_USEGL
    #ifdef PSRC_USEGL11
    RENDAPI_GL11,
    #endif
    #ifdef PSRC_USEGL33
    RENDAPI_GL33,
    #endif
    #ifdef PSRC_USEGLES30
    RENDAPI_GLES30,
    #endif
    #endif
    RENDAPI__COUNT,
};

enum rendapigroup {
    RENDAPIGROUP__INVAL = -1,
    #ifdef PSRC_USEGL
    RENDAPIGROUP_GL,
    #endif
    RENDAPIGROUP__COUNT,
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
    SDL_Window* window;
    char* icon;
    enum rendapi api;
    enum rendapigroup apigroup;
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
    enum rcopt_texture_qlt texqlt;
    enum rendlighting lighting;
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

extern const char* rendapi_ids[];
extern const char* rendapi_names[];

#endif
