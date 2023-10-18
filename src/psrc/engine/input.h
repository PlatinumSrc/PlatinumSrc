#ifndef PSRC_ENGINE_INPUT_H
#define PSRC_ENGINE_INPUT_H

#include "renderer.h"

#include <stdbool.h>

enum inputdev {
    INPUTDEV_KEYBOARD,
    INPUTDEV_MOUSE,
    INPUTDEV_GAMEPAD,
};

enum inputdevpart {
    INPUTDEVPART_MOUSE_BUTTONS,
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

struct inputinfo {
    char* name;
    int keyct;
    struct inputkey* keys;
};

struct inputaction {
    int id;
    char* name;
    float amount;
};

struct inputactiondata {
    char* name;
    float amount;
};

struct inputstate {
};

extern struct inputstate inputstate;

bool initInput(void);
int registerAction(const char*, struct inputkey*);
void removeAction(int);
void pollInput(void);
bool getNextAction(struct inputaction*);
void termInput(void);
struct inputkey* inputKeysFromStr(const char*);
char* inputKeysToStr(const struct inputkey*);

extern int quitreq;

#endif
