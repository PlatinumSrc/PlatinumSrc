#include "version.h"
#include "platform.h"
#include "debug.h"
#include "common.h"

#include "common/logging.h"
#include "common/string.h"
#include "common/filesystem.h"
#include "common/config.h"
#include "common/resource.h"
#include "common/time.h"

#ifndef PSRC_MODULE_SERVER
    #if PLATFORM == PLAT_NXDK || PLATFORM == PLAT_GDK
        #include <SDL.h>
    #elif defined(PSRC_USESDL1)
        #include <SDL/SDL.h>
        #ifdef main
            #undef main
        #endif
    #else
        #include <SDL2/SDL.h>
    #endif
    #if PLATFORM == PLAT_GDK
        #include <SDL_main.h>
    #endif
#endif

#if (PLATFLAGS & PLATFLAG_UNIXLIKE) || PLATFORM == PLAT_WIN32
    #include <signal.h>
#endif
#include <string.h>
#if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #include <unistd.h>
#endif
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#if PLATFORM == PLAT_NXDK
    #include <xboxkrnl/xboxkrnl.h>
    #include <winapi/winnt.h>
    #include <hal/video.h>
#elif (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #include <windows.h>
#elif PLATFORM == PLAT_EMSCR
    #include <emscripten.h>
#elif PLATFORM == PLAT_DREAMCAST
    #include <kos.h>
    #include <dirent.h>
    #define PSRC_KOS_INIT_FLAGS (INIT_IRQ | INIT_NET | INIT_DEFAULT_ARCH | INIT_NO_DCLOAD)
    #if DEBUG(0)
    KOS_INIT_FLAGS(PSRC_KOS_INIT_FLAGS);
    #else
    KOS_INIT_FLAGS(PSRC_KOS_INIT_FLAGS | INIT_QUIET);
    #endif
    #undef PSRC_KOS_INIT_FLAGS
#elif PLATFORM == PLAT_3DS
    #include <3ds.h>
    #include <dirent.h>
#elif PLATFORM == PLAT_WII || PLATFORM == PLAT_GAMECUBE
    #include <fat.h>
    #include <dirent.h>
#endif

#include "glue.h"

#ifndef PSRC_DEFAULTGAME
    #define PSRC_DEFAULTGAME default
#endif

#define _STR(x) #x
#define STR(x) _STR(x)

#if PLATFORM == PLAT_NXDK && !defined(PSRC_NOMT)
static thread_t watchdogthread;
static volatile bool killwatchdog;
static void* watchdog(struct thread_data* td) {
    plog(LL_INFO, "Watchdog armed for %u seconds", (unsigned)td->args);
    uint64_t t = altutime() + (uint64_t)(td->args) * 1000000;
    while (t > altutime()) {
        if (killwatchdog) {
            killwatchdog = false;
            plog(LL_INFO, "Watchdog cancelled");
            return NULL;
        }
        yield();
    }
    HalReturnToFirmware(HalRebootRoutine);
    return NULL;
}
static void armWatchdog(unsigned sec) {
    createThread(&watchdogthread, "watchdog", watchdog, (void*)sec);
}
static void cancelWatchdog(void) {
    killwatchdog = true;
    destroyThread(&watchdogthread, NULL);
}
static void rearmWatchdog(unsigned sec) {
    killwatchdog = true;
    destroyThread(&watchdogthread, NULL);
    createThread(&watchdogthread, "watchdog", watchdog, (void*)sec);
}
#endif

#if defined(PSRC_MODULE_ENGINE)
    #ifndef PSRC_DEFAULTLOGO
        #define PSRC_DEFAULTLOGO internal:icons/engine
    #endif
    #include "main/engine.c"
#elif defined(PSRC_MODULE_SERVER)
    #ifndef PSRC_DEFAULTLOGO
        #define PSRC_DEFAULTLOGO internal:icons/server
    #endif
    #include "main/server.c"
#elif defined(PSRC_MODULE_EDITOR)
    #ifndef PSRC_DEFAULTLOGO
        #define PSRC_DEFAULTLOGO internal:icons/editor
    #endif
    #include "main/editor.c"
#endif

