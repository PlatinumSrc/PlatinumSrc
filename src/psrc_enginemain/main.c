#include "../psrc_engine/renderer.h"
#include "../psrc_engine/input.h"
#include "../psrc_aux/statestack.h"
#include "../psrc_aux/logging.h"
#include "../psrc_aux/string.h"
#include "../psrc_aux/fs.h"
#include "../psrc_aux/config.h"
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

void findDirs() {
    #if PLATFORM != PLAT_XBOX
    maindir = SDL_GetBasePath();
    if (!maindir) {
        plog(LL_WARN, "Failed to get start directory: %s", (char*)SDL_GetError());
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
        case SIGINT:;
            if (quitreq > 0) {
                sigh_log(LL_WARN, signame, "Graceful exit already requested; Forcing exit...");
                exit(1);
            } else {
                sigh_log(LL_INFO, signame, "Requesting graceful exit...");
                ++quitreq;
            }
            break;
        case SIGTERM:;
        #ifdef SIGQUIT
        case SIGQUIT:;
        #endif
            sigh_log(LL_WARN, signame, "Forcing exit...");
            exit(1);
            break;
        default:;
            sigh_log(LL_WARN, signame, NULL);
            break;
    }
}

#endif

#if PLATFORM == PLAT_XBOX
static void logMemUsage() {
    MM_STATISTICS mstats = {.Length = sizeof(mstats)};
    MmQueryStatistics(&mstats);
    unsigned long mtotal = mstats.TotalPhysicalPages * PAGE_SIZE;
    unsigned long mavail = mstats.AvailablePages * PAGE_SIZE;
    unsigned long mused = mtotal - mavail;
    mtotal = (uint64_t)mtotal * 1000 / 1024 * 1000 / 1024;
    mavail = (uint64_t)mavail * 1000 / 1024 * 1000 / 1024;
    mused = (uint64_t)mused * 1000 / 1024 * 1000 / 1024;
    unsigned long mtotal_dec = (mtotal % 1000000) / 1000;
    unsigned long mavail_dec = (mavail % 1000000) / 1000;
    unsigned long mused_dec = (mused % 1000000) / 1000;
    int mtotal_pad = 3;
    int mavail_pad = 3;
    int mused_pad = 3;
    while (mtotal_pad > 1 && mtotal_dec % 10 == 0) {mtotal_dec /= 10; --mtotal_pad;}
    while (mavail_pad > 1 && mavail_dec % 10 == 0) {mavail_dec /= 10; --mavail_pad;}
    while (mused_pad > 1 && mused_dec % 10 == 0) {mused_dec /= 10; --mused_pad;}
    mtotal /= 1000000;
    mavail /= 1000000;
    mused /= 1000000;
    plog(
        LL_INFO,
        "Memory usage: %lu.%0*luMiB used out of %lu.%0*luMiB (%lu.%0*luMiB available)",
        mused, mused_pad, mused_dec, mtotal, mtotal_pad, mtotal_dec, mavail, mavail_pad, mavail_dec
    );
}
#endif

struct states {
    struct rendstate renderer;
    struct inputstate input;
};

static void do_nothing(struct statestack* state) {
    (void)state;
    if (quitreq > 0) state_return(state, NULL);
}

static int run(int argc, char** argv) {
    (void)argc;
    (void)argv;

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
    struct statestack statestack;

    if (!initRenderer(&states->renderer)) return 1;
    states->renderer.cfg.icon = mkpath(maindir, "icons", "engine.png", NULL);
    if (!startRenderer(&states->renderer)) return 1;
    if (!initInput(&states->input, &states->renderer)) return 1;

    #if PLATFORM == PLAT_XBOX
    logMemUsage();
    #endif

    state_initstack(&statestack);
    state_push(&statestack, do_nothing, states);
    while (statestack.index >= 0) {
        pollInput(&states->input);
        state_runstack(&statestack);
        render(&states->renderer);
    }
    state_deinitstack(&statestack);

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
    #else
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
