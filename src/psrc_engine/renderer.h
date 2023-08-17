#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include "../platform.h"
#include "../psrc_game/resource.h"

#include "../cglm/cglm.h"

#if PLATFORM != PLAT_XBOX
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

#include <stdbool.h>
#include <stdint.h>

enum rendapi {
    RENDAPI__INVAL = -1,
    RENDAPI_GL11,
    #if PLATFORM != PLAT_XBOX
    RENDAPI_GL33,
    RENDAPI_GLES30,
    #endif
    RENDAPI__COUNT,
};

enum rendapigroup {
    RENDAPIGROUP__INVAL = -1,
    RENDAPIGROUP_GL,
    RENDAPIGROUP__COUNT,
};

enum rendmode {
    RENDMODE_WINDOWED,
    RENDMODE_BORDERLESS,
    RENDMODE_FULLSCREEN,
};

struct rendres {
    int width, height, hz;
};

struct material {
    
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
    bool vsync;
    struct {
        struct rendres current;
        struct rendres windowed, fullscr;
    } res;
    enum rcopt_texture_qlt texqlt;
    enum rendlighting lighting;
    union {
        struct {
            bool init;
            #if PLATFORM != PLAT_XBOX
            SDL_GLContext ctx;
            #endif
            union {
                struct {
                    bool depthstate;
                } gl11;
                #if PLATFORM != PLAT_XBOX
                struct {
                } gl33;
                struct {
                } gles;
                #endif
            };
        } gl;
    };
    bool evenframe;
};

enum rendopt {
    RENDOPT_END,
    RENDOPT_ICON, // char*
    RENDOPT_API, // enum rendapi
    RENDOPT_MODE, // enum rendmode
    RENDOPT_VSYNC, // bool
    RENDOPT_RES, // struct rendres*
    RENDOPT_LIGHTING, // enum rendlighting
    RENDOPT_TEXTUREQLT, // enum rcopt_texture_qlt
};

bool initRenderer(struct rendstate*);
bool startRenderer(struct rendstate*);
bool updateRendererConfig(struct rendstate*, ...);
void lockRendererConfig(struct rendstate*);
void unlockRendererConfig(struct rendstate*);
bool restartRenderer(struct rendstate*);
void stopRenderer(struct rendstate*);
void termRenderer(struct rendstate*);
void render(struct rendstate*);

extern const char* rendapi_ids[];
extern const char* rendapi_names[];

#endif
