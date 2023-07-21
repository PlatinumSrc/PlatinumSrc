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
    #include <hal/video.h>
    #include <pbgl.h>
#endif

#include "../glue.h"

#if PLATFORM == PLAT_XBOX
__asm__ (
    ".section \"XTIMAGE\"\n"
    ".byte 0"
);
#endif

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

int main(int argc, char** argv) {
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
        XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    #endif

    #if PLATFORM == PLAT_XBOX
    pbgl_init(true);
    #endif

    int ret = run(argc, argv);

    #if PLATFORM == PLAT_XBOX
    pbgl_shutdown();
    #endif
    SDL_Quit();

    return ret;
}
