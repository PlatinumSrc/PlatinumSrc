#include "input.h"
#include "renderer.h"
#include "../common/logging.h"
#include "../common/string.h"
#include "../common.h"
#include "../platform.h"
#include "../debug.h"

#if PLATFORM == PLAT_EMSCR
    #include <emscripten/html5.h>
#endif

#include <string.h>

#include "../glue.h"

struct inputstate inputstate;

static const char* kbkeynames[INKEY_KB__COUNT] = {
    "esc", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12",
    "`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "backspace",
    "tab", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "enter",
    "lshift", "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "rshift",
    "lctrl", "lalt", "space", "ralt", "menu", "rctrl",
    "prtsc", "pause",
    "ins", "home", "pgup",
    "del", "end", "pgdn",
    "up",
    "left", "down", "right",
    "kp /", "kp *", "kp -",
    "kp 7", "kp 8", "kp 9", "kp +",
    "kp 4", "kp 5", "kp 6",
    "kp 1", "kp 2", "kp 3", "kp enter",
    "kp 0", "kp ."
};
static const char* kbkeydispnames[INKEY_KB__COUNT] = {
    "Esc", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "Backspace",
    "Tab", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "\\",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "Enter",
    "Left Shift", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "Right Shift",
    "Left Control", "Left Alt", "Space", "Right Alt", "Menu", "Right Control",
    "Print Screen", "Pause",
    "Insert", "Home", "Page Up",
    "Delete", "End", "Page Down",
    "Up",
    "Left", "Down", "Right",
    "Keypad /", "Keypad *", "Keypad -",
    "Keypad 7", "Keypad 8", "Keypad 9", "Keypad +",
    "Keypad 4", "Keypad 5", "Keypad 6",
    "Keypad 1", "Keypad 2", "Keypad 3", "Keypad Enter",
    "Keypad 0", "Keypad ."
};
#ifndef PSRC_USESDL1
static const SDL_Scancode kbkeynums[INKEY_KB__COUNT] = {
    SDL_SCANCODE_ESCAPE, SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4, SDL_SCANCODE_F5,
    SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8, SDL_SCANCODE_F9, SDL_SCANCODE_F10, SDL_SCANCODE_F11,
    SDL_SCANCODE_F12,
    SDL_SCANCODE_GRAVE, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6,
    SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_0, SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS,
    SDL_SCANCODE_BACKSPACE,
    SDL_SCANCODE_TAB, SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T, SDL_SCANCODE_Y,
    SDL_SCANCODE_U, SDL_SCANCODE_I, SDL_SCANCODE_O, SDL_SCANCODE_P, SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_RIGHTBRACKET,
    SDL_SCANCODE_BACKSLASH,
    SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J,
    SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_RETURN,
    SDL_SCANCODE_LSHIFT, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V, SDL_SCANCODE_B, SDL_SCANCODE_N,
    SDL_SCANCODE_M, SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH, SDL_SCANCODE_RSHIFT,
    SDL_SCANCODE_LCTRL, SDL_SCANCODE_LALT, SDL_SCANCODE_SPACE, SDL_SCANCODE_RALT, SDL_SCANCODE_MENU, SDL_SCANCODE_RCTRL,
    SDL_SCANCODE_PRINTSCREEN, SDL_SCANCODE_PAUSE,
    SDL_SCANCODE_INSERT, SDL_SCANCODE_HOME, SDL_SCANCODE_PAGEUP,
    SDL_SCANCODE_DELETE, SDL_SCANCODE_END, SDL_SCANCODE_PAGEDOWN,
    SDL_SCANCODE_UP,
    SDL_SCANCODE_LEFT, SDL_SCANCODE_DOWN, SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_KP_DIVIDE, SDL_SCANCODE_KP_MULTIPLY, SDL_SCANCODE_KP_MINUS,
    SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9, SDL_SCANCODE_KP_PLUS,
    SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_6,
    SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3, SDL_SCANCODE_KP_ENTER,
    SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_PERIOD
};
#else
static const SDLKey kbkeynums[INKEY_KB__COUNT] = {
    SDLK_ESCAPE, SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10, SDLK_F11,
    SDLK_F12,
    SDLK_BACKQUOTE, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9, SDLK_0, SDLK_MINUS,
    SDLK_EQUALS, SDLK_BACKSPACE,
    SDLK_TAB, SDLK_q, SDLK_w, SDLK_e, SDLK_r, SDLK_t, SDLK_y, SDLK_u, SDLK_i, SDLK_o, SDLK_p, SDLK_LEFTBRACKET,
    SDLK_RIGHTBRACKET, SDLK_BACKSLASH,
    SDLK_a, SDLK_s, SDLK_d, SDLK_f, SDLK_g, SDLK_h, SDLK_j, SDLK_k, SDLK_l, SDLK_SEMICOLON, SDLK_QUOTE, SDLK_RETURN,
    SDLK_LSHIFT, SDLK_z, SDLK_x, SDLK_c, SDLK_v, SDLK_b, SDLK_n, SDLK_m, SDLK_COMMA, SDLK_PERIOD, SDLK_SLASH,
    SDLK_RSHIFT,
    SDLK_LCTRL, SDLK_LALT, SDLK_SPACE, SDLK_RALT, SDLK_MENU, SDLK_RCTRL,
    SDLK_PRINT, SDLK_PAUSE,
    SDLK_INSERT, SDLK_HOME, SDLK_PAGEUP,
    SDLK_DELETE, SDLK_END, SDLK_PAGEDOWN,
    SDLK_UP,
    SDLK_LEFT, SDLK_DOWN, SDLK_RIGHT,
    SDLK_KP_DIVIDE, SDLK_KP_MULTIPLY, SDLK_KP_MINUS,
    SDLK_KP7, SDLK_KP8, SDLK_KP9, SDLK_KP_PLUS,
    SDLK_KP4, SDLK_KP5, SDLK_KP6,
    SDLK_KP1, SDLK_KP2, SDLK_KP3, SDLK_KP_ENTER,
    SDLK_KP0, SDLK_KP_PERIOD
};
#endif

