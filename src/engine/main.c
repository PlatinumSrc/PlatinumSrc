#include "renderer.h"
#include "statestack.h"
#include <logging.h>
#include <version.h>

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

static int quitreq; // move into input handling code

void sigh_log(int l, char* name, char* msg) {
    if (msg) {
        plog(l, "Received signal: %s; %s", name, msg);
    } else {
        plog(l, "Received signal: %s", name);
    }
}
void sigh(int sig) {
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
                sigh_log(LOGLVL_WARN, signame, "Graceful exit already requested; Forcing exit...");
                exit(1);
            } else {
                sigh_log(LOGLVL_INFO, signame, "Requesting graceful exit...");
                ++quitreq;
            }
            break;
        case SIGTERM:;
        #ifdef SIGQUIT
        case SIGQUIT:;
        #endif
            sigh_log(LOGLVL_WARN, signame, "Forcing exit...");
            exit(1);
            break;
        default:;
            sigh_log(LOGLVL_WARN, signame, NULL);
            break;
    }
}

void do_nothing(struct statestack* state) {(void)state;}

int run(int argc, char** argv) {
    (void)argc;
    (void)argv;
    plog(LOGLVL_PLAIN, "PlatinumSrc build %u", (unsigned)PSRC_BUILD);

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

    struct statestack state;
    struct rendstate renderer;
    if (!initRenderer(&renderer)) return 1;

    state_initstack(&state);
    state_push(&state, do_nothing, NULL);
    while (quitreq <= 0 && state.index >= 0) {
        //getInput(renderer->window);
        state_runstack(&state);
        render(&renderer);
    }
    state_deinitstack(&state);

    termRenderer(&renderer);

    return 0;
}

int main(int argc, char** argv) {
    return run(argc, argv);
}
