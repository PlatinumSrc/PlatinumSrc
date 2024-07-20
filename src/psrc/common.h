#ifndef PSRC_COMMON_COMMON_H
#define PSRC_COMMON_COMMON_H

#include "common/config.h"

#include <stdbool.h>

extern int quitreq;

extern struct options {
    char* game;
    char* mods;
    char* icon;
    struct cfg* set;
    char* maindir;
    char* userdir;
    char* config;
    bool nouserconfig;
    bool nocontroller;
} options;

extern char* curgame;
enum dir {
    DIR_MAIN,
    DIR_GAME,
    #ifndef PSRC_MODULE_SERVER
    DIR_USER,
    DIR_DATA,
    DIR_SAVES,
    DIR_SCREENSHOTS,
    DIR_DOWNLOADS,
    DIR_CUSTOM,
    #endif
    DIR__COUNT
};
extern char* dirs[DIR__COUNT];
extern char* dirdesc[DIR__COUNT][2];

extern char* maindir;
extern char* userdir;
extern char* gamedir; // relative to <maindir>/games
extern char* savedir;

extern struct cfg* config;
extern struct cfg* gameconfig;

bool setGame(const char* n);

bool common_findpv(const char*, int*);

#endif
