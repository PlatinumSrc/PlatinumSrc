#include "common.h"

unsigned quitreq = 0;

struct gameinfo gameinfo = {0};

char* dirs[DIR__COUNT];
char* dirdesc[DIR__COUNT] = {
    "main",
    "internal data",
    "internal resources",
    "games",
    "mods",
    "current game",
    #ifndef PSRC_MODULE_SERVER
    "user data",
    "user resources",
    "user mods",
    "screenshots",
    "saves",
    #endif
    "configs",
    "databases",
    #ifndef PSRC_MODULE_SERVER
    "server downloads",
    "player downloads"
    #endif
};

struct cfg config;
