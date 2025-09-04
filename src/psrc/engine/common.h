#ifndef PSRC_ENGINE_COMMON_H
#define PSRC_ENGINE_COMMON_H

#include <stdbool.h>

#include "../string.h"
#include "../common/config.h"
#include "../common/pbasic.h"

extern struct engine {
    struct {
        struct pbasic pb;
        struct pb_compiler_opt compopt;
    } pb;
    struct {
        char* game;
        char* mods;
        #ifndef PSRC_MODULE_SERVER
        char* icon;
        #endif
        bool set__setup;
        struct cfg set;
        char* maindir;
        char* gamesdir;
        char* modsdir;
        #ifndef PSRC_MODULE_SERVER
        char* userdir;
        #endif
        char* config;
        #ifndef PSRC_MODULE_SERVER
        bool nouserconfig;
        bool nocontroller;
        #endif
    } opt;
} engine;

void setupBaseDirs(void);
bool setGame(const char*, bool maybepath, struct charbuf* err);

#endif
