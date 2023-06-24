#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include <SDL2/SDL.h>

#include <stdbool.h>
#include <inttypes.h>

enum rendapi {
    RENDAPI__INVAL = -1,
    RENDAPI_SOFTWARE,
    RENDAPI_GL_LEGACY,
    RENDAPI_GL_ADVANCED,
    RENDAPI_GLES_LEGACY,
    RENDAPI_GLES_ADVANCED,
    RENDAPI__COUNT,
};

enum rendapigroup {
    RENDAPIGROUP__INVAL = -1,
    RENDAPIGROUP_SOFTWARE,
    RENDAPIGROUP_GL,
    RENDAPIGROUP__COUNT,
};

enum winmode {
    WINMODE_WINDOWED,
    WINMODE_BORDERLESS,
    WINMODE_FULLSCREEN,
};

struct res {
    int width, height, hz;
};

struct rendstate {
    SDL_Window* window;
    SDL_GLContext glctx;
    enum rendapi api;
    enum rendapigroup apigroup;
    bool vsync;
    enum winmode winmode;
    struct {
        struct res current, windowed, fullscr, desktop;
    } res;
    union {
        struct {
            union {
                struct {
                } gl11;
                struct {
                } gl33;
                struct {
                } gles20;
                struct {
                } gles30;
            };
        } gl;
    };
    bool evenframe;
};

bool initRenderer(struct rendstate*);
bool startRenderer(struct rendstate*);
bool restartRenderer(struct rendstate*);
void stopRenderer(struct rendstate*);
void termRenderer(struct rendstate*);
void render(struct rendstate*);

extern const char* rendapi_ids[];
extern const char* rendapi_names[];

#endif