static inline int getkbkeyfromname(const char* name) {
    for (int i = 0; i < INKEY_KB__COUNT; ++i) {
        if (!strcasecmp(name, kbkeynames[i])) return i;
    }
    return -1;
}
static inline const char* getnamefromkbkey(enum inputdevkey_keyboard key) {
    return kbkeynames[key];
}

void setInputMode(enum inputmode m) {
    inputstate.mode = m;
    switch (m) {
        case INPUTMODE_UI: {
            #if PLATFORM != PLAT_NXDK
            #ifndef PSRC_USESDL1
            SDL_SetRelativeMouseMode(false);
            #else
            SDL_WM_GrabInput(SDL_GRAB_OFF);
            SDL_ShowCursor(1);
            #endif
            #endif
        } break;
        case INPUTMODE_INGAME: {
            #if PLATFORM != PLAT_NXDK
            #ifndef PSRC_USESDL1
            SDL_SetRelativeMouseMode(true);
            #else
            SDL_ShowCursor(0);
            SDL_WM_GrabInput(SDL_GRAB_ON);
            #endif
            #endif
        } break;
        case INPUTMODE_TEXTINPUT: {
            #if PLATFORM != PLAT_NXDK
            #ifndef PSRC_USESDL1
            SDL_SetRelativeMouseMode(false);
            #else
            SDL_WM_GrabInput(SDL_GRAB_OFF);
            SDL_ShowCursor(1);
            #endif
            #endif
        } break;
        case INPUTMODE_GETKEY: {
            #if PLATFORM != PLAT_NXDK
            #ifndef PSRC_USESDL1
            SDL_SetRelativeMouseMode(true);
            #else
            SDL_ShowCursor(0);
            SDL_WM_GrabInput(SDL_GRAB_ON);
            #endif
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
    #ifndef PSRC_USESDL1
    char* tmp;
    if (!options.nocontroller) {
        tmp = cfg_getvar(config, "Input", "nocontroller");
        if (!strbool(tmp, false)) {
            if (SDL_Init(SDL_INIT_GAMECONTROLLER)) return false;
            SDL_GameControllerEventState(SDL_ENABLE);
        }
        free(tmp);
    }
    tmp = cfg_getvar(config, "Input", "rawmouse");
    SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, (strbool(tmp, true)) ? "1" : "0", SDL_HINT_OVERRIDE);
    free(tmp);
    inputstate.keystates = SDL_GetKeyboardState(&inputstate.keystatecount);
    #else
    inputstate.keystates = SDL_GetKeyState(&inputstate.keystatecount);
    #endif
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
            #ifndef PSRC_USESDL1
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
            #else
            case SDL_VIDEORESIZE: {
                struct rendres res = {
                    .width = e.resize.w,
                    .height = e.resize.h
                };
                updateRendererConfig(RENDOPT_RES, &res, RENDOPT_END);
            } break;
            #endif
            default: {
                switch (inputstate.mode) {
                    case INPUTMODE_INGAME: {
                        switch (e.type) {
                            case SDL_MOUSEMOTION: {
                                #ifndef PSRC_USESDL1
                                if (e.motion.which == SDL_TOUCH_MOUSEID) break;
                                #endif
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
                    if (inputstate.keystates[kbkeynums[k.keyboard.key]] && value < 32767) {
                        constant = true;
                        value = 32767;
                    }
                } break;
                case INPUTDEV_MOUSE: {
                    switch (k.mouse.part) {
                        case INPART_MOUSE_MOVEMENT: {
                            switch (k.mouse.movement) {
                                case INKEY_MOUSE_MOVEMENT_PX: {
                                    int tmp = inputstate.mousechx * 2500;
                                    if (tmp > value) {constant = false; value = tmp;}
                                } break;
                                case INKEY_MOUSE_MOVEMENT_PY: {
                                    int tmp = inputstate.mousechy * -2500;
                                    if (tmp > value) {constant = false; value = tmp;}
                                } break;
                                case INKEY_MOUSE_MOVEMENT_NX: {
                                    int tmp = inputstate.mousechx * -2500;
                                    if (tmp > value) {constant = false; value = tmp;}
                                } break;
                                case INKEY_MOUSE_MOVEMENT_NY: {
                                    int tmp = inputstate.mousechy * 2500;
                                    if (tmp > value) {constant = false; value = tmp;}
                                } break;
                            }
                        } break;
                        default: break;
                    }
                } break;
                #ifndef PSRC_USESDL1
                case INPUTDEV_GAMEPAD: {
                    switch (k.gamepad.part) {
                        case INPART_GAMEPAD_AXIS: {
                            int tmp = inputstate.gamepadaxes[k.gamepad.axis.id];
                            if (k.gamepad.axis.negative) tmp *= -1;
                            if (tmp >= 7634 && tmp > value) {constant = true; value = tmp;}
                        } break;
                        case INPART_GAMEPAD_BUTTON: {
                            if (inputstate.gamepadbuttons[k.gamepad.button / 8] & (1 << k.gamepad.button % 8) && value < 32767) {
                                constant = true;
                                value = 32767;
                            }
                        } break;
                    }
                } break;
                #endif
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
                int tmp = getkbkeyfromname(kds[1]);
                if (tmp != -1) {
                    k[i].dev = INPUTDEV_KEYBOARD;
                    k[i].keyboard.key = tmp;
                }
            }
        } else if (dcount == 3) {
            if (!strcasecmp(kds[0], "m") || !strcasecmp(kds[0], "mouse")) {
                if (!strcasecmp(kds[1], "b") || !strcasecmp(kds[1], "button")) {
                    if (!strcasecmp(kds[2], "l") || !strcasecmp(kds[2], "left")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPART_MOUSE_BUTTON;
                        k[i].mouse.button = INKEY_MOUSE_BUTTON_LEFT;
                    } else if (!strcasecmp(kds[2], "r") || !strcasecmp(kds[2], "right")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPART_MOUSE_BUTTON;
                        k[i].mouse.button = INKEY_MOUSE_BUTTON_RIGHT;
                    } else if (!strcasecmp(kds[2], "m") || !strcasecmp(kds[2], "middle")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPART_MOUSE_BUTTON;
                        k[i].mouse.button = INKEY_MOUSE_BUTTON_MIDDLE;
                    }
                } else if (!strcasecmp(kds[1], "m") || !strcasecmp(kds[1], "movement")) {
                    if (!strcasecmp(kds[2], "+x")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPART_MOUSE_MOVEMENT;
                        k[i].mouse.movement = INKEY_MOUSE_MOVEMENT_PX;
                    } else if (!strcasecmp(kds[2], "-x")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPART_MOUSE_MOVEMENT;
                        k[i].mouse.movement = INKEY_MOUSE_MOVEMENT_NX;
                    } else if (!strcasecmp(kds[2], "+y")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPART_MOUSE_MOVEMENT;
                        k[i].mouse.movement = INKEY_MOUSE_MOVEMENT_PY;
                    } else if (!strcasecmp(kds[2], "-y")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPART_MOUSE_MOVEMENT;
                        k[i].mouse.movement = INKEY_MOUSE_MOVEMENT_NY;
                    }
                } else if (!strcasecmp(kds[1], "s") || !strcasecmp(kds[1], "scroll")) {
                    if (!strcasecmp(kds[2], "+") || !strcasecmp(kds[2], "u") || !strcasecmp(kds[2], "up")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPART_MOUSE_SCROLL;
                        k[i].mouse.scroll = 1;
                    } else if (!strcasecmp(kds[2], "-") || !strcasecmp(kds[2], "d") || !strcasecmp(kds[2], "down")) {
                        k[i].dev = INPUTDEV_MOUSE;
                        k[i].mouse.part = INPART_MOUSE_SCROLL;
                        k[i].mouse.scroll = 0;
                    }
                }
            }
            #ifndef PSRC_USESDL1
              else if (!strcasecmp(kds[0], "g") || !strcasecmp(kds[0], "gamepad")) {
                if (!strcasecmp(kds[1], "a") || !strcasecmp(kds[1], "axis")) {
                    if (*kds[2] == '+' || *kds[2] == '-') {
                        SDL_GameControllerAxis tmp = SDL_GameControllerGetAxisFromString(&kds[2][1]);
                        if (tmp >= 0) {
                            k[i].dev = INPUTDEV_GAMEPAD;
                            k[i].gamepad.part = INPART_GAMEPAD_AXIS;
                            k[i].gamepad.axis.id = tmp;
                            k[i].gamepad.axis.negative = (*kds[2] == '-');
                        }
                    }
                } else if (!strcasecmp(kds[1], "b") || !strcasecmp(kds[1], "button")) {
                    SDL_GameControllerButton tmp = SDL_GameControllerGetButtonFromString(kds[2]);
                    if (tmp >= 0) {
                        k[i].dev = INPUTDEV_GAMEPAD;
                        k[i].gamepad.part = INPART_GAMEPAD_BUTTON;
                        k[i].gamepad.button = tmp;
                    }
                }
            }
            #endif
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

void quitInput(void) {
    clearInputActions();
    #if PLATFORM == PLAT_EMSCR
    emscripten_set_fullscreenchange_callback("#canvas", NULL, false, NULL);
    #endif
}
