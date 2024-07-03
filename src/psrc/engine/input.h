#ifndef PSRC_ENGINE_INPUT_H
#define PSRC_ENGINE_INPUT_H

#include "../platform.h"
#include "../common/threading.h"

#if PLATFORM == PLAT_NXDK || PLATFORM == PLAT_GDK
    #include <SDL.h>
#elif defined(PSRC_USESDL1)
    #include <SDL/SDL.h>
    #if PLATFORM == PLAT_DREAMCAST
        #include <dc/maple.h>
        #include <dc/maple/controller.h>
    #endif
#else
    #include <SDL2/SDL.h>
#endif

#include <stdbool.h>

#include "../util.h"

PACKEDENUM(inputdev {
    INPUTDEV__NULL = -2,
    INPUTDEV__INVALID,
    INPUTDEV_KEYBOARD,
    INPUTDEV_MOUSE,
    INPUTDEV_GAMEPAD,
});

PACKEDENUM(inputdevpart_mouse {
    INPART_MOUSE_BUTTON,
    INPART_MOUSE_MOVEMENT,
    INPART_MOUSE_SCROLL,
});
PACKEDENUM(inputdevpart_gamepad {
    INPART_GAMEPAD_AXIS,
    INPART_GAMEPAD_BUTTON,
});

PACKEDENUM(inputdevkey_keyboard {
    INKEY_KB_ESC, INKEY_KB_F1, INKEY_KB_F2, INKEY_KB_F3, INKEY_KB_F4, INKEY_KB_F5, INKEY_KB_F6, INKEY_KB_F7,
    INKEY_KB_F8, INKEY_KB_F9, INKEY_KB_F10, INKEY_KB_F11, INKEY_KB_F12,
    INKEY_KB_BACKTICK, INKEY_KB_1, INKEY_KB_2, INKEY_KB_3, INKEY_KB_4, INKEY_KB_5, INKEY_KB_6, INKEY_KB_7, INKEY_KB_8,
    INKEY_KB_9, INKEY_KB_0, INKEY_KB_MINUS, INKEY_KB_EQUALS, INKEY_KB_BACKSPACE,
    INKEY_KB_TAB, INKEY_KB_Q, INKEY_KB_W, INKEY_KB_E, INKEY_KB_R, INKEY_KB_T, INKEY_KB_Y, INKEY_KB_U, INKEY_KB_I,
    INKEY_KB_O, INKEY_KB_P, INKEY_KB_LEFTBRACKET, INKEY_KB_RIGHTBRACKET, INKEY_KB_BACKSLASH,
    INKEY_KB_A, INKEY_KB_S, INKEY_KB_D, INKEY_KB_F, INKEY_KB_G, INKEY_KB_H, INKEY_KB_J, INKEY_KB_K, INKEY_KB_L,
    INKEY_KB_SEMICOLON, INKEY_KB_APOSTROPHE, INKEY_KB_ENTER,
    INKEY_KB_LEFTSHIFT, INKEY_KB_Z, INKEY_KB_X, INKEY_KB_C, INKEY_KB_V, INKEY_KB_B, INKEY_KB_N, INKEY_KB_M,
    INKEY_KB_COMMA, INKEY_KB_PERIOD, INKEY_KB_SLASH, INKEY_KB_RIGHTSHIFT,
    INKEY_KB_LEFTCTRL, INKEY_KB_LEFTALT, INKEY_KB_SPACE, INKEY_KB_RIGHTALT, INKEY_KB_MENU, INKEY_KB_RIGHTCTRL,
    INKEY_KB_PRTSC, INKEY_KB_PAUSE,
    INKEY_KB_INS, INKEY_KB_HOME, INKEY_KB_PGUP,
    INKEY_KB_DEL, INKEY_KB_END, INKEY_KB_PGDN,
    INKEY_KB_UP,
    INKEY_KB_LEFT, INKEY_KB_DOWN, INKEY_KB_RIGHT,
    INKEY_KB_KP_SLASH, INKEY_KB_KP_ASTERISK, INKEY_KB_KP_MINUS,
    INKEY_KB_KP_7, INKEY_KB_KP_8, INKEY_KB_KP_9, INKEY_KB_KP_PLUS,
    INKEY_KB_KP_4, INKEY_KB_KP_5, INKEY_KB_KP_6,
    INKEY_KB_KP_1, INKEY_KB_KP_2, INKEY_KB_KP_3, INKEY_KB_KP_ENTER,
    INKEY_KB_KP_0, INKEY_KB_KP_PERIOD,
    INKEY_KB__COUNT,
    
});
PACKEDENUM(inputdevkey_mouse_button {
    INKEY_MOUSE_BUTTON_LEFT,
    INKEY_MOUSE_BUTTON_RIGHT,
    INKEY_MOUSE_BUTTON_MIDDLE,
});
PACKEDENUM(inputdevkey_mouse_movement {
    INKEY_MOUSE_MOVEMENT_PX,
    INKEY_MOUSE_MOVEMENT_PY,
    INKEY_MOUSE_MOVEMENT_NX,
    INKEY_MOUSE_MOVEMENT_NY,
});

