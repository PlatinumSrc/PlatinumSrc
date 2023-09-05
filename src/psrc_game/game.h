#ifndef GAME_GAME_H
#define GAME_GAME_H

#include "../psrc_aux/config.h"

extern char* maindir;
extern char* userdir;

extern char* gamedir; // relative to <maindir>/games
extern char* savedir;

extern struct cfg* config;
extern struct cfg* gameconfig;

#endif
