#ifndef PSRC_COMMON_H
#define PSRC_COMMON_H

#include "versioning.h"
#include "string.h"

#include "common/config.h"

#include <stdbool.h>

extern unsigned quitreq;

extern struct options {
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
} options;

extern struct gameinfo {
    char* dir;
    char* name;
    char* author;
    char* desc;
    #ifndef PSRC_MODULE_SERVER
    char* icon;
    #endif
    struct version version;
    struct version minver;
    #ifndef PSRC_MODULE_SERVER
    char* userdir;
    #endif
} gameinfo;

enum dir {
    DIR_MAIN,        // instance dir
    DIR_INTERNAL,    // internal data dir; 'internal' in the main dir
    DIR_INTERNALRC,  // internal:/ resources; 'resources' in the internal data dir
    DIR_GAMES,       // game dirs and game:/ resources; 'games' in the main dir
    DIR_MODS,        // mods and mod:/ resources; 'mods' in the main dir
    DIR_GAME,        // game dir
    #ifndef PSRC_MODULE_SERVER
    DIR_USER,        // user data dir; NULL if there is no suitable user file storage
    DIR_USERRC,      // user:/ resources; 'resources' in the user data dir; NULL if the user data dir is NULL
    DIR_USERMODS,    // user mods and mod:/ resources; 'mods' in the user data dir; NULL if the user data dir is NULL
    DIR_SCREENSHOTS, // 'screenshots' in the user data dir; NULL if the user data dir is NULL or there is no writeable
                     // filesystem suitable for large files
    DIR_SAVES,       // save location; platform-specific dir or 'saves' dir in the user data dir; NULL if there is no
                     // writable filesystem sutiable for saves
    #endif
    DIR_CONFIGS,     // 'configs' in the user data dir, or in 'data/<userdir name>/' in the main dir if the user data
                     // dir is NULL
    DIR_DATABASES,   // 'databases' in the user data dir, or in 'data/<userdir name>/' in the main dir if the user data
                     // dir is NULL
    #ifndef PSRC_MODULE_SERVER
    DIR_SVDL,        // typically 'server' in 'downloads' in the user data dir; NULL if the user data dir is NULL
    DIR_PLDL,        // typically 'player' in 'downloads' in the user data dir; NULL if the user data dir is NULL
    #endif
    DIR__COUNT
};
extern char* dirs[DIR__COUNT];
extern char* dirdesc[DIR__COUNT];

extern struct cfg config;

void setupBaseDirs(void);
bool setGame(const char*, bool maybepath, struct charbuf* err);

#endif
