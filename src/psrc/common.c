#include "common.h"
#include "platform.h"
#include "version.h"

#include "common/filesystem.h"
#include "common/logging.h"

#if !defined(PSRC_USESDL1) && PLATFORM != PLAT_3DS && PLATFORM != PLAT_WII && PLATFORM != PLAT_GAMECUBE
    #if PLATFORM == PLAT_NXDK || PLATFORM == PLAT_GDK
        #include <SDL.h>
    #else
        #include <SDL2/SDL.h>
    #endif
#endif
#if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #include <windows.h>
#elif PLATFORM == PLAT_DREAMCAST
    #include <dirent.h>
#endif

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "glue.h"

int quitreq = 0;

struct options options = {0};
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
    "databases",
    #ifndef PSRC_MODULE_SERVER
    "server downloads",
    "player downloads"
    #endif
};

struct cfg config;

void setupBaseDirs(void) {
    if (options.maindir) {
        dirs[DIR_MAIN] = strpath(options.maindir);
    } else {
        #if PLATFORM == PLAT_NXDK
        dirs[DIR_MAIN] = "D:\\";
        #elif PLATFORM == PLAT_DREAMCAST
        DIR* d = opendir("/cd");
        if (d) {
            closedir(d);
            dirs[DIR_MAIN] = "/cd";
        } else {
            dirs[DIR_MAIN] = "/sd/psrc";
        }
        #elif PLATFORM == PLAT_3DS
        dirs[DIR_MAIN] = "sdmc:/3ds/psrc";
        #elif PLATFORM == PLAT_WII
        dirs[DIR_MAIN] = "/apps/psrc";
        #elif PLATFORM == PLAT_GAMECUBE
        dirs[DIR_MAIN] = "/";
        #elif !defined(PSRC_USESDL1)
        char* tmp = SDL_GetBasePath();
        if (tmp) {
            char* tmp2 = tmp;
            tmp = strpath(tmp);
            SDL_free(tmp2);
            dirs[DIR_MAIN] = tmp;
        } else {
            plog(LL_WARN, "Failed to get main directory: %s", SDL_GetError());
            dirs[DIR_MAIN] = ".";
        }
        #elif PLATFORM == PLAT_WIN32
        char* tmp = malloc(MAX_PATH + 1);
        DWORD r = GetModuleFileName(NULL, tmp, MAX_PATH);
        if (r) {
            for (int i = r - 1; i >= 0; --i) {
                if (ispathsep(tmp[i])) {
                    tmp[i] = 0;
                    tmp = realloc(tmp, i + 1);
                    break;
                }
            }
            dirs[DIR_MAIN] = tmp;
        } else {
            free(tmp);
            plog(LL_WARN, "Failed to get main directory");
            dirs[DIR_MAIN] = ".";
        }
        #else // TODO: implement more platforms
        plog(LL_WARN, "Failed to get main directory");
        dirs[DIR_MAIN] = ".";
        #endif
    }
    dirs[DIR_INTERNAL] = mkpath(dirs[DIR_MAIN], "internal", NULL);
    dirs[DIR_INTERNALRC] = mkpath(dirs[DIR_INTERNAL], "resources", NULL);
    dirs[DIR_GAMES] = mkpath(dirs[DIR_MAIN], "games", NULL);
    dirs[DIR_MODS] = mkpath(dirs[DIR_MAIN], "mods", NULL);
}
static inline void logdirs(int until) {
    plog(LL_INFO, "Directories:");
    for (int i = 0; i <= until; ++i) {
        if (dirs[i]) plog(LL_INFO, "  %c%s: %s", toupper(*dirdesc[i]), dirdesc[i] + 1, dirs[i]);
    }
}
bool setGame(const char* g, bool p, struct charbuf* err) {
    if (!*g) return false;
    char* d;
    char* n;
    if (p) {
        {
            const char* g2 = g;
            const char* n2 = NULL;
            char c = *g;
            do {
                ++g2;
                if (ispathsep(c)) n2 = g2;
            } while ((c = *g2));
            if (n2 || (g[0] == '.' && (!g[1] || (g[1] == '.' && !g[2])))) {
                d = strpath(g);
            } else {
                d = mkpath(dirs[DIR_GAMES], g, NULL);
            }
        }
    } else {
        const char* g2 = g;
        char c = *g;
        do {
            if (ispathsep(c)) {
                cb_addstr(err, "Invalid game dir '");
                cb_addstr(err, g);
                cb_add(err, '\'');
                return false;
            }
            ++g2;
        } while ((c = *g2));
        d = mkpath(dirs[DIR_GAMES], g, NULL);
    }
    {
        char* tmp = realpath(d, NULL);
        if (tmp) {
            free(d);
            d = tmp;
        } else {
            plog(LL_WARN, "Could not find realpath of '%s'", d);
            tmp = d;
        }
        tmp = strpath(tmp);
        n = basepathname(tmp);
        if (!n[0] || (ispathsep(n[0]) && !n[1]) || (n[0] == '.' && (!n[1] || (n[1] == '.' && !n[2])))) {
            cb_addstr(err, "Invalid game dir '");
            cb_addstr(err, d);
            cb_add(err, '\'');
            free(d);
            free(tmp);
            return false;
        }
        n = strdup(n);
        free(tmp);
    }
    if (isFile(d)) {
        logdirs(DIR_GAME);
        cb_addstr(err, "Could not find game directory '");
        cb_addstr(err, d);
        cb_add(err, '\'');
        free(d);
        free(n);
        return false;
    }
    {
        char* tmp = mkpath(d, "game.cfg", NULL);
        struct cfg gameconfig;
        {
            struct datastream ds;
            bool ret = ds_openfile(tmp, 0, &ds);
            free(tmp);
            if (!ret) {
                cb_addstr(err, "Could not read game.cfg in '");
                cb_addstr(err, d);
                cb_add(err, '\'');
                free(d);
                free(n);
                return false;
            }
            cfg_open(&ds, &gameconfig);
            ds_close(&ds);
        }
        free(gameinfo.dir);
        gameinfo.dir = n;
        tmp = cfg_getvar(&gameconfig, NULL, "name");
        free(gameinfo.name);
        if (tmp && *tmp) {
            gameinfo.name = tmp;
        } else {
            gameinfo.name = strdup(n);
            free(tmp);
        }
        titlestr = gameinfo.name;
        free(gameinfo.author);
        gameinfo.author = cfg_getvar(&gameconfig, NULL, "author");
        free(gameinfo.desc);
        gameinfo.desc = cfg_getvar(&gameconfig, NULL, "desc");
        free(gameinfo.icon);
        gameinfo.icon = cfg_getvar(&gameconfig, NULL, "icon");
        tmp = cfg_getvar(&gameconfig, NULL, "version");
        if (tmp) {
            if (!strtover(tmp, &gameinfo.version)) gameinfo.version = MKVER_8_16_8(1, 0, 0);
            free(tmp);
        } else {
            gameinfo.version = MKVER_8_16_8(1, 0, 0);
        }
        tmp = cfg_getvar(&gameconfig, NULL, "minver");
        if (tmp) {
            if (!strtover(tmp, &gameinfo.minver)) gameinfo.minver = gameinfo.version;
            free(tmp);
        } else {
            gameinfo.minver = gameinfo.version;
        }
        #ifndef PSRC_MODULE_SERVER
        free(gameinfo.userdir);
        tmp = cfg_getvar(&gameconfig, NULL, "userdir");
        if (tmp) {
            gameinfo.userdir = restrictpath(tmp, "/", PATHSEP, '_');
            free(tmp);
        } else {
            gameinfo.userdir = sanfilename(gameinfo.name, '_');
        }
        #endif
        cfg_close(&gameconfig);
    }
    free(dirs[DIR_GAME]);
    dirs[DIR_GAME] = d;
    #ifndef PSRC_MODULE_SERVER
        #if PLATFORM != PLAT_DREAMCAST
            #if PLATFORM != PLAT_NXDK
                free(dirs[DIR_USER]);
                if (options.userdir) {
                    dirs[DIR_USER] = strpath(options.userdir);
                } else {
                    #if PLATFORM == PLAT_3DS || PLATFORM == PLAT_WII || PLATFORM == PLAT_GAMECUBE || PLATFORM == PLAT_EMSCR
                        dirs[DIR_USER] = mkpath(dirs[DIR_MAIN], "data", gameinfo.userdir, NULL);
                    #elif !defined(PSRC_USESDL1)
                        char* tmp;
                        tmp = SDL_GetPrefPath(NULL, gameinfo.userdir);
                        if (tmp) {
                            char* tmp2 = tmp;
                            dirs[DIR_USER] = strpath(tmp);
                            SDL_free(tmp2);
                        } else {
                            plog(LL_WARN, "Failed to get user directory: %s", SDL_GetError());
                            dirs[DIR_USER] = mkpath(dirs[DIR_MAIN], "data", gameinfo.userdir, NULL);
                        }
                    #elif PLATFORM == PLAT_WIN32
                        char* tmp = getenv("AppData");
                        if (tmp) {
                            dirs[DIR_USER] = mkpath(tmp, gameinfo.userdir, NULL);
                        } else {
                            plog(LL_WARN, "Failed to get user directory");
                            dirs[DIR_USER] = mkpath(dirs[DIR_MAIN], "data", n, NULL);
                        }
                    #else
                        plog(LL_WARN, "Failed to get user directory");
                        dirs[DIR_USER] = mkpath(dirs[DIR_MAIN], "data", gameinfo.userdir, NULL);
                    #endif
                }
            #else
                {
                    struct charbuf cb;
                    cb_init(&cb, 32);
                    cb_addstr(&cb, "data - ");
                    cb_addstr(&cb, n);
                    free(dirs[DIR_USER]);
                    dirs[DIR_USER] = mkpath("E:\\UDATA", titleidstr, cb_peek(&cb), NULL);
                    cb_clear(&cb);
                    cb_addstr(&cb, "svdl - ");
                    cb_addstr(&cb, n);
                    free(dirs[DIR_SVDL]);
                    dirs[DIR_SVDL] = mkpath("E:\\UDATA", titleidstr, cb_peek(&cb), NULL);
                    cb_clear(&cb);
                    cb_addstr(&cb, "pldl - ");
                    cb_addstr(&cb, n);
                    free(dirs[DIR_PLDL]);
                    dirs[DIR_PLDL] = mkpath("E:\\UDATA", titleidstr, cb_peek(&cb), NULL);
                    cb_dump(&cb);
                }
            #endif
            free(dirs[DIR_USERRC]);
            dirs[DIR_USERRC] = mkpath(dirs[DIR_USER], "resources", NULL);
            free(dirs[DIR_USERMODS]);
            dirs[DIR_USERMODS] = mkpath(dirs[DIR_USER], "mods", NULL);
            free(dirs[DIR_SCREENSHOTS]);
            dirs[DIR_SCREENSHOTS] = mkpath(dirs[DIR_USER], "screenshots", NULL);
            #if PLATFORM != PLAT_NXDK
                free(dirs[DIR_SAVES]);
                dirs[DIR_SAVES] = mkpath(dirs[DIR_USER], "saves", NULL);
                free(dirs[DIR_SVDL]);
                dirs[DIR_SVDL] = mkpath(dirs[DIR_USER], "downloads", "server", NULL);
                free(dirs[DIR_PLDL]);
                dirs[DIR_PLDL] = mkpath(dirs[DIR_USER], "downloads", "player", NULL);
            #else
                free(dirs[DIR_SAVES]);
                dirs[DIR_SAVES] = mkpath("E:\\UDATA", titleidstr, NULL);
            #endif
            free(dirs[DIR_DATABASES]);
            dirs[DIR_DATABASES] = mkpath(dirs[DIR_MAIN], "databases", gameinfo.userdir, NULL);
            for (enum dir i = DIR_USER; i < DIR__COUNT; ++i) {
                if (dirs[i]) {
                    if (i != DIR_SAVES && !dirs[DIR_USER]) {
                        free(dirs[i]);
                        dirs[i] = NULL;
                    } else if (!md(dirs[i])) {
                        plog(LL_ERROR, "Failed to make %s directory", dirdesc[i]);
                        free(dirs[i]);
                        dirs[i] = NULL;
                    }
                }
            }
            char* tmp = mkpath(dirs[DIR_USER], "log.txt", NULL);
            if (!plog_setfile(tmp)) {
                plog(LL_WARN, "Failed to set log file");
            }
            free(tmp);
            #if PLATFORM == PLAT_NXDK
                tmp = mkpath("E:\\UDATA", titleidstr, "TitleMeta.xbx", NULL);
                if (isFile(tmp) < 0) {
                    FILE* f = fopen(tmp, "w");
                    if (f) {
                        fputs("TitleName=PlatinumSrc\n", f);
                        fclose(f);
                    } else {
                        plog(LL_WARN, "Failed to create TitleMeta.xbx");
                    }
                }
                free(tmp);
                // TODO: copy icon maybe?
                if (dirs[DIR_USER]) {
                    tmp = mkpath(dirs[DIR_USER], "SaveMeta.xbx", NULL);
                    if (isFile(tmp) < 0) {
                        FILE* f = fopen(tmp, "w");
                        if (f) {
                            fputs("Name=", f);
                            fputs(gameinfo.name, f);
                            fputs(" user data\n", f);
                            fclose(f);
                        } else {
                            plog(LL_WARN, "Failed to create user data SaveMeta.xbx");
                        }
                    }
                    free(tmp);
                }
                if (dirs[DIR_SVDL]) {
                    tmp = mkpath(dirs[DIR_SVDL], "SaveMeta.xbx", NULL);
                    if (isFile(tmp) < 0) {
                        FILE* f = fopen(tmp, "w");
                        if (f) {
                            fputs("Name=", f);
                            fputs(gameinfo.name, f);
                            fputs(" server content\n", f);
                            fclose(f);
                        } else {
                            plog(LL_WARN, "Failed to create server content SaveMeta.xbx");
                        }
                    }
                    free(tmp);
                }
                if (dirs[DIR_PLDL]) {
                    tmp = mkpath(dirs[DIR_PLDL], "SaveMeta.xbx", NULL);
                    if (isFile(tmp) < 0) {
                        FILE* f = fopen(tmp, "w");
                        if (f) {
                            fputs("Name=", f);
                            fputs(gameinfo.name, f);
                            fputs(" player content\n", f);
                            fclose(f);
                        } else {
                            plog(LL_WARN, "Failed to create player content SaveMeta.xbx");
                        }
                    }
                    free(tmp);
                }
            #endif
        #endif
    #else
        free(dirs[DIR_DATABASES]);
        dirs[DIR_DATABASES] = mkpath(dirs[DIR_MAIN], "databases", d, NULL);
    #endif
    logdirs(DIR__COUNT - 1);
    return true;
}

