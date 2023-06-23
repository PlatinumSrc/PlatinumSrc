#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include <SDL2/SDL.h>

#include <stdbool.h>

enum rendapi {
    RENDAPI__INVAL = -1,
    RENDAPI_SOFTWARE,
    RENDAPI_GL_LEGACY,
    RENDAPI_GL_ADVANCED,
    RENDAPI_GLES_LEGACY,
    RENDAPI_GLES_ADVANCED,
    RENDAPI__COUNT,
};

enum rendapitype {
    RENDAPITYPE__INVAL = -1,
    RENDAPITYPE_SOFTWARE,
    RENDAPITYPE_GL,
    RENDAPITYPE__COUNT,
};

struct rendstate {
    SDL_Window* window;
    SDL_GLContext glctx;
    enum rendapi api;
    enum rendapitype apitype;
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
