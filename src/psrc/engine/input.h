#ifndef PSRC_ENGINE_INPUT_H
#define PSRC_ENGINE_INPUT_H

#include "../utils/threading.h"

#include <stdbool.h>

enum __attribute__((packed)) inputdev {
    INPUTDEV_KEYBOARD,
    INPUTDEV_MOUSE,
    INPUTDEV_GAMEPAD,
};

enum __attribute__((packed)) inputdevpart {
    INPUTDEVPART_MOUSE_BUTTON,
    INPUTDEVPART_MOUSE_SCROLLWHEEL,
    INPUTDEVPART_GAMEPAD_AXIS,
    INPUTDEVPART_GAMEPAD_BUTTON,
};

struct inputkey {
    enum inputdev dev;
    union {
        struct {
            int key;
        } keyboard;
        struct {
            enum inputdevpart part;
            union {
                uint8_t button;
                uint8_t scrolldir;
            };
        } mouse;
        struct {
            enum inputdevpart part;
            union {
                struct {
                    int8_t id;
                    uint8_t positive : 1;
                } axis;
                int8_t button;
            };
        } gamepad;
    };
};

struct inputaction {
    int id;
    float amount;
    struct inputactiondata* data;
};

enum __attribute__((packed)) inputactiontype {
    INPUTACTIONTYPE_INVALID = -1,
    INPUTACTIONTYPE_SINGLE,
    INPUTACTIONTYPE_MULTI,
};

struct inputactiondata {
    enum inputactiontype type;
    char* name;
    struct inputkey* keys;
};

enum __attribute__((packed)) inputmode {
    INPUTMODE_UI,
    INPUTMODE_INGAME,
    INPUTMODE_TEXTINPUT,
    INPUTMODE_GETKEY,
};

struct inputstate {
    struct accesslock lock;
    enum inputmode mode;
    int keystates;
    const uint8_t* keystatedata;
    struct {
        int size;
        int len;
        struct inputactiondata* data;
    } actions;
    int curaction;
    int activeaction;
};

extern struct inputstate inputstate;

bool initInput(void);
void setInputMode(enum inputmode);
void pollInput(void);
int newInputAction(enum inputactiontype, const char*, struct inputkey*);
void setInputActionKeys(int, struct inputkey*);
void deleteInputAction(int);
bool getNextAction(struct inputaction*);
void termInput(void);
struct inputkey* strToInputKeys(const char*);

#endif
