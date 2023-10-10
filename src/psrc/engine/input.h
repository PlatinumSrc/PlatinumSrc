#ifndef PSRC_ENGINE_INPUT_H
#define PSRC_ENGINE_INPUT_H

#include "renderer.h"

#include <stdbool.h>

enum inputdevice {
    INPUTDEVICE_KEYBOARD,
    INPUTDEVICE_MOUSE,
    INPUTDEVICE__COUNT,
};

struct inputkey {
    enum inputdevice devtype;
    int devid;
    int button;
};

struct inputinfo {
    char* name;
    int keyct;
    struct inputkey* keys;
};

struct inputevent {
    int id;
    char* name;
    struct inputkey* key;
    float amount;
};

struct inputstate {
    struct {
        int count;
        struct inputinfo* data;
    } events;
    struct {
        int size;
        int rptr;
        int wptr;
        struct {
            int id;
            float amount;
        }* data;
    } eventcache;
};

extern struct inputstate inputstate;

bool initInput(void);
int registerEvent(char*, struct inputkey*);
void removeEvent(int);
void pollInput(void);
bool getNextEvent(struct inputevent*);
void termInput(void);

extern int quitreq;

#endif
