#include "common/logging.h"
#include "common/string.h"
#include "common/filesystem.h"
#include "common/config.h"
#include "common/resource.h"
#include "common/common.h"
#include "common/time.h"
#include "common/arg.h"

#include "version.h"
#include "platform.h"
#include "debug.h"
#include "loop.h"

#if PLATFORM != PLAT_NXDK
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#if PLATFORM == PLAT_WIN32
    #include <windows.h>
#elif PLATFORM == PLAT_EMSCR
    #include <emscripten.h>
#elif PLATFORM == PLAT_NXDK
    #include <xboxkrnl/xboxkrnl.h>
    #include <winapi/winnt.h>
    #include <hal/video.h>
    #include <pbkit/pbkit.h>
    #include <pbgl.h>
    #include <GL/gl.h>
#endif

#include "glue.h"

#if PLATFORM != PLAT_NXDK && PLATFORM != PLAT_EMSCR
static void sigh_log(int l, char* name, char* msg) {
    if (msg) {
        plog(l, "Received signal: %s; %s", name, msg);
    } else {
        plog(l, "Received signal: %s", name);
    }
}
static void sigh(int sig) {
    signal(sig, sigh);
    #if defined(_POSIX_VERSION) && _POSIX_VERSION >= 200809L
    char* signame = strsignal(sig);
    #else
    char signame[8];
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
        default:
            sigh_log(LL_WARN, signame, NULL);
            break;
    }
}
#endif

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

