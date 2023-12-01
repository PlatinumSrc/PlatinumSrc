#include "../engine/renderer.h"
#include "../engine/input.h"
#include "../engine/ui.h"
#include "../engine/audio.h"
#include "../utils/logging.h"
#include "../utils/string.h"
#include "../utils/filesystem.h"
#include "../utils/config.h"
#include "../common/resource.h"
#include "../common/common.h"
#include "../common/time.h"

#include "../version.h"
#include "../platform.h"
#include "../debug.h"

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#if PLATFORM == PLAT_WINDOWS
    #include <windows.h>
#elif PLATFORM == PLAT_NXDK
    #include <xboxkrnl/xboxkrnl.h>
    #include <winapi/winnt.h>
    #include <hal/video.h>
    #include <pbkit/pbkit.h>
    #include <pbgl.h>
    #include <GL/gl.h>
#endif

#include "../glue.h"

#if PLATFORM != PLAT_NXDK

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

static int run(int argc, char** argv) {
    (void)argc;
    (void)argv;

    plog(LL_INFO, "Main directory (absolute): %s", maindir);
    plog(LL_INFO, "Game directory (relative): %s", gamedir);
    plog(LL_INFO, "User directory (absolute): %s", userdir);
    plog(LL_INFO, "Save directory (absolute): %s", savedir);

    plog(LL_INFO, "Initializing resource manager...");
    if (!initResource()) {
        plog(LL_CRIT | LF_FUNCLN, "Failed to init resource manager");
        return 1;
    }

    plog(LL_INFO, "Initializing renderer...");
    if (!initRenderer()) {
        plog(LL_CRIT | LF_MSGBOX | LF_FUNCLN, "Failed to init renderer");
        return 1;
    }
    plog(LL_INFO, "Initializing input manager...");
    if (!initInput()) {
        plog(LL_CRIT | LF_MSGBOX | LF_FUNCLN, "Failed to init input manager");
        return 1;
    }
    plog(LL_INFO, "Initializing UI manager...");
    if (!initUI()) {
        plog(LL_CRIT | LF_MSGBOX | LF_FUNCLN, "Failed to init UI manager");
        return 1;
    }
    plog(LL_INFO, "Initializing audio manager...");
    if (!initAudio()) {
        plog(LL_CRIT | LF_MSGBOX | LF_FUNCLN, "Failed to init audio manager");
        return 1;
    }

    {
        char* tmp = cfg_getvar(gameconfig, NULL, "icon");
        if (tmp) {
            rendstate.icon = getRcPath(tmp, RC_TEXTURE, NULL);
            free(tmp);
        } else {
            rendstate.icon = mkpath(maindir, "icons", "engine.png", NULL);
        }
    }
    plog(LL_INFO, "Starting renderer...");
    if (!startRenderer()) {
        plog(LL_CRIT | LF_MSGBOX | LF_FUNCLN, "Failed to start renderer");
        return 1;
    }
    plog(LL_INFO, "Starting audio manager...");
    if (!startAudio()) {
        plog(LL_CRIT | LF_MSGBOX | LF_FUNCLN, "Failed to start audio manager");
        return 1;
    }

    plog(LL_INFO, "Almost there...");

    struct rc_sound* test;
    test = loadResource(RC_SOUND, "common:sounds/ambient/wind1", &audiostate.soundrcopt);
    if (test) playSound(false, test, SOUNDFLAG_LOOP, SOUNDFX_VOL, 0.5, 0.5, SOUNDFX_END);
    freeResource(test);
    //test = loadResource(RC_SOUND, "game:test/mp3test_1", &audiostate.soundrcopt);
    //if (test) playSound(false, test, SOUNDFLAG_LOOP, SOUNDFX_VOL, 0.25, 0.25, SOUNDFX_END);
    //freeResource(test);
    test = loadResource(RC_SOUND, "sounds/healthstation", &audiostate.soundrcopt);
    uint64_t testsound = -1;
    if (test) testsound = playSound(
        false, test,
        SOUNDFLAG_POSEFFECT | SOUNDFLAG_FORCEMONO | SOUNDFLAG_LOOP,
        SOUNDFX_VOL, 0.5, 0.5, SOUNDFX_POS, 0.0, 0.0, 5.0, SOUNDFX_END
    );
    freeResource(test);

    #if PLATFORM == PLAT_NXDK
    uint64_t ticks = SDL_GetTicks() + 30000;
    plog__nodraw = true;
    #endif
    uint64_t toff = SDL_GetTicks();
    plog(LL_INFO, "All systems go!");
    while (!quitreq) {
        long lt = SDL_GetTicks() - toff;
        double dt = (double)(lt % 1000) / 1000.0;
        double t = (double)(lt / 1000) + dt;
        changeSoundFX(testsound, false, SOUNDFX_POS, sin(t * 2.5) * 2.0, 0.0, cos(t * 2.5) * 2.0, SOUNDFX_END);
        pollInput();
        render();
        #if PLATFORM == PLAT_NXDK
        if (SDL_TICKS_PASSED(SDL_GetTicks(), ticks)) ++quitreq;
        #endif
    }
    plog(LL_INFO, "Quit requested");
    #if PLATFORM == PLAT_NXDK
    plog__nodraw = false;
    #endif

    plog(LL_INFO, "Stopping audio manager...");
    stopAudio();
    plog(LL_INFO, "Stopping renderer...");
    stopRenderer();

    plog(LL_INFO, "Terminating audio manager...");
    termAudio();
    plog(LL_INFO, "Terminating UI manager...");
    termUI();
    plog(LL_INFO, "Terminating input manager...");
    termInput();
    plog(LL_INFO, "Terminating renderer...");
    termRenderer();

    plog(LL_INFO, "Terminating resource manager...");
    termResource();

    plog(LL_INFO, "Done");
    return 0;
}

