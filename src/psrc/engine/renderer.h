#ifndef PSRC_ENGINE_RENDERER_H
#define PSRC_ENGINE_RENDERER_H

#include "../platform.h"
#include "../common/resource.h"

#include "../../cglm/cglm.h"

#if PLATFORM != PLAT_NXDK
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

#include <stdbool.h>
#include <stdint.h>

enum rendapi {
    RENDAPI__INVAL = -1,
    #ifndef PSRC_ENGINE_RENDERER_NOGL
    #ifndef PSRC_ENGINE_RENDERER_NOGL11
    RENDAPI_GL11,
    #endif
    #ifndef PSRC_ENGINE_RENDERER_NOGL33
    RENDAPI_GL33,
    #endif
    #ifndef PSRC_ENGINE_RENDERER_NOGLES30
    RENDAPI_GLES30,
    #endif
    #endif
    RENDAPI__COUNT,
};

enum rendapigroup {
    RENDAPIGROUP__INVAL = -1,
    #ifndef PSRC_ENGINE_RENDERER_NOGL
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
    union {
        #ifndef PSRC_ENGINE_RENDERER_NOGL
        struct {
            #if PLATFORM != PLAT_NXDK
            SDL_GLContext ctx;
            #endif
            uint8_t fastclear : 1;
            float nearplane;
            float farplane;
            mat4 projmat;
            mat4 viewmat;
            #if PLATFORM == PLAT_NXDK
            float scale;
            #endif
            union {
                #ifndef PSRC_ENGINE_RENDERER_NOGL11
                struct {
                } gl11;
                #endif
                #ifndef PSRC_ENGINE_RENDERER_NOGL33
                struct {
                } gl33;
                #endif
                #ifndef PSRC_ENGINE_RENDERER_NOGLES30
                struct {
                } gles;
                #endif
            };
        } gl;
        #endif
    };
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
void termRenderer(void);
void render(void);
void display(void);
void* takeScreenshot(int*, int*, int*);

extern const char* rendapi_ids[];
extern const char* rendapi_names[];

#endif
