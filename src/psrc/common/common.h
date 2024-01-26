#ifndef PSRC_COMMON_COMMON_H
#define PSRC_COMMON_COMMON_H

#include "config.h"

struct options {
    char* game;
    char* maindir;
    char* userdir;
    struct cfg* set;
    char* config;
    bool nouserconfig;
    char* mods;
    char* icon;
};

extern int quitreq;

extern char* maindir;
extern char* userdir;

extern char* gamedir; // relative to <maindir>/games
extern char* savedir;

extern struct cfg* config;
extern struct cfg* gameconfig;

extern struct options options;

#endif
