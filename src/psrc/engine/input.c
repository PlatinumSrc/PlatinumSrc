#include "input.h"
#include "renderer.h"
#include "../common/logging.h"
#include "../common/string.h"
#include "../common/common.h"
#include "../platform.h"
#include "../debug.h"

#if PLATFORM == PLAT_EMSCR
    #include <emscripten/html5.h>
#endif

#include <string.h>

#include "../glue.h"

struct inputstate inputstate;

void setInputMode(enum inputmode m) {
    inputstate.mode = m;
    switch (m) {
        case INPUTMODE_UI: {
            #if PLATFORM != PLAT_NXDK
            SDL_SetRelativeMouseMode(false);
            #endif
        } break;
        case INPUTMODE_INGAME: {
            #if PLATFORM != PLAT_NXDK
            SDL_SetRelativeMouseMode(true);
            #endif
        } break;
        case INPUTMODE_TEXTINPUT: {
            #if PLATFORM != PLAT_NXDK
            SDL_SetRelativeMouseMode(false);
            #endif
        } break;
        case INPUTMODE_GETKEY: {
            #if PLATFORM != PLAT_NXDK
            SDL_SetRelativeMouseMode(true);
            #endif
        } break;
    }
}

#if PLATFORM == PLAT_EMSCR
static bool emscrfullscr = false;
static EM_BOOL emscrfullscrcb(int e, const EmscriptenFullscreenChangeEvent* ed, void *d) {
    (void)e; (void)d;
    emscrfullscr = ed->isFullscreen;
    return EM_TRUE;
}
#endif

bool initInput(void) {
    #if PLATFORM == PLAT_EMSCR
    emscripten_set_fullscreenchange_callback("#canvas", NULL, false, emscrfullscrcb);
    emscrfullscr = (rendstate.mode == RENDMODE_BORDERLESS || rendstate.mode == RENDMODE_FULLSCREEN);
    #endif
    char* tmp = cfg_getvar(config, "Input", "nocontroller");
    if (!strbool(tmp, false)) {
        if (SDL_Init(SDL_INIT_GAMECONTROLLER)) return false;
        SDL_GameControllerEventState(SDL_ENABLE);
    }
    free(tmp);
    tmp = cfg_getvar(config, "Input", "rawmouse");
    if (strbool(tmp, true)) {
        SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0", SDL_HINT_OVERRIDE);
    } else {
        SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1", SDL_HINT_OVERRIDE);
    }
    free(tmp);
    inputstate.keystates = SDL_GetKeyboardState(&inputstate.keystatecount);
    setInputMode(INPUTMODE_UI);
    clearInputActions();
    return true;
}

