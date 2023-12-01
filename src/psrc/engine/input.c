#include "input.h"
#include "renderer.h"
#include "../utils/logging.h"
#include "../utils/string.h"
#include "../common/common.h"
#include "../debug.h"

struct inputstate inputstate;

void setInputMode(enum inputmode m) {
    inputstate.mode = m;
    switch (m) {
        case INPUTMODE_UI: {
            SDL_SetRelativeMouseMode(false);
        } break;
        case INPUTMODE_INGAME: {
            SDL_SetRelativeMouseMode(true);
        } break;
        case INPUTMODE_TEXTINPUT: {
            SDL_SetRelativeMouseMode(false);
        } break;
        case INPUTMODE_GETKEY: {
            SDL_SetRelativeMouseMode(true);
        } break;
    }
}

bool initInput(void) {
    char* tmp = cfg_getvar(config, "Input", "nocontroller");
    if (!strbool(tmp, false)) {
        if (SDL_Init(SDL_INIT_GAMECONTROLLER)) return false;
        SDL_GameControllerEventState(SDL_ENABLE);
    }
    free(tmp);
    inputstate.keystates = SDL_GetKeyboardState(&inputstate.keystatecount);
    setInputMode(INPUTMODE_UI);
    clearInputActions();
    return true;
}

static int getKeys(struct inputkey* keys) {
    int ret = 0;
    while (keys->dev != INPUTDEV__NULL) {
        struct inputkey k = *keys;
        switch (k.dev) {
            case INPUTDEV_KEYBOARD: {
                if (k.keyboard.key >= 0 && k.keyboard.key < inputstate.keystatecount) {
                    if (inputstate.keystates[k.keyboard.key]) if (ret < 32767) ret = 32767;
                }
            } break;
            case INPUTDEV_MOUSE: {
                switch (k.mouse.part) {
                    case INPUTDEVPART_MOUSE_MOVEMENT: {
                        switch (k.mouse.movement) {
                            case INPUTDEVKEY_MOUSE_MOVEMENT_PX: {
                                if (ret < inputstate.mousechx) ret = inputstate.mousechx;
                            } break;
                            case INPUTDEVKEY_MOUSE_MOVEMENT_PY: {
                                if (ret < inputstate.mousechy) ret = inputstate.mousechy;
                            } break;
                            case INPUTDEVKEY_MOUSE_MOVEMENT_NX: {
                                int tmp = -inputstate.mousechx + 1;
                                if (ret < tmp) ret = tmp;
                            } break;
                            case INPUTDEVKEY_MOUSE_MOVEMENT_NY: {
                                int tmp = -inputstate.mousechy + 1;
                                if (ret < tmp) ret = tmp;
                            } break;
                        }
                    } break;
                    default: break;
                }
            } break;
            default: break;
        }
    }
    return ret;
}

void pollInput(void) {
    inputstate.curaction = 0;
    inputstate.mousechx = 0;
    inputstate.mousechy = 0;
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
                            .height = e.window.data2
                        };
                        updateRendererConfig(RENDOPT_RES, &res, RENDOPT_END);
                    } break;
                }
            } break;
            default: {
                switch (inputstate.mode) {
                    case INPUTMODE_INGAME: {
                        switch (e.type) {
                            case SDL_MOUSEMOTION: {
                                if (e.motion.which == SDL_TOUCH_MOUSEID) break;
                                inputstate.mousechx += e.motion.xrel;
                                inputstate.mousechy += e.motion.yrel;
                            } break;
                            default: break;
                        };
                    }
                    default: break;
                }
            } break;
        }
    }
}

bool getNextAction(struct inputaction* a) {
    if (inputstate.mode != INPUTMODE_INGAME) return false;
    struct inputactiondata* actdata;
    trynext:;
    if (inputstate.curaction >= inputstate.actions.len) return false;
    int index = inputstate.curaction++;
    actdata = &inputstate.actions.data[index];
    if (actdata->type == INPUTACTIONTYPE_INVALID) goto trynext;
    if (inputstate.activeaction >= 0 && actdata->type == INPUTACTIONTYPE_SINGLE) goto trynext;
    
    goto trynext;
    wract:;
    if (actdata->type == INPUTACTIONTYPE_SINGLE) inputstate.activeaction = index;
    return true;
}

