#ifndef PSRC_COMMON_H
#define PSRC_COMMON_H

#include "versioning.h"
#include "version.h"
#include "string.h"
#include "debug.h"

#include "common/config.h"

extern unsigned quitreq;

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

#define PSRC_COMMON_PBPREPROCVARS_BASE \
    {.name = "psrc:build", .namecrc = 0xB52F9B10, .data.type = PB_PREPROC_TYPE_U32, .data.u32 = PSRC_BUILD},\
    {.name = "psrc:module:engine", .namecrc = 0x18735912, .data.type = PB_PREPROC_TYPE_U8, .data.u8 = 0},\
    {.name = "psrc:module:server", .namecrc = 0xAAB69669, .data.type = PB_PREPROC_TYPE_U8, .data.u8 = 1},\
    {.name = "psrc:module:editor", .namecrc = 0x3C2AB225, .data.type = PB_PREPROC_TYPE_U8, .data.u8 = 2}
#define PSRC_COMMON_PBPREPROCVARCT_BASE (4)

#if DEBUG(0)
    #define PSRC_COMMON_PBPREPROCVARS_DEBUG \
        {.name = "psrc:debug", .namecrc = 0x642A2E6C, .data.type = PB_PREPROC_TYPE_U8, .data.u8 = PSRC_DBGLVL},
    #define PSRC_COMMON_PBPREPROCVARCT_DEBUG (1)
#else
    #define PSRC_COMMON_PBPREPROCVARS_DEBUG
    #define PSRC_COMMON_PBPREPROCVARCT_DEBUG (0)
#endif

#define PSRC_COMMON_PBPREPROCVARS \
    PSRC_COMMON_PBPREPROCVARS_DEBUG\
    PSRC_COMMON_PBPREPROCVARS_BASE
#define PSRC_COMMON_PBPREPROCVARCT (PSRC_COMMON_PBPREPROCVARCT_DEBUG + PSRC_COMMON_PBPREPROCVARCT_BASE)

#endif