void pollInput(void) {
    inputstate.curaction = 0;
    inputstate.mousechx = 0;
    inputstate.mousechy = 0;
    #if PLATFORM == PLAT_EMSCR
    if (rendstate.mode == RENDMODE_BORDERLESS || rendstate.mode == RENDMODE_FULLSCREEN) {
        if (!emscrfullscr) updateRendererConfig(RENDOPT_FULLSCREEN, 0, RENDOPT_END);
    }
    #endif
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
            case SDL_CONTROLLERDEVICEADDED: {
                if (!inputstate.gamepad) {
                    inputstate.gamepad = SDL_GameControllerOpen(e.cdevice.which);
                }
            } break;
            case SDL_CONTROLLERDEVICEREMOVED: {
                if (inputstate.gamepad) {
                    if (e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(inputstate.gamepad))) {
                        SDL_GameControllerClose(inputstate.gamepad);
                        inputstate.gamepad = NULL;
                    }
                }
            } break;
            case SDL_CONTROLLERAXISMOTION: {
                inputstate.gamepadaxes[e.caxis.axis] = e.caxis.value + (e.caxis.value < 0);
            } break;
            case SDL_CONTROLLERBUTTONDOWN: {
                inputstate.gamepadbuttons[e.cbutton.button / 8] |= 0x01 << (e.cbutton.button % 8);
            } break;
            case SDL_CONTROLLERBUTTONUP: {
                inputstate.gamepadbuttons[e.cbutton.button / 8] &= ~(0x01 << (e.cbutton.button % 8));
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

bool getNextInputAction(struct inputaction* a) {
    if (inputstate.mode != INPUTMODE_INGAME) return false;
    struct inputactiondata* actdata;
    trynext:;
    if (inputstate.curaction >= inputstate.actions.len) return false;
    int index = inputstate.curaction++;
    actdata = &inputstate.actions.data[index];
    if (actdata->type == INPUTACTIONTYPE_INVALID) goto trynext;
    if (inputstate.activeaction >= 0 &&
        (actdata->type == INPUTACTIONTYPE_ONCE || actdata->type == INPUTACTIONTYPE_SINGLE) &&
        index != inputstate.activeaction) goto trynext;
    bool constant;
    int value = 0;
    {
        struct inputkey* keys = actdata->keys;
        while (keys->dev != INPUTDEV__NULL) {
            struct inputkey k = *keys;
            switch (k.dev) {
                case INPUTDEV_KEYBOARD: {
                    if (inputstate.keystates[k.keyboard.key] && value < 32767) {
                        constant = true;
                        value = 32767;
                    }
                } break;
                case INPUTDEV_MOUSE: {
                    switch (k.mouse.part) {
                        case INPUTDEVPART_MOUSE_MOVEMENT: {
                            switch (k.mouse.movement) {
                                case INPUTDEVKEY_MOUSE_MOVEMENT_PX: {
                                    int tmp = inputstate.mousechx * 2500;
                                    if (tmp > value) {constant = false; value = tmp;}
                                } break;
                                case INPUTDEVKEY_MOUSE_MOVEMENT_PY: {
                                    int tmp = inputstate.mousechy * -2500;
                                    if (tmp > value) {constant = false; value = tmp;}
                                } break;
                                case INPUTDEVKEY_MOUSE_MOVEMENT_NX: {
                                    int tmp = inputstate.mousechx * -2500;
                                    if (tmp > value) {constant = false; value = tmp;}
                                } break;
                                case INPUTDEVKEY_MOUSE_MOVEMENT_NY: {
                                    int tmp = inputstate.mousechy * 2500;
                                    if (tmp > value) {constant = false; value = tmp;}
                                } break;
                            }
                        } break;
                        default: break;
                    }
                } break;
                case INPUTDEV_GAMEPAD: {
                    switch (k.gamepad.part) {
                        case INPUTDEVPART_GAMEPAD_AXIS: {
                            int tmp = inputstate.gamepadaxes[k.gamepad.axis.id];
                            if (k.gamepad.axis.negative) tmp *= -1;
                            if (tmp >= 7634 && tmp > value) {constant = true; value = tmp;}
                        } break;
                        case INPUTDEVPART_GAMEPAD_BUTTON: {
                            if (inputstate.gamepadbuttons[k.gamepad.button / 8] & (1 << k.gamepad.button % 8) && value < 32767) {
                                constant = true;
                                value = 32767;
                            }
                        } break;
                    }
                } break;
                default: break;
            }
            ++keys;
        }
    }
    if (value <= 0) {
        if ((actdata->type == INPUTACTIONTYPE_ONCE || actdata->type == INPUTACTIONTYPE_SINGLE) &&
            index == inputstate.activeaction) inputstate.activeaction = -1;
        goto trynext;
    }
    if (actdata->type == INPUTACTIONTYPE_ONCE) {
        if (index == inputstate.activeaction) goto trynext;
        inputstate.activeaction = index;
    } else if (actdata->type == INPUTACTIONTYPE_SINGLE) {
        inputstate.activeaction = index;
    }
    a->id = index;
    a->amount = value;
    a->constant = constant;
    a->data = actdata;
    a->userdata = actdata->userdata;
    return true;
}

int newInputAction(enum inputactiontype type, const char* name, struct inputkey* keys, void* userdata) {
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
        .keys = dupkeys,
        .userdata = userdata
    };
    return index;
}

struct inputkey* inputKeysFromStr(const char* s) {
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
                SDL_Scancode tmp = SDL_GetScancodeFromName(kds[1]);
                if (tmp >= 0) {
                    k[i].dev = INPUTDEV_KEYBOARD;
                    k[i].keyboard.key = tmp;
                }
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
            } else if (!strcasecmp(kds[0], "g") || !strcasecmp(kds[0], "gamepad")) {
                if (!strcasecmp(kds[1], "a") || !strcasecmp(kds[1], "axis")) {
                    if (*kds[2] == '+' || *kds[2] == '-') {
                        SDL_GameControllerAxis tmp = SDL_GameControllerGetAxisFromString(&kds[2][1]);
                        if (tmp >= 0) {
                            k[i].dev = INPUTDEV_GAMEPAD;
                            k[i].gamepad.part = INPUTDEVPART_GAMEPAD_AXIS;
                            k[i].gamepad.axis.id = tmp;
                            k[i].gamepad.axis.negative = (*kds[2] == '-');
                        }
                    }
                } else if (!strcasecmp(kds[1], "b") || !strcasecmp(kds[1], "button")) {
                    SDL_GameControllerButton tmp = SDL_GameControllerGetButtonFromString(kds[2]);
                    if (tmp >= 0) {
                        k[i].dev = INPUTDEV_GAMEPAD;
                        k[i].gamepad.part = INPUTDEVPART_GAMEPAD_BUTTON;
                        k[i].gamepad.button = tmp;
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
    inputstate.activeaction = -1;
    for (int i = 0; i < inputstate.actions.len; ++i) {
        free(inputstate.actions.data[i].name);
        free(inputstate.actions.data[i].keys);
    }
    free(inputstate.actions.data);
    inputstate.actions.len = 0;
    inputstate.actions.size = 16;
    inputstate.actions.data = malloc(inputstate.actions.size * sizeof(*inputstate.actions.data));
}

void termInput(void) {
    clearInputActions();
    #if PLATFORM == PLAT_EMSCR
    emscripten_set_fullscreenchange_callback("#canvas", NULL, false, NULL);
    #endif
}