static int bootstrap(int argc, char** argv) {
    makeVerStrs();

    #if PLATFORM != PLAT_NXDK
    puts(verstr);
    puts(platstr);
    #else
    pb_print("%s\n", verstr);
    pb_print("%s\n", platstr);
    pbgl_swap_buffers();
    #endif

    if (!initLogging()) {
        fputs(LP_ERROR "Failed to init logging\n", stderr);
        return 1;
    }
    #if PLATFORM == PLAT_NXDK
    plog_setfile("D:\\log.txt");
    #endif

    #if PLATFORM != PLAT_NXDK
    maindir = SDL_GetBasePath();
    if (!maindir) {
        plog(LL_WARN, "Failed to get main directory: %s", SDL_GetError());
        maindir = ".";
    } else {
        char* tmp = maindir;
        maindir = mkpath(tmp, NULL);
        SDL_free(tmp);
    }
    #else
    maindir = mkpath("D:\\", NULL);
    #endif

    char* tmp = mkpath(maindir, "engine/config", "config.cfg", NULL);
    config = cfg_open(tmp);
    free(tmp);
    if (!config) {
        plog(LL_WARN, "Failed to load main config");
        config = cfg_open(NULL);
    }

    tmp = cfg_getvar(config, NULL, "defaultgame");
    cfg_delvar(config, NULL, "defaultgame");
    if (tmp) {
        gamedir = mkpath(NULL, tmp, NULL);
        free(tmp);
    } else {
        const char* fallback = "default";
        plog(LL_WARN, "No default game specified, falling back to %s", fallback);
        gamedir = mkpath(NULL, fallback, NULL);
    }

    tmp = mkpath(maindir, "games", gamedir, "game.cfg", NULL);
    gameconfig = cfg_open(tmp);
    free(tmp);
    if (!gameconfig) {
        plog(LL_CRIT | LF_MSGBOX, "Could not read game config for %s", gamedir);
        return 1;
    }

    tmp = cfg_getvar(gameconfig, NULL, "userdir");
    if (tmp) {
        char* tmp2 = mkpath(NULL, tmp, NULL);
        free(tmp);
        tmp = tmp2;
    } else {
        tmp = strdup(gamedir);
    }
    #if PLATFORM != PLAT_NXDK
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

    tmp = mkpath(userdir, "config", "config.cfg", NULL);
    if (!cfg_merge(config, tmp, true)) {
        plog(LL_WARN, "Failed to load user config");
    }
    free(tmp);

    int ret = run(argc, argv);

    cfg_close(gameconfig);

    tmp = mkpath(userdir, "config", "config.cfg", NULL);
    //cfg_write(config, tmp);
    free(tmp);
    cfg_close(config);

    SDL_Quit();

    return ret;
}

int main(int argc, char** argv) {
    #if PLATFORM != PLAT_NXDK
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

    #if PLATFORM == PLAT_WINDOWS
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
    #elif PLATFORM == PLAT_NXDK
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

    int ret = bootstrap(argc, argv);

    #if PLATFORM == PLAT_WINDOWS
    timeEndPeriod(tmrres);
    #elif PLATFORM == PLAT_NXDK
    pbgl_shutdown();
    HalReturnToFirmware(HalQuickRebootRoutine);
    #endif

    return ret;
}
