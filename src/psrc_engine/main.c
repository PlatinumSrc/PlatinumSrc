#include "renderer.h"
#include "input.h"
#include "../psrc_aux/statestack.h"
#include "../psrc_aux/logging.h"
#include "../version.h"

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

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

struct states {
    struct rendstate renderer;
    struct inputstate input;
};

void do_nothing(struct statestack* state) {
    (void)state;
    if (quitreq > 0) state_return(state, NULL);
}

int run(int argc, char** argv) {
    (void)argc;
    (void)argv;
    plog(LOGLVL_PLAIN, "PlatinumSrc build %u", (unsigned)PSRC_BUILD);

    struct states* states = malloc(sizeof(*states));
    struct statestack statestack;
    
    if (!initRenderer(&states->renderer)) return 1;
    if (!startRenderer(&states->renderer)) return 1;
    if (!initInput(&states->input, &states->renderer)) return 1;

    {
        struct rendupdate u = {0};
        struct rendconfig c;
        if (!updateRendererConfig(&states->renderer, &u, &c)) {
            plog(LOGLVL_CRIT, "Failed to update renderer config");
            return 1;
        }
    }

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

    return run(argc, argv);
}
