#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include <SDL2/SDL.h>

#include <stdbool.h>
#include <inttypes.h>

enum rendapi {
    RENDAPI__INVAL = -1,
    RENDAPI_SOFTWARE,
    RENDAPI_GL11,
    RENDAPI_GL33,
    RENDAPI_GLES30,
    RENDAPI__COUNT,
};

enum rendapigroup {
    RENDAPIGROUP__INVAL = -1,
    RENDAPIGROUP_SOFTWARE,
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

struct rendupdate {
    bool api : 1;
    bool mode : 1;
    bool vsync : 1;
    bool res : 1;
    bool icon : 1;
};

struct rendconfig {
    enum rendapi api;
    enum rendmode mode;
    bool vsync;
    struct {
        struct rendres current;
        struct rendres windowed, fullscr;
    } res;
    char* icon;
};

struct rendstate {
    SDL_Window* window;
    enum rendapigroup apigroup;
    union {
        struct {
            bool init;
            SDL_GLContext ctx;
            union {
                struct {
                } gl11;
                struct {
                } gl33;
                struct {
                } gles;
            };
        } gl;
    };
    bool evenframe;
    struct rendconfig cfg;
};

bool initRenderer(struct rendstate*);
bool startRenderer(struct rendstate*);
bool updateRendererConfig(struct rendstate*, struct rendupdate*, struct rendconfig*);
bool restartRenderer(struct rendstate*);
void stopRenderer(struct rendstate*);
void termRenderer(struct rendstate*);
void render(struct rendstate*);

extern const char* rendapi_ids[];
extern const char* rendapi_names[];

#endif