#if (PLATFLAGS & PLATFLAG_UNIXLIKE) || PLATFORM == PLAT_WIN32
static void sigh_log(int l, char* name, char* msg) {
    if (msg) {
        plog(l, "Received signal: %s; %s", name, msg);
    } else {
        plog(l, "Received signal: %s", name);
    }
}
#ifndef PSRC_USESDL1
static void sigh_cb_addstr(struct charbuf* b, const char* s) {
    char c;
    while ((c = *s)) {
        if (c == ' ') {
            cb_add(b, '+');
        } else if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
            cb_add(b, c);
        } else {
            cb_add(b, '%');
            char tmp[3];
            sprintf(tmp, "%02X", c);
            cb_add(b, tmp[0]);
            cb_add(b, tmp[1]);
        }
        ++s;
    }
}
#endif
static void sigh(int sig) {
    signal(sig, sigh);
    #if defined(_POSIX_VERSION) && _POSIX_VERSION >= 200809L
    char* signame = strsignal(sig);
    #else
    char signame[12];
    snprintf(signame, sizeof(signame), "%d", sig);
    #endif
    switch (sig) {
        case SIGINT:
            if (quitreq > 0) {
                sigh_log(LL_WARN, signame, "Graceful exit already requested; Forcing exit...");
                exit(1);
            } else {
                sigh_log(LL_INFO, signame, "Requesting graceful exit...");
                ++quitreq;
            }
            break;
        case SIGTERM:
        #ifdef SIGQUIT
        case SIGQUIT:
        #endif
            sigh_log(LL_WARN, signame, "Forcing exit...");
            exit(1);
            break;
        case SIGSEGV:
        #ifdef SIGABRT
        case SIGABRT:
        #endif
        #ifdef SIGBUS
        case SIGBUS:
        #endif
        #ifdef SIGFPE
        case SIGFPE:
        #endif
        #ifdef SIGILL
        case SIGILL:
        #endif
        {
            plog(LL_CRIT, "Received signal: %s", signame);
            #ifndef PSRC_USESDL1
            SDL_MessageBoxButtonData btndata[] = {
                {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes"},
                {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No"}
            };
            char msgdata[256] = "Received signal: ";
            strcat(msgdata, signame);
            strcat(msgdata, ".\nTry to submit a bug report on GitHub?");
            SDL_MessageBoxData boxdata = {
                .flags = SDL_MESSAGEBOX_ERROR,
                .title = "Fatal crash",
                .message = msgdata,
                .numbuttons = sizeof(btndata) / sizeof(*btndata),
                .buttons = btndata
            };
            int btn = 0;
            SDL_ShowMessageBox(&boxdata, &btn);
            if (btn) {
                struct charbuf cb;
                cb_init(&cb, 4096);
                cb_addstr(&cb, "https://github.com/PlatinumSrc/PlatinumSrc/issues/new?labels=bug&title=Automatic%20crash%20report&body=");
                sigh_cb_addstr(&cb, verstr);
                sigh_cb_addstr(&cb, "\n");
                sigh_cb_addstr(&cb, platstr);
                sigh_cb_addstr(&cb, "\nGame: ");
                sigh_cb_addstr(&cb, gameinfo.dir);
                if (logpath) {
                    sigh_cb_addstr(&cb, "\n\n***!!! PLEASE UPLOAD THE LOG AT `");
                    sigh_cb_addstr(&cb, logpath);
                    sigh_cb_addstr(&cb, "` HERE AND DELETE THIS TEXT !!!***");
                }
                puts(cb_peek(&cb));
                #if PLATFORM == PLAT_WIN32
                ShellExecute(NULL, NULL, cb_peek(&cb), NULL, NULL, SW_NORMAL);
                #elif PLATFORM == PLAT_ANDROID
                execlp("am", "am", "start", "-a", "android.intent.action.VIEW", "-d", cb_peek(&cb), NULL);
                #else
                execlp("xdg-open", "xdg-open", cb_peek(&cb), NULL);
                execlp("open", "open", cb_peek(&cb), NULL);
                #endif
            }
            #endif
            exit(1);
        } break;
        default:
            sigh_log(LL_WARN, signame, NULL);
            break;
    }
}
#endif

static int bootstrap(void) {
    puts(verstr);
    puts(platstr);
    #if PLATFORM == PLAT_LINUX
    setenv("SDL_VIDEODRIVER", "wayland", false);
    #elif PLATFORM == PLAT_NXDK
    pb_print("%s\n", verstr);
    pb_print("%s\n", platstr);
    pbgl_swap_buffers();
    #endif

    if (!initLogging()) {
        fputs("{X}: Failed to init logging\n", stderr);
        return 1;
    }

    char* tmp = (options.config) ? strpath(options.config) : mkpath(dirs[DIR_INTERNAL], "config.cfg", NULL);
    {
        struct datastream ds;
        bool ret = ds_openfile(tmp, 0, &ds);
        free(tmp);
        if (ret) {
            cfg_open(&ds, &config);
            ds_close(&ds);
        } else {
            plog(LL_WARN, "Failed to load main config");
            cfg_open(NULL, &config);
        }
    }
    if (options.set__setup) cfg_mergemem(&config, &options.set, true);

    {
        struct charbuf err;
        cb_init(&err, 256);
        if (options.game) {
            if (!setGame(options.game, true, &err)) {
                plog(LL_CRIT | LF_MSGBOX, "%s", cb_peek(&err));
                cb_dump(&err);
                return 1;
            }
            free(options.game);
            options.game = NULL;
        } else if ((tmp = cfg_getvar(&config, NULL, "defaultgame"))) {
            if (!setGame(tmp, false, &err)) {
                plog(LL_CRIT | LF_MSGBOX, "%s", cb_peek(&err));
                cb_dump(&err);
                return 1;
            }
            free(tmp);
        } else {
            plog(LL_WARN, "No default game specified, falling back to '%s'", STR(PSRC_DEFAULTGAME));
            if (!setGame(STR(PSRC_DEFAULTGAME), false, &err)) {
                plog(LL_CRIT | LF_MSGBOX, "%s", cb_peek(&err));
                cb_dump(&err);
                return 1;
            }
        }
        cb_dump(&err);
    }

    #ifndef PSRC_MODULE_SERVER
    if (dirs[DIR_USER]) {
        if (!options.nouserconfig) {
            tmp = mkpath(dirs[DIR_USER], "config.cfg", NULL);
            struct datastream ds;
            bool ret = ds_openfile(tmp, 0, &ds);
            free(tmp);
            if (ret) {
                cfg_merge(&config, &ds, true);
                ds_close(&ds);
                if (options.set__setup) cfg_mergemem(&config, &options.set, true);
            } else {
                plog(LL_WARN, "Failed to load user config");
            }
        }
    }
    #endif

    plog(LL_INFO, "Initializing resource manager...");
    if (!initRcMgr()) {
        plog(LL_CRIT | LF_FUNCLN, "Failed to init resource manager");
        return 1;
    }

    {
        tmp = cfg_getvar(&config, NULL, "mods");
        if (options.mods) {
            if (tmp) {
                int ct1, ct2;
                char** l1;
                char** l2;
                l1 = splitstrlist(tmp, ',', false, &ct1);
                free(tmp);
                l2 = splitstrlist(options.mods, ',', false, &ct2);
                free(options.mods);
                options.mods = NULL;
                l1 = realloc(l1, (ct1 + ct2) * sizeof(*l1));
                for (int i = 0; i < ct2; ++i) {
                    l1[i + ct1] = l2[i];
                }
                free(*l2);
                free(l2);
                free(*l1);
                free(l1);
            } else {
                int ct;
                char** l = splitstrlist(options.mods, ',', false, &ct);
                free(options.mods);
                options.mods = NULL;
                loadMods((const char* const *)l, ct);
                free(*l);
                free(l);
            }
        } else if (tmp) {
            int ct;
            char** l = splitstrlist(tmp, ',', false, &ct);
            free(tmp);
            loadMods((const char* const *)l, ct);
            free(*l);
            free(l);
        }
        int ct;
        struct modinfo* data = queryMods(&ct);
        if (data) {
            plog(LL_INFO, "Mod info:");
            for (int i = 0; i < ct; ++i) {
                plog(LL_INFO, "  %s (%s)", data[i].name, data[i].dir);
                plog(LL_INFO, "    Path: %s", data[i].path);
                if (data[i].author) plog(LL_INFO, "    Author: %s", data[i].author);
                if (data[i].desc) plog(LL_INFO, "    Description: %s", data[i].desc);
                char s[16];
                vertostr(&data[i].version, s);
                plog(LL_INFO, "    Version: %s", s);
            }
            freeModList(data);
        } else {
            plog(LL_INFO, "No mods laoded");
        }
    }

    #if 0
    struct rcls l;
    if (lsRc("textures/env", &l)) {
        for (int ri = 0; ri < RC__DIR + 1; ++ri) {
            int ct = l.count[ri];
            printf("TYPE[%d] (%d):\n", ri, ct);
            for (int i = 0; i < ct; ++i) {
                struct rcls_file* f = &l.files[ri][i];
                printf("  name: {%s}, crc: [%08X]\n", f->name, f->namecrc);
            }
        }
        freeRcls(&l);
    } else {
        puts("LIST FAILED");
    }
    #endif

    return 0;
}
static void unstrap(void) {
    plog(LL_INFO, "Quitting resource manager...");
    quitRcMgr();

    #ifndef PSRC_MODULE_SERVER
    if (dirs[DIR_USER]) {
        char* tmp = mkpath(dirs[DIR_USER], "config.cfg", NULL);
        //cfg_write(config, tmp);
        free(tmp);
        cfg_close(&config);
    }
    #endif

    #if PLATFORM == PLAT_NXDK && !defined(PSRC_NOMT)
    armWatchdog(5);
    #endif
    SDL_Quit();
    #if PLATFORM == PLAT_NXDK && !defined(PSRC_NOMT)
    cancelWatchdog();
    #endif

    plog(LL_INFO, "Done");
}

#if PLATFORM == PLAT_EMSCR
volatile bool issyncfsok = false;
void __attribute__((used)) syncfsok(void) {issyncfsok = true;}
static void emscrexit(void* arg) {
    exit((int)(uintptr_t)arg);
}
static void emscrmain(void) {
    static bool doloop = false;
    static bool syncmsg = false;
    if (doloop) {
        if (!quitreq) {
            doLoop();
        } else {
            static bool doexit = false;
            if (doexit) {
                if (!syncmsg) {
                    puts("Waiting on RAM to disk FS.syncfs...");
                    syncmsg = true;
                }
                if (issyncfsok) {
                    emscripten_cancel_main_loop();
                    emscripten_async_call(emscrexit, (void*)(uintptr_t)0, -1);
                    puts("Done");
                }
                return;
            }
            quitLoop();
            unstrap();
            EM_ASM(
                FS.syncfs(false, function (e) {
                    if (e) console.error("FS.syncfs:", e);
                    ccall("syncfsok");
                });
            );
            doexit = true;
        }
    } else {
        if (!syncmsg) {
            puts("Waiting on disk to RAM FS.syncfs...");
            syncmsg = true;
        }
        if (!issyncfsok) return;
        issyncfsok = false;
        syncmsg = false;
        static int ret;
        ret = bootstrap();
        if (!ret) {
            ret = initLoop();
            if (!ret) {
                doloop = true;
                return;
            }
            unstrap();
        }
        emscripten_cancel_main_loop();
        emscripten_async_call(emscrexit, (void*)(uintptr_t)1, -1);
    }
}
#elif PLATFORM == PLAT_GDK
#define main SDL_main
#endif

int main(int argc, char** argv) {
    makeVerStrs();

    int ret;
    if (argc > 1) {
        ret = parseargs(argc - 1, argv + 1);
        if (ret >= 0) return ret;
    }

    #if (PLATFLAGS & PLATFLAG_UNIXLIKE) || PLATFORM == PLAT_WIN32
        signal(SIGINT, sigh);
        signal(SIGTERM, sigh);
        #ifdef SIGQUIT
        signal(SIGQUIT, sigh);
        #endif
        #ifdef SIGUSR1
        signal(SIGUSR1, SIG_IGN);
        #endif
        #ifdef SIGUSR2
        signal(SIGUSR2, SIG_IGN);
        #endif
        #ifdef SIGPIPE
        signal(SIGPIPE, SIG_IGN);
        #endif
        #if !DEBUG(0)
            signal(SIGSEGV, sigh);
            #ifdef SIGABRT
            signal(SIGABRT, sigh);
            #endif
            #ifdef SIGBUS
            signal(SIGBUS, sigh);
            #endif
            #ifdef SIGFPE
            signal(SIGFPE, sigh);
            #endif
            #ifdef SIGILL
            signal(SIGILL, sigh);
            #endif
        #endif
    #endif
    #if PLATFORM == PLAT_WIN32
        TIMECAPS tc;
        UINT tmrres = 1;
        if (timeGetDevCaps(&tc, sizeof(tc)) != TIMERR_NOERROR) {
            if (tmrres < tc.wPeriodMin) tmrres = tc.wPeriodMin;
            else if (tmrres > tc.wPeriodMax) tmrres = tc.wPeriodMax;
        }
        timeBeginPeriod(tmrres);
    #elif PLATFORM == PLAT_WII
        fatInitDefault();
    #endif
    #if PLATFORM == PLAT_NXDK
        perfctfreq = KeQueryPerformanceFrequency();
        XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    #elif (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        QueryPerformanceFrequency(&perfctfreq);
        while (!(perfctfreq.QuadPart % 10) && !(perfctmul % 10)) {
            perfctfreq.QuadPart /= 10;
            perfctmul /= 10;
        }
    #endif

    setupBaseDirs();
    #if PLATFORM != PLAT_EMSCR
        ret = bootstrap();
        if (!ret) {
            ret = initLoop();
            if (!ret) {
                while (!quitreq) {
                    doLoop();
                }
                quitLoop();
            }
            unstrap();
        }
    #else
        EM_ASM(
            FS.mkdir('/libsdl');
            FS.mount(IDBFS, {}, '/libsdl');
            FS.syncfs(true, function (e) {
                if (e) console.error("FS.syncfs:", e);
                ccall("syncfsok");
            });
        );
        emscripten_set_main_loop(emscrmain, -1, true);
        ret = 0;
    #endif

    #if PLATFORM == PLAT_WIN32
        timeEndPeriod(tmrres);
    #elif PLATFORM == PLAT_NXDK
        if (ret) Sleep(5000);
        pbgl_shutdown();
        HalReturnToFirmware(HalQuickRebootRoutine);
    #elif PLATFORM == PLAT_DREAMCAST
        arch_menu();
    #endif

    return ret;
}
