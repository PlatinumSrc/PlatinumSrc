#include "input.h"
#include "renderer.h"
#include "../utils/logging.h"
#include "../common/common.h"
#include "../debug.h"

struct inputstate inputstate;

void setInputMode(enum inputmode m) {
    switch (m) {
        case INPUTMODE_UI: {
            SDL_SetRelativeMouseMode(false);
        } break;
        case INPUTMODE_INGAME: {
            SDL_SetRelativeMouseMode(true);
        } break;
        case INPUTMODE_TEXTINPUT: {
        } break;
        case INPUTMODE_GETKEY: {
        } break;
    }
}

bool initInput(void) {
    if (!createAccessLock(&inputstate.lock)) return false;
    if (SDL_Init(SDL_INIT_GAMECONTROLLER)) return false;
    SDL_GameControllerEventState(SDL_ENABLE);
    inputstate.keystatedata = SDL_GetKeyboardState(&inputstate.keystates);
    inputstate.mode = INPUTMODE_UI;
    setInputMode(INPUTMODE_UI);
    return true;
}

void pollInput(void) {
    inputstate.curaction = 0;
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

bool getNextAction(struct inputaction* a) {
    acquireReadAccess(&inputstate.lock);
    struct inputactiondata* actdata;
    trynext:;
    if (inputstate.curaction >= inputstate.actions.len) {
        releaseReadAccess(&inputstate.lock);
        return false;
    }
    actdata = &inputstate.actions.data[inputstate.curaction];
    ++inputstate.curaction;
    if (actdata->type == INPUTACTIONTYPE_INVALID) goto trynext;
    if (inputstate.activeaction >= 0 && actdata->type == INPUTACTIONTYPE_SINGLE) goto trynext;
    releaseReadAccess(&inputstate.lock);
    return true;
}

void termInput(void) {
    destroyAccessLock(&inputstate.lock);
}
