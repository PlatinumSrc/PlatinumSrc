#include "../psrc_engine/renderer.h"
#include "../psrc_engine/input.h"
#include "../psrc_engine/audio.h"
#include "../psrc_aux/logging.h"
#include "../psrc_aux/string.h"
#include "../psrc_aux/filesystem.h"
#include "../psrc_aux/config.h"
#include "../psrc_game/resource.h"
#include "../psrc_game/dirs.h"
#include "../version.h"
#include "../platform.h"

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#if PLATFORM == PLAT_XBOX
    #include <xboxkrnl/xboxkrnl.h>
    #include <winapi/winnt.h>
    #include <hal/video.h>
    #include <pbgl.h>
#endif

#include "../glue.h"

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

static struct cfg* config;
static struct cfg* gameconfig;

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

    char* logfile = mkpath(userdir, "log.txt", NULL);
    plog_setfile(logfile);
    free(logfile);

    {
        char* months[12] = {
            "Jan", "Feb", "Mar", "Apr",
            "May", "Jun", "Jul", "Aug",
            "Sep", "Oct", "Nov", "Dec"
        };
        plog(
            LL_PLAIN,
            "PlatinumSrc build %u (%s %u, %u; rev %u)",
            (unsigned)PSRC_BUILD,
            months[(((unsigned)PSRC_BUILD / 10000) % 100) - 1],
            ((unsigned)PSRC_BUILD / 100) % 100,
            (unsigned)PSRC_BUILD / 1000000,
            ((unsigned)PSRC_BUILD % 100) + 1
        );
    }
    plog(LL_PLAIN, "Platform: %s; Architecture: %s", (char*)PLATSTR, (char*)ARCHSTR);

    plog(LL_INFO, "Main directory: %s", maindir);
    plog(LL_INFO, "User directory: %s", userdir);
    plog(LL_INFO, "Game directory: %s", gamedir);
    plog(LL_INFO, "Save directory: %s", savedir);

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

    #if PLATFORM != PLAT_XBOX
    maindir = SDL_GetBasePath();
    if (!maindir) {
        fprintf(stderr, LP_WARN "Failed to get main directory: %s\n", SDL_GetError());
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
    if (!config) config = cfg_open(NULL);
    free(tmp);

    tmp = cfg_getvar(config, NULL, "defaultgame");
    cfg_delvar(config, NULL, "defaultgame");
    if (!tmp) tmp = strdup("h74");
    gamedir = mkpath(NULL, tmp, NULL);
    free(tmp);

    tmp = mkpath(maindir, "games", gamedir, "game.cfg", NULL);
    gameconfig = cfg_open(tmp);
    if (!gameconfig) {
        fprintf(stderr, LP_CRIT "Could not read game config for %s\n", gamedir);
        return 1;
    }
    free(tmp);

    tmp = cfg_getvar(config, NULL, "userdir");
    if (tmp) {
        char* tmp2 = mkpath(NULL, tmp, NULL);
        free(tmp);
        tmp = tmp2;
    } else {
        tmp = strdup(gamedir);
    }

    #if PLATFORM != PLAT_XBOX
    userdir = SDL_GetPrefPath(NULL, tmp);
    free(tmp);
    if (userdir) {
        char* tmp = userdir;
        userdir = mkpath(tmp, NULL);
        SDL_free(tmp);
    } else {
        fprintf(stderr, LP_WARN "Failed to get user directory: %s\n", SDL_GetError());
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
    }
    savedir = mkpath("E:\\UDATA", titleidstr, NULL);
    #endif

    tmp = mkpath(userdir, "config", "config.cfg", NULL);
    cfg_merge(config, tmp, true);
    free(tmp);

    #if PLATFORM == PLAT_XBOX
    {
        int w = 640, h = 480, r = REFRESH_DEFAULT;
        if (!XVideoSetMode(w, h, 32, r)) {
            plog(LL_WARN, "Failed to set resolution to %dx%d@%d", w, h, r);
            if (!XVideoSetMode((w = 640), (h = 480), 32, (r = REFRESH_DEFAULT))) return 1;
        }
    }
    if (pbgl_init(true)) return 1;
    #endif

    if (!initLogging()) {
        fputs(LP_ERROR "Failed to init logging\n", stderr);
        return 1;
    }

    int ret = run(argc, argv);

    cfg_close(gameconfig);

    tmp = mkpath(userdir, "config", "config.cfg", NULL);
    //cfg_write(config, tmp);
    free(tmp);
    cfg_close(config);

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
