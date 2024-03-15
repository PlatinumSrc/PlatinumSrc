#ifndef PSRC_ENGINE_INPUT_H
#define PSRC_ENGINE_INPUT_H

#include "../platform.h"
#include "../common/threading.h"

#if PLATFORM == PLAT_NXDK
    #include <SDL.h>
#elif defined(PSRC_USESDL1)
    #include <SDL/SDL.h>
#else
    #include <SDL2/SDL.h>
#endif

#include <stdbool.h>

enum __attribute__((packed)) inputdev {
    INPUTDEV__NULL = -2,
    INPUTDEV__INVALID,
    INPUTDEV_KEYBOARD,
    INPUTDEV_MOUSE,
    INPUTDEV_GAMEPAD,
};

enum __attribute__((packed)) inputdevpart_mouse {
    INPUTDEVPART_MOUSE_BUTTON,
    INPUTDEVPART_MOUSE_MOVEMENT,
    INPUTDEVPART_MOUSE_SCROLL,
};
enum __attribute__((packed)) inputdevpart_gamepad {
    INPUTDEVPART_GAMEPAD_AXIS,
    INPUTDEVPART_GAMEPAD_BUTTON,
};

enum __attribute__((packed)) inputdevkey_mouse_button {
    INPUTDEVKEY_MOUSE_BUTTON_LEFT,
    INPUTDEVKEY_MOUSE_BUTTON_RIGHT,
    INPUTDEVKEY_MOUSE_BUTTON_MIDDLE,
};
enum __attribute__((packed)) inputdevkey_mouse_movement {
    INPUTDEVKEY_MOUSE_MOVEMENT_PX,
    INPUTDEVKEY_MOUSE_MOVEMENT_PY,
    INPUTDEVKEY_MOUSE_MOVEMENT_NX,
    INPUTDEVKEY_MOUSE_MOVEMENT_NY,
};

struct inputkey {
    enum inputdev dev;
    union {
        struct {
            int key;
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
                uint8_t button;
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

enum __attribute__((packed)) inputactiontype {
    INPUTACTIONTYPE_INVALID = -1,
    INPUTACTIONTYPE_ONCE,
    INPUTACTIONTYPE_SINGLE,
    INPUTACTIONTYPE_MULTI,
};

struct __attribute__((packed)) inputactiondata {
    enum inputactiontype type;
    char* name;
    struct inputkey* keys;
    void* userdata;
};

enum __attribute__((packed)) inputmode {
    INPUTMODE_UI,
    INPUTMODE_INGAME,
    INPUTMODE_TEXTINPUT,
    INPUTMODE_GETKEY,
};

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