struct inputkey {
    enum inputdev dev;
    union {
        struct {
            enum inputdevkey_keyboard key;
        } keyboard;
        struct {
            enum inputdevpart_mouse part;
            union {
                enum inputdevkey_mouse_button button;
                enum inputdevkey_mouse_movement movement;
                uint8_t scroll;
            };
        } mouse;
        struct {
            enum inputdevpart_gamepad part;
            union {
                struct {
                    uint8_t whole : 1;
                    uint8_t negative : 1;
                    uint8_t id : 6;
                } axis;
                #if defined(PSRC_USESDL1) && PLATFORM == PLAT_DREAMCAST
                uint16_t button;
                #else
                uint8_t button;
                #endif
            };
        } gamepad;
    };
};

struct inputaction {
    int id;
    int amount;
    bool constant;
    struct inputactiondata* data;
    void* userdata;
};

PACKEDENUM(inputactiontype {
    INPUTACTIONTYPE__INVALID = -1,
    INPUTACTIONTYPE_ONCE,
    INPUTACTIONTYPE_SINGLE,
    INPUTACTIONTYPE_MULTI,
});

PACKEDSTRUCT(inputactiondata {
    enum inputactiontype type;
    char* name;
    struct inputkey* keys;
    void* userdata;
});

PACKEDENUM(inputmode {
    INPUTMODE_UI,
    INPUTMODE_INGAME,
    INPUTMODE_TEXTINPUT,
    INPUTMODE_GETKEY,
});

struct inputstate {
    enum inputmode mode;
    int keystatecount;
    const uint8_t* keystates;
    int mousechx;
    int mousechy;
    #ifndef PSRC_USESDL1
    SDL_GameController* gamepad;
    int16_t gamepadaxes[SDL_CONTROLLER_AXIS_MAX];
    uint8_t gamepadbuttons[(SDL_CONTROLLER_BUTTON_MAX + 7) / 8];
    #elif PLATFORM == PLAT_DREAMCAST
    cont_state_t* gamepadstate;
    #endif
    struct {
        struct inputactiondata* data;
        int len;
        int size;
    } actions;
    int curaction;
    int activeaction;
};

extern struct inputstate inputstate;

bool initInput(void);
void setInputMode(enum inputmode);
void pollInput(void);
int newInputAction(enum inputactiontype, const char* name, struct inputkey*, void* userdata);
void clearInputActionKeys(int);
void setInputActionKeys(int, struct inputkey*);
void addInputActionKey(int, struct inputkey*);
void clearInputActions(void);
struct inputkey* inputKeysFromStr(const char*);
char* strFromInputKeys(struct inputkey*);
void deleteInputKeys(struct inputkey*);
bool getNextInputAction(struct inputaction*);
void quitInput(void);

#endif
