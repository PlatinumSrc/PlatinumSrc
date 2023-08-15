#ifndef ENGINE_INPUT_H
#define ENGINE_INPUT_H

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
    struct rendstate* r;
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

bool initInput(struct inputstate*, struct rendstate*);
int registerEvent(struct inputstate*, char*, struct inputkey*);
void removeEvent(struct inputstate*, int);
void pollInput(struct inputstate*);
bool getNextEvent(struct inputstate*, struct inputevent*);
void termInput(struct inputstate*);

extern int quitreq;

#endif