static int bootstrap(void) {
    puts(verstr);
    puts(platstr);
    #if PLATFORM == PLAT_NXDK
    pb_print("%s\n", verstr);
    pb_print("%s\n", platstr);
    pbgl_swap_buffers();
    #endif

    if (!initLogging()) {
        fputs(LP_ERROR "Failed to init logging\n", stderr);
        return 1;
    }
    #if PLATFORM != PLAT_NXDK
    if (options.maindir) {
        maindir = mkpath(NULL, options.maindir, NULL);
        free(options.maindir);
    } else {
        maindir = SDL_GetBasePath();
        if (!maindir) {
            plog(LL_WARN, "Failed to get main directory: %s", SDL_GetError());
            maindir = ".";
        } else {
            char* tmp = maindir;
            maindir = mkpath(tmp, NULL);
            SDL_free(tmp);
        }
    }
    #else
    plog_setfile("D:\\log.txt");
    maindir = mkpath("D:\\", NULL);
    #endif

    char* tmp;

    if (options.config) {
        tmp = mkpath(NULL, options.config, NULL);
        free(options.config);
    } else {
        tmp = mkpath(maindir, "engine/config/config.cfg", NULL);
    }
    config = cfg_open(tmp);
    free(tmp);
    if (!config) {
        plog(LL_WARN, "Failed to load main config");
        config = cfg_open(NULL);
    }
    if (options.set) cfg_mergemem(config, options.set, true);

    tmp = (options.game) ? options.game : cfg_getvar(config, NULL, "defaultgame");
    if (tmp) {
        gamedir = mkpath(NULL, tmp, NULL);
        free(tmp);
    } else {
        static const char* fallback = "default";
        plog(LL_WARN, "No default game specified, falling back to '%s'", fallback);
        gamedir = mkpath(NULL, fallback, NULL);
    }

    tmp = mkpath(maindir, "games", gamedir, NULL);
    if (isFile(tmp)) {
        plog(LL_CRIT | LF_MSGBOX, "Could not find game directory for '%s'", gamedir);
        free(tmp);
        return 1;
    }
    free(tmp);

    tmp = mkpath(maindir, "games", gamedir, "game.cfg", NULL);
    gameconfig = cfg_open(tmp);
    free(tmp);
    if (!gameconfig) {
        plog(LL_CRIT | LF_MSGBOX, "Could not read game.cfg for '%s'", gamedir);
        return 1;
    }

    #if PLATFORM != PLAT_NXDK
    if (options.userdir) {
        userdir = mkpath(NULL, options.userdir, NULL);
        free(options.userdir);
    } else {
        tmp = cfg_getvar(gameconfig, NULL, "userdir");
        if (tmp) {
            char* tmp2 = mkpath(NULL, tmp, NULL);
            free(tmp);
            tmp = tmp2;
        } else {
            tmp = strdup(gamedir);
        }
        userdir = SDL_GetPrefPath(NULL, tmp);
        free(tmp);
        if (userdir) {
            char* tmp = userdir;
            userdir = mkpath(tmp, NULL);
            SDL_free(tmp);
        } else {
            plog(LL_WARN, LP_WARN "Failed to get user directory: %s", SDL_GetError());
            userdir = "." PATHSEPSTR "data";
        }
    }
    savedir = mkpath(userdir, "saves", NULL);
    #else
    {
        uint32_t titleid = CURRENT_XBE_HEADER->CertificateHeader->TitleID;
        char titleidstr[9];
        sprintf(titleidstr, "%08x", (unsigned)titleid);
        userdir = mkpath("E:\\TDATA", titleidstr, "userdata", tmp, NULL);
        free(tmp);
        savedir = mkpath("E:\\UDATA", titleidstr, NULL);
    }
    #endif

    tmp = cfg_getvar(gameconfig, NULL, "name");
    if (tmp) {
        free(titlestr);
        titlestr = tmp;
    }

    char* logfile = mkpath(userdir, "log.txt", NULL);
    if (!plog_setfile(logfile)) {
        plog(LL_WARN, "Failed to set log file");
    }
    free(logfile);

    if (!options.nouserconfig) {
        tmp = mkpath(userdir, "config", "config.cfg", NULL);
        if (!cfg_merge(config, tmp, true)) {
            plog(LL_WARN, "Failed to load user config");
        }
        free(tmp);
        if (options.set) cfg_mergemem(config, options.set, true);
    }

    plog(LL_INFO, "Main directory: %s", maindir);
    plog(LL_INFO, "Game directory: %s" PATHSEPSTR "%s", maindir, gamedir);
    plog(LL_INFO, "User directory: %s", userdir);
    plog(LL_INFO, "Save directory: %s", savedir);

    plog(LL_INFO, "Initializing resource manager...");
    if (!initResource()) {
        plog(LL_CRIT | LF_FUNCLN, "Failed to init resource manager");
        return 1;
    }

    return 0;
}
static void unstrap(void) {
    plog(LL_INFO, "Quitting resource manager...");
    quitResource();

    cfg_close(gameconfig);
    char* tmp = mkpath(userdir, "config", "config.cfg", NULL);
    //cfg_write(config, tmp);
    free(tmp);
    cfg_close(config);

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
    (void)arg;
    exit(0);
}
static void emscrmain(void) {
    static bool doloop = false;
    static bool syncmsg = false;
    if (doloop) {
        doLoop();
        if (quitreq) {
            static bool doexit = false;
            if (doexit) {
                if (!syncmsg) {
                    puts("Waiting on RAM to disk FS.syncfs...");
                    syncmsg = true;
                }
                if (issyncfsok) {
                    emscripten_cancel_main_loop();
                    emscripten_async_call(emscrexit, NULL, -1);
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
    }
}
#endif
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    makeVerStrs();
    static int ret;
    #if PLATFORM != PLAT_EMSCR
    #if PLATFORM != PLAT_NXDK
    ret = -1;
    {
        struct args a;
        args_init(&a, argc, argv);
        struct charbuf opt;
        struct charbuf var;
        struct charbuf val;
        struct charbuf err;
        cb_init(&opt, 32);
        cb_init(&var, 32);
        cb_init(&val, 256);
        cb_init(&err, 64);
        while (1) {
            int e = args_getopt(&a, &opt, &err);
            if (e == -1) {
                fprintf(stderr, "%s\n", cb_peek(&err));
                ret = 1;
                break;
            } else if (e == 0) {
                break;
            }
            cb_nullterm(&opt);
            if (!strcmp(opt.data, "help")) {
                e = args_getoptval(&a, 0, -1, &val, &err);
                if (e == -1) {
                    fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                    ret = 1;
                    break;
                }
                printf("USAGE: %s [OPTION]...\n", argv[0]);
                puts("OPTIONS:");
                puts("    -help - Display this help text.");
                puts("    -version - Display version information.");
                puts("    -game=NAME - Set the game to run.");
                puts("    -maindir=DIR - Set the main directory.");
                puts("    -userdir=DIR - Set the user directory.");
                puts("    -{set|s} [SECT|VAR=VAL]... - Override config values.");
                puts("    -{config|cfg|c}=FILE - Set the config file path.");
                puts("    -{nouserconfig|nousercfg} - Do not load the user config.");
                puts("    -mods NAME[,NAME]... - Mods to load (on top of the mods in the config).");
                puts("    -icon PATH - Override the game's icon.");
                ret = 0;
            } else if (!strcmp(opt.data, "version")) {
                e = args_getoptval(&a, 0, -1, &val, &err);
                if (e == -1) {
                    fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                    ret = 1;
                    break;
                }
                puts(verstr);
                puts(platstr);
                #if DEBUG(0)
                printf("Debug level: %d\n", PSRC_DBGLVL);
                #endif
                #ifdef USE_DISCORD_GAME_SDK
                puts("Discord game SDK enabled");
                #endif
                #ifdef PSRC_NOSIMD
                puts("No SIMD");
                #endif
                #ifdef PSRC_NOMT
                puts("No multithreading");
                #endif
                ret = 0;
            } else if (!strcmp(opt.data, "game")) {
                e = args_getoptval(&a, 1, -1, &val, &err);
                if (e == -1) {
                    fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                    ret = 1;
                    break;
                }
                free(options.game);
                options.game = cb_reinit(&val, 256);
            } else if (!strcmp(opt.data, "mods")) {
                e = args_getoptval(&a, 1, -1, &val, &err);
                if (e == -1) {
                    fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                    ret = 1;
                    break;
                }
                free(options.mods);
                options.mods = cb_reinit(&val, 256);
            } else if (!strcmp(opt.data, "icon")) {
                e = args_getoptval(&a, 1, -1, &val, &err);
                if (e == -1) {
                    fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                    ret = 1;
                    break;
                }
                free(options.icon);
                options.icon = cb_reinit(&val, 256);
            } else if (!strcmp(opt.data, "set") || !strcmp(opt.data, "s")) {
                e = args_getoptval(&a, 0, -1, &val, &err);
                if (e == -1) {
                    fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                    ret = 1;
                    break;
                }
                if (!options.set) options.set = cfg_open(NULL);
                char* sect = NULL;
                while (1) {
                    e = args_getvar(&a, &var, &err);
                    if (e == -1) {
                        fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                        ret = 1;
                        free(sect);
                        goto longbreak;
                    } else if (e == 0) {
                        break;
                    }
                    e = args_getvarval(&a, -1, &val, &err);
                    if (e == -1) {
                        fprintf(stderr, "-%s: %s: %s\n", opt.data, var.data, cb_peek(&err));
                        ret = 1;
                        free(sect);
                        goto longbreak;
                    } else if (e == 0) {
                        free(sect);
                        sect = cb_reinit(&var, 32);
                    } else {
                        cfg_setvar(options.set, sect, cb_peek(&var), cb_peek(&val), true);
                        cb_clear(&val);
                    }
                    cb_clear(&var);
                }
                free(sect);
            } else if (!strcmp(opt.data, "maindir")) {
                e = args_getoptval(&a, 1, -1, &val, &err);
                if (e == -1) {
                    fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                    ret = 1;
                    break;
                }
                free(options.maindir);
                options.maindir = cb_reinit(&val, 256);
            } else if (!strcmp(opt.data, "userdir")) {
                e = args_getoptval(&a, 1, -1, &val, &err);
                if (e == -1) {
                    fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                    ret = 1;
                    break;
                }
                free(options.userdir);
                options.userdir = cb_reinit(&val, 256);
            } else if (!strcmp(opt.data, "config") || !strcmp(opt.data, "cfg") || !strcmp(opt.data, "c")) {
                e = args_getoptval(&a, 1, -1, &val, &err);
                if (e == -1) {
                    fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                    ret = 1;
                    break;
                }
                free(options.config);
                options.config = cb_reinit(&val, 256);
            } else if (!strcmp(opt.data, "nouserconfig") || !strcmp(opt.data, "nousercfg")) {
                e = args_getoptval(&a, 0, -1, &val, &err);
                if (e == -1) {
                    fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                    ret = 1;
                    break;
                }
                options.nouserconfig = true;
            } else if (!strcmp(opt.data, "nocontroller")) {
                e = args_getoptval(&a, 0, -1, &val, &err);
                if (e == -1) {
                    fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                    ret = 1;
                    break;
                }
                options.nocontroller = true;
            } else {
                fprintf(stderr, "Unknown option: -%s\n", opt.data);
                ret = 1;
                break;
            }
            cb_clear(&opt);
        }
        longbreak:;
        cb_dump(&opt);
        cb_dump(&var);
        cb_dump(&val);
        cb_dump(&err);
        if (ret >= 0) return ret;
    }
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
    #endif
    #if PLATFORM == PLAT_WIN32
    TIMECAPS tc;
    UINT tmrres = 1;
    if (timeGetDevCaps(&tc, sizeof(tc)) != TIMERR_NOERROR) {
        if (tmrres < tc.wPeriodMin) {
            tmrres = tc.wPeriodMin;
        } else if (tmrres > tc.wPeriodMax) {
            tmrres = tc.wPeriodMax;
        }
    }
    timeBeginPeriod(tmrres);
    QueryPerformanceFrequency(&perfctfreq);
    while (!(perfctfreq.QuadPart % 10) && !(perfctmul % 10)) {
        perfctfreq.QuadPart /= 10;
        perfctmul /= 10;
    }
    #elif PLATFORM == PLAT_NXDK
    perfctfreq = KeQueryPerformanceFrequency();
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    pbgl_set_swap_interval(0);
    //pbgl_set_swap_interval(1);
    pbgl_init(true);
    //pbgl_set_swap_interval(1);
    glClearColor(0.0, 0.25, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    pbgl_swap_buffers();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    pbgl_swap_buffers();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    pbgl_swap_buffers();
    #endif
    ret = bootstrap();
    if (!ret) {
        ret = initLoop();
        if (!ret) {
            while (1) {
                doLoop();
                if (quitreq) break;
            }
            quitLoop();
        }
        unstrap();
    }
    #if PLATFORM == PLAT_WIN32
    timeEndPeriod(tmrres);
    #elif PLATFORM == PLAT_NXDK
    if (ret) Sleep(5);
    pbgl_shutdown();
    HalReturnToFirmware(HalQuickRebootRoutine);
    #endif
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
    return ret;
}
