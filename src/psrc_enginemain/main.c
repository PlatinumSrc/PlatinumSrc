#include "../psrc_engine/renderer.h"
#include "../psrc_engine/input.h"
#include "../psrc_engine/audio.h"
#include "../psrc_aux/logging.h"
#include "../psrc_aux/string.h"
#include "../psrc_aux/filesystem.h"
#include "../psrc_aux/config.h"
#include "../psrc_game/resource.h"
#include "../version.h"
#include "../platform.h"

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#if PLATFORM == PLAT_XBOX
    #include <xboxkrnl/xboxkrnl.h>
    #include <hal/video.h>
    #include <pbgl.h>
#endif

#include "../glue.h"

char* curdir;
char* maindir;

void findDirs(void) {
    #if PLATFORM != PLAT_XBOX
    maindir = SDL_GetBasePath();
    if (!maindir) {
        plog(LL_WARN, "Failed to get start directory: %s", SDL_GetError());
        maindir = ".";
    }
    curdir = ".";
    #else
    maindir = "D:\\";
    curdir = "D:\\";
    #endif
}

#if PLATFORM != PLAT_XBOX

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

struct states {
    struct rendstate renderer;
    struct inputstate input;
    struct audiostate audio;
};

static int run(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (!initResource()) {
        plog(LL_CRIT, "Failed to init resource manager");
        return 1;
    }

    findDirs();

    char* logfile = mkpath(maindir, "log.txt", NULL);
    plog_setfile(logfile);
    free(logfile);

    plog(LL_PLAIN, "PlatinumSrc build %u", (unsigned)PSRC_BUILD);
    plog(LL_PLAIN, "Platform: %s; Architecture: %s", (char*)PLATSTR, (char*)ARCHSTR);

    plog(LL_INFO, "Main directory: %s", maindir);
    plog(LL_INFO, "Current directory: %s", curdir);

    char* tmp = mkpath(maindir, "config", "base.txt", NULL);
    cfg_open(tmp);
    free(tmp);

    struct states* states = malloc(sizeof(*states));

    if (!initRenderer(&states->renderer)) {
        plog(LL_CRIT, "Failed to init renderer");
        return 1;
    }
    states->renderer.icon = mkpath(maindir, "icons", "engine.png", NULL);
    if (!startRenderer(&states->renderer)) {
        plog(LL_CRIT, "Failed to start renderer");
        return 1;
    }
    if (!initInput(&states->input, &states->renderer)) {
        plog(LL_CRIT, "Failed to init input manager");
        return 1;
    }
    if (!initAudio(&states->audio)) {
        plog(LL_CRIT, "Failed to init audio manager");
        return 1;
    }
    if (!startAudio(&states->audio)) {
        plog(LL_CRIT, "Failed to start audio manager");
        return 1;
    }

    while (!quitreq) {
        pollInput(&states->input);
        render(&states->renderer);
    }

    termInput(&states->input);
    termRenderer(&states->renderer);

    free(states);

    return 0;
}

static int bootstrap(int argc, char** argv) {
    #if PLATFORM != PLAT_XBOX
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
    if (!initLogging()) {
        fputs("Failed to init logging\n", stderr);
        return 1;
    }
    #if PLATFORM == PLAT_XBOX
    int w = 640, h = 480, r = REFRESH_DEFAULT;
    if (!XVideoSetMode(w, h, 32, r)) {
        plog(LL_WARN, "Failed to set resolution to %dx%d@%d", w, h, r);
        if (!XVideoSetMode((w = 640), (h = 480), 32, (r = REFRESH_DEFAULT))) return 1;
    }
    if (pbgl_init(true)) return 1;
    #endif

    int ret = run(argc, argv);

    #if PLATFORM == PLAT_XBOX
    pbgl_shutdown();
    #endif
    SDL_Quit();

    return ret;
}

int main(int argc, char** argv) {
    int ret = bootstrap(argc, argv);
    #if PLATFORM == PLAT_XBOX
    HalReturnToFirmware(HalQuickRebootRoutine);
    #endif
    return ret;
}
