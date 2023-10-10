#include "input.h"
#include "renderer.h"
#include "../aux/logging.h"
#include "../debug.h"

struct inputstate inputstate;

int quitreq;

bool initInput(void) {
    inputstate.eventcache.size = 256;
    inputstate.eventcache.data = malloc(inputstate.eventcache.size * sizeof(*inputstate.eventcache.data));
    if (SDL_Init(SDL_INIT_GAMECONTROLLER)) return false;
    SDL_GameControllerEventState(SDL_ENABLE);
    return true;
}

void pollInput(void) {
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
                        updateRendererConfig(RENDOPT_RES, &res, RENDOPT_END);
                    } break;
                }
            } break;
        }
    }
}

void termInput(void) {
    free(inputstate.events.data);
    free(inputstate.eventcache.data);
}
