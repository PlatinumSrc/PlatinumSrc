#ifndef PSRC_COMMON_COMMON_H
#define PSRC_COMMON_COMMON_H

#include "../utils/config.h"

extern int quitreq;

extern char* maindir;
extern char* userdir;

extern char* gamedir; // relative to <maindir>/games
extern char* savedir;

extern struct cfg* config;
extern struct cfg* gameconfig;

#endif