bool common_findpv(const char* n, int* d) {
    if (*n) {
        if (*n == 'm' || *n == 'M') {
            ++n;
            if (!strcasecmp(n, "odule")) {
                #if defined(PSRC_MODULE_SERVER)
                    *d = 1;
                #elif defined(PSRC_MODULE_EDITOR)
                    *d = 2;
                #else
                    *d = 0;
                #endif
                return true;
            } else if (!strcasecmp(n, "odule_client")) {
                *d = 0; return true;
            } else if (!strcasecmp(n, "odule_server")) {
                *d = 1; return true;
            } else if (!strcasecmp(n, "odule_editor")) {
                *d = 2; return true;
            }
        } else if (*n == 'p' || *n == 'P') {
            ++n;
            if (!strcasecmp(n, "latform")) {
                *d = PLATFORM; return true;
            } else if (!strcasecmp(n, "lat_3ds")) {
                *d = PLAT_3DS; return true;
            } else if (!strcasecmp(n, "lat_android")) {
                *d = PLAT_ANDROID; return true;
            } else if (!strcasecmp(n, "lat_dreamcast")) {
                *d = PLAT_DREAMCAST; return true;
            } else if (!strcasecmp(n, "lat_emscr")) {
                *d = PLAT_EMSCR; return true;
            } else if (!strcasecmp(n, "lat_freebsd")) {
                *d = PLAT_FREEBSD; return true;
            } else if (!strcasecmp(n, "lat_gamecube")) {
                *d = PLAT_GAMECUBE; return true;
            } else if (!strcasecmp(n, "lat_gdk")) {
                *d = PLAT_GDK; return true;
            } else if (!strcasecmp(n, "lat_linux")) {
                *d = PLAT_LINUX; return true;
            } else if (!strcasecmp(n, "lat_macos")) {
                *d = PLAT_MACOS; return true;
            } else if (!strcasecmp(n, "lat_netbsd")) {
                *d = PLAT_NETBSD; return true;
            } else if (!strcasecmp(n, "lat_openbsd")) {
                *d = PLAT_OPENBSD; return true;
            } else if (!strcasecmp(n, "lat_ps2")) {
                *d = PLAT_PS2; return true;
            } else if (!strcasecmp(n, "lat_unix")) {
                *d = PLAT_UNIX; return true;
            } else if (!strcasecmp(n, "lat_wii")) {
                *d = PLAT_WII; return true;
            } else if (!strcasecmp(n, "lat_win32")) {
                *d = PLAT_WIN32; return true;
            } else if (!strcasecmp(n, "lat_nxdk")) {
                *d = PLAT_NXDK; return true;
            } else if (!strcasecmp(n, "latflags")) {
                *d = PLATFLAGS; return true;
            } else if (!strcasecmp(n, "latflag_unixlike")) {
                *d = PLATFLAG_UNIXLIKE; return true;
            } else if (!strcasecmp(n, "latflag_windowslike")) {
                *d = PLATFLAG_WINDOWSLIKE; return true;
            }
        }
    }
    return false;
}