int newInputAction(enum inputactiontype type, const char* name, struct inputkey* keys) {
    int index = -1;
    for (int i = 0; i < inputstate.actions.len; ++i) {
        if (inputstate.actions.data[i].type == INPUTACTIONTYPE_INVALID) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        index = inputstate.actions.len++;
        if (inputstate.actions.len == inputstate.actions.size) {
            inputstate.actions.size *= 2;
            inputstate.actions.data = realloc(
                inputstate.actions.data, inputstate.actions.size * sizeof(*inputstate.actions.data)
            );
        }
    }
    int dklen = 0;
    struct inputkey* dupkeys = keys;
    while (dupkeys->dev != INPUTDEV__NULL) {
        ++dupkeys;
        ++dklen;
    }
    dklen = dklen * sizeof(*dupkeys) + sizeof(enum inputdev);
    dupkeys = malloc(dklen);
    memcpy(dupkeys, keys, dklen);
    inputstate.actions.data[index] = (struct inputactiondata){
        .type = type,
        .name = strdup(name),
        .keys = dupkeys
    };
    return index;
}

struct inputkey* inputKeysFromStr(char* s) {
    if (!s || !*s) return NULL;
    int count;
    char** ks = splitstrlist(s, ';', false, &count);
    struct inputkey* k = malloc(count * sizeof(*k) + sizeof(enum inputdev));
    k[count].dev = INPUTDEV__NULL;
    for (int i = 0; i < count; ++i) {
        int dcount;
        char** kds = splitstrlist(ks[i], ',', false, &dcount);
        k[i].dev = INPUTDEV__INVALID;
        if (dcount == 2) {
            if (!strcasecmp(kds[0], "k") || !strcasecmp(kds[0], "keyboard")) {
                k[i].dev = INPUTDEV_KEYBOARD;
                k[i].keyboard.key = SDL_GetScancodeFromName(kds[1]);
            }
        } else if (dcount == 3) {
            if (!strcasecmp(kds[0], "m") || !strcasecmp(kds[0], "mouse")) {
                if (!strcasecmp(kds[1], "b") || !strcasecmp(kds[1], "button")) {
                    if (!strcasecmp(kds[2], "l") || !strcasecmp(kds[2], "left")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPUTDEVPART_MOUSE_BUTTON;
                        k[i].mouse.button = INPUTDEVKEY_MOUSE_BUTTON_LEFT;
                    } else if (!strcasecmp(kds[2], "r") || !strcasecmp(kds[2], "right")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPUTDEVPART_MOUSE_BUTTON;
                        k[i].mouse.button = INPUTDEVKEY_MOUSE_BUTTON_RIGHT;
                    } else if (!strcasecmp(kds[2], "m") || !strcasecmp(kds[2], "middle")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPUTDEVPART_MOUSE_BUTTON;
                        k[i].mouse.button = INPUTDEVKEY_MOUSE_BUTTON_MIDDLE;
                    }
                } else if (!strcasecmp(kds[1], "m") || !strcasecmp(kds[1], "movement")) {
                    if (!strcasecmp(kds[2], "+x")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPUTDEVPART_MOUSE_MOVEMENT;
                        k[i].mouse.movement = INPUTDEVKEY_MOUSE_MOVEMENT_PX;
                    } else if (!strcasecmp(kds[2], "-x")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPUTDEVPART_MOUSE_MOVEMENT;
                        k[i].mouse.movement = INPUTDEVKEY_MOUSE_MOVEMENT_NX;
                    } else if (!strcasecmp(kds[2], "+y")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPUTDEVPART_MOUSE_MOVEMENT;
                        k[i].mouse.movement = INPUTDEVKEY_MOUSE_MOVEMENT_PY;
                    } else if (!strcasecmp(kds[2], "-y")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPUTDEVPART_MOUSE_MOVEMENT;
                        k[i].mouse.movement = INPUTDEVKEY_MOUSE_MOVEMENT_NY;
                    }
                } else if (!strcasecmp(kds[1], "s") || !strcasecmp(kds[1], "scroll")) {
                    if (!strcasecmp(kds[2], "+") || !strcasecmp(kds[2], "u") || !strcasecmp(kds[2], "up")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPUTDEVPART_MOUSE_SCROLL;
                        k[i].mouse.scroll = 1;
                    } else if (!strcasecmp(kds[2], "-") || !strcasecmp(kds[2], "d") || !strcasecmp(kds[2], "down")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPUTDEVPART_MOUSE_SCROLL;
                        k[i].mouse.scroll = 0;
                    }
                }
            }
        }
        for (int j = 0; j < dcount; ++j) {
            free(kds[j]);
        }
        free(kds);
        free(ks[i]);
    }
    free(ks);
    return k;
}

void clearInputActions(void) {
    free(inputstate.actions.data);
    inputstate.actions.len = 0;
    inputstate.actions.size = 16;
    inputstate.actions.data = malloc(inputstate.actions.size * sizeof(*inputstate.actions.data));
}

void termInput(void) {
}
