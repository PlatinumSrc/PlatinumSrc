#include "input.h"
#include "renderer.h"
#include "../psrc_aux/logging.h"
#include "../debug.h"

int quitreq;

bool initInput(struct inputstate* s, struct rendstate* r) {
    memset(s, 0, sizeof(*s));
    s->r = r;
    s->eventcache.size = 256;
    s->eventcache.data = malloc(s->eventcache.size * sizeof(*s->eventcache.data));
    if (SDL_Init(SDL_INIT_GAMECONTROLLER)) return false;
    SDL_GameControllerEventState(SDL_ENABLE);
    return true;
}

void pollInput(struct inputstate* s) {
    (void)s;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT: {
                ++quitreq;
            } break;
            case SDL_WINDOWEVENT: {
                switch (e.window.event) {
                    case SDL_WINDOWEVENT_RESIZED: {
                        struct rendres res = {
                            .width = e.window.data1,
                            .height = e.window.data2,
                            .hz = -1
                        };
                        updateRendererConfig(s->r, RENDOPT_RES, &res);
                    } break;
                }
            } break;
        }
    }
}

void termInput(struct inputstate* s) {
    free(s->events.data);
    free(s->eventcache.data);
}
