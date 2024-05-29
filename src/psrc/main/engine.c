#include "../engine/renderer.h"
#include "../engine/input.h"
#include "../engine/ui.h"
#include "../engine/audio.h"
#include "../common/logging.h"
#include "../common/string.h"
#include "../common/filesystem.h"
#include "../common/config.h"
#include "../common/resource.h"
#include "../common/time.h"
#include "../common/arg.h"

#include "../version.h"
#include "../platform.h"
#include "../debug.h"
#include "../common.h"

#if PLATFORM == PLAT_NXDK
    #include <SDL.h>
#elif defined(PSRC_USESDL1)
    #include <SDL/SDL.h>
#else
    #include <SDL2/SDL.h>
#endif

#if (PLATFLAGS & PLATFLAG_UNIXLIKE) || PLATFORM == PLAT_WIN32
    #include <signal.h>
#endif
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
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
#elif PLATFORM == PLAT_DREAMCAST
    #include <kos.h>
    #include <dirent.h>
    KOS_INIT_FLAGS(INIT_IRQ | INIT_THD_PREEMPT | INIT_CONTROLLER | INIT_VMU | INIT_NO_DCLOAD);
#elif PLATFORM == PLAT_3DS
    #include <3ds.h>
    #include <dirent.h>
#elif PLATFORM == PLAT_WII || PLATFORM == PLAT_GAMECUBE
    #include <fat.h>
    #include <dirent.h>
#endif

#include "../glue.h"

#if (PLATFLAGS & PLATFLAG_UNIXLIKE) || PLATFORM == PLAT_WIN32
static void sigh_log(int l, char* name, char* msg) {
    if (msg) {
        plog(l, "Received signal: %s; %s", name, msg);
    } else {
        plog(l, "Received signal: %s", name);
    }
}
static void sigh_cb_addstr(struct charbuf* b, const char* s) {
    char c;
    while ((c = *s)) {
        if (isalnum(c)) {
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
                cb_addstr(&cb, "https://github.com/PQCraft/PlatinumSrc/issues/new?labels=bug&title=Automatic%20crash%20report&body=");
                sigh_cb_addstr(&cb, verstr);
                sigh_cb_addstr(&cb, "\n");
                sigh_cb_addstr(&cb, platstr);
                sigh_cb_addstr(&cb, "\nGame: ");
                sigh_cb_addstr(&cb, gamedir);
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
            exit(1);
        } break;
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

struct rc_script* mainscript;

static struct rc_sound* test;
static int testemt_map;
static int testemt_obj;
static float lookspeed[2];
static uint64_t toff;
static uint64_t framestamp;

enum __attribute__((packed)) action {
    ACTION_NONE,
    ACTION_MENU,
    ACTION_FULLSCREEN,
    ACTION_SCREENSHOT,
    ACTION_MOVE_FORWARDS,
    ACTION_MOVE_BACKWARDS,
    ACTION_MOVE_LEFT,
    ACTION_MOVE_RIGHT,
    ACTION_LOOK_UP,
    ACTION_LOOK_DOWN,
    ACTION_LOOK_LEFT,
    ACTION_LOOK_RIGHT,
    ACTION_WALK,
    ACTION_JUMP,
    ACTION_CROUCH,
};

int initLoop(void) {
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
    plog(LL_INFO, "Initializing audio manager...");
    if (!initAudio()) {
        plog(LL_CRIT | LF_MSGBOX | LF_FUNCLN, "Failed to init audio manager");
        return 1;
    }

    char* tmp;
    if (options.icon) {
        rendstate.icon = mkpath(NULL, options.icon, NULL);
    } else {
        tmp = cfg_getvar(gameconfig, NULL, "icon");
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
        //return 1;
    }
    plog(LL_INFO, "Starting audio manager...");
    if (!startAudio()) {
        plog(LL_CRIT | LF_MSGBOX | LF_FUNCLN, "Failed to start audio manager");
        return 1;
    }

    plog(LL_INFO, "Almost there...");

    mainscript = loadResource(RC_SCRIPT, "main", NULL);
    if (!mainscript) {
        plog(LL_CRIT | LF_MSGBOX, "Could not start main script");
        //return 1;
    }

    testemt_map = newAudioEmitter(1, true, SOUNDFX_END);
    testemt_obj = newAudioEmitter(1, false, 0.0, 0.0, 4.0, SOUNDFX_END);

    test = loadResource(RC_SOUND, "sounds/ambient/wind1", &audiostate.soundrcopt);
    if (test) setAmbientSound(test);
    freeResource(test);
    test = loadResource(RC_SOUND, "sounds/ac1", &audiostate.soundrcopt);
    if (test) playSound(testemt_map, test, SOUNDFLAG_LOOP | SOUNDFLAG_WRAP, SOUNDFX_POS, 0.0, 0.0, 2.0, SOUNDFX_END);
    freeResource(test);
    test = loadResource(RC_SOUND, "sounds/healthstation", &audiostate.soundrcopt);
    if (test) playSound(testemt_obj, test, SOUNDFLAG_LOOP, SOUNDFX_END);
    freeResource(test);

    // TODO: cleanup
    setInputMode(INPUTMODE_INGAME);
    struct inputkey* k;
    #if PLATFORM != PLAT_EMSCR
    k = inputKeysFromStr("k,esc;g,b,start");
    #else
    k = inputKeysFromStr("k,backspace;g,b,start");
    #endif
    newInputAction(INPUTACTIONTYPE_ONCE, "menu", k, (void*)ACTION_MENU);
    free(k);
    k = inputKeysFromStr("k,f11");
    newInputAction(INPUTACTIONTYPE_ONCE, "fullscreen", k, (void*)ACTION_FULLSCREEN);
    free(k);
    k = inputKeysFromStr("k,f2");
    newInputAction(INPUTACTIONTYPE_ONCE, "screenshot", k, (void*)ACTION_SCREENSHOT);
    free(k);
    #if PLATFORM != PLAT_DREAMCAST
    k = inputKeysFromStr("k,w;g,a,-lefty");
    #else
    k = inputKeysFromStr("k,w;g,b,y");
    #endif
    newInputAction(INPUTACTIONTYPE_MULTI, "move_forwards", k, (void*)ACTION_MOVE_FORWARDS);
    free(k);
    #if PLATFORM != PLAT_DREAMCAST
    k = inputKeysFromStr("k,a;g,a,-leftx");
    #else
    k = inputKeysFromStr("k,a;g,b,x");
    #endif
    newInputAction(INPUTACTIONTYPE_MULTI, "move_left", k, (void*)ACTION_MOVE_LEFT);
    free(k);
    #if PLATFORM != PLAT_DREAMCAST
    k = inputKeysFromStr("k,s;g,a,+lefty");
    #else
    k = inputKeysFromStr("k,s;g,b,a");
    #endif
    newInputAction(INPUTACTIONTYPE_MULTI, "move_backwards", k, (void*)ACTION_MOVE_BACKWARDS);
    free(k);
    #if PLATFORM != PLAT_DREAMCAST
    k = inputKeysFromStr("k,d;g,a,+leftx");
    #else
    k = inputKeysFromStr("k,d;g,b,b");
    #endif
    newInputAction(INPUTACTIONTYPE_MULTI, "move_right", k, (void*)ACTION_MOVE_RIGHT);
    free(k);
    #if PLATFORM != PLAT_DREAMCAST
    k = inputKeysFromStr("m,m,+y;k,up;g,a,-righty");
    #else
    k = inputKeysFromStr("m,m,+y;k,up;g,a,-lefty");
    #endif
    newInputAction(INPUTACTIONTYPE_MULTI, "look_up", k, (void*)ACTION_LOOK_UP);
    free(k);
    #if PLATFORM != PLAT_DREAMCAST
    k = inputKeysFromStr("m,m,-x;k,left;g,a,-rightx");
    #else
    k = inputKeysFromStr("m,m,-x;k,left;g,a,-leftx");
    #endif
    newInputAction(INPUTACTIONTYPE_MULTI, "look_left", k, (void*)ACTION_LOOK_LEFT);
    free(k);
    #if PLATFORM != PLAT_DREAMCAST
    k = inputKeysFromStr("m,m,-y;k,down;g,a,+righty");
    #else
    k = inputKeysFromStr("m,m,-y;k,down;g,a,+lefty");
    #endif
    newInputAction(INPUTACTIONTYPE_MULTI, "look_down", k, (void*)ACTION_LOOK_DOWN);
    free(k);
    #if PLATFORM != PLAT_DREAMCAST
    k = inputKeysFromStr("m,m,+x;k,right;g,a,+rightx");
    #else
    k = inputKeysFromStr("m,m,+x;k,right;g,a,+leftx");
    #endif
    newInputAction(INPUTACTIONTYPE_MULTI, "look_right", k, (void*)ACTION_LOOK_RIGHT);
    free(k);
    #if PLATFORM != PLAT_EMSCR
    k = inputKeysFromStr("k,lctrl;g,b,leftstick");
    #else
    k = inputKeysFromStr("k,c;g,b,leftstick");
    #endif
    newInputAction(INPUTACTIONTYPE_MULTI, "walk", k, (void*)ACTION_WALK);
    free(k);
    #if PLATFORM != PLAT_DREAMCAST
    k = inputKeysFromStr("k,space;g,b,a");
    #else
    k = inputKeysFromStr("k,space;g,a,+lefttrigger");
    #endif
    newInputAction(INPUTACTIONTYPE_MULTI, "jump", k, (void*)ACTION_JUMP);
    free(k);
    #if PLATFORM != PLAT_DREAMCAST
    k = inputKeysFromStr("k,lshift;g,a,+lefttrigger");
    #else
    k = inputKeysFromStr("k,lshift;g,b,dpdown");
    #endif
    newInputAction(INPUTACTIONTYPE_MULTI, "crouch", k, (void*)ACTION_CROUCH);
    free(k);

    tmp = cfg_getvar(config, "Input", "lookspeed");
    if (tmp) {
        sscanf(tmp, "%f,%f", &lookspeed[0], &lookspeed[1]);
        free(tmp);
    } else {
        lookspeed[0] = 2.0f;
        lookspeed[1] = 2.0f;
    }

    plog(LL_INFO, "All systems go!");
    toff = SDL_GetTicks();
    framestamp = altutime();

    return 0;
}

static float fwrap(float n, float d) {
    float tmp = n - (int)(n / d) * d;
    if (tmp < 0.0) tmp += d;
    return tmp;
}
void doLoop(void) {
    static float runspeed = 4.0f;
    static float walkspeed = 2.0f;
    static double framemult = 0.0;

    pollInput();
    #if PLATFORM == PLAT_3DS
    if (!aptMainLoop()) ++quitreq;
    #endif
    if (quitreq) return;

    // TODO: run scripts here

    long lt = SDL_GetTicks() - toff;
    double dt = (double)(lt % 1000) / 1000.0;
    double t = (double)(lt / 1000) + dt;
    editAudioEmitter(testemt_obj, false, SOUNDFX_POS, sin(t * 2.5) * 4.0, 0.0, cos(t * 2.5) * 4.0, SOUNDFX_END);

    static bool screenshot = false;

    bool walk = false;
    float movex = 0.0f, movez = 0.0f, movey = 0.0f;
    float lookx = 0.0f, looky = 0.0f;
    struct inputaction a;
    while (getNextInputAction(&a)) {
        //printf("action!: %s: %f\n", a.data->name, (float)a.amount / 32767.0f);
        switch ((enum action)a.userdata) {
            case ACTION_MENU: ++quitreq; break;
            case ACTION_FULLSCREEN: updateRendererConfig(RENDOPT_FULLSCREEN, -1, RENDOPT_END); break;
            case ACTION_SCREENSHOT: screenshot = true; break;
            case ACTION_MOVE_FORWARDS: movez += (float)a.amount / 32767.0f; break;
            case ACTION_MOVE_BACKWARDS: movez -= (float)a.amount / 32767.0f; break;
            case ACTION_MOVE_LEFT: movex -= (float)a.amount / 32767.0f; break;
            case ACTION_MOVE_RIGHT: movex += (float)a.amount / 32767.0f; break;
            case ACTION_LOOK_UP: lookx += (float)a.amount * ((a.constant) ? framemult * 50.0f : 1.0f) / 32767.0f; break;
            case ACTION_LOOK_DOWN: lookx -= (float)a.amount * ((a.constant) ? framemult * 50.0f : 1.0f) / 32767.0f; break;
            case ACTION_LOOK_LEFT: looky -= (float)a.amount * ((a.constant) ? framemult * 50.0f : 1.0f) / 32767.0f; break;
            case ACTION_LOOK_RIGHT: looky += (float)a.amount * ((a.constant) ? framemult * 50.0f : 1.0f) / 32767.0f; break;
            case ACTION_WALK: walk = true; break;
            case ACTION_JUMP: movey += (float)a.amount / 32767.0f; break;
            case ACTION_CROUCH: movey -= (float)a.amount / 32767.0f; break;
            default: break;
        }
    }
    float speed = (walk) ? walkspeed : runspeed;
    float jumpspeed = (walk) ? 1.0f : 2.5f;

    char* tmp = cfg_getvar(config, "Debug", "printfps");
    bool printfps = strbool(tmp, false);
    free(tmp);

    {
        // this can probably be optimized
        float mul = atan2f(fabs(movex), fabs(movez));
        mul = fabs(1 / (cosf(mul) + sinf(mul)));
        movex *= mul;
        movez *= mul;
        float yrotrad = rendstate.camrot[1] * (float)M_PI / 180.0f;
        float tmp[4] = {
            movez * sinf(yrotrad),
            movex * cosf(yrotrad),
            movez * cosf(yrotrad),
            movex * -sinf(yrotrad),
        };
        movex = tmp[0] + tmp[1];
        movez = tmp[2] + tmp[3];
    }
    audiostate.cam.pos[0] = (rendstate.campos[0] += movex * speed * framemult);
    audiostate.cam.pos[2] = (rendstate.campos[2] += movez * speed * framemult);
    audiostate.cam.pos[1] = (rendstate.campos[1] += movey * jumpspeed * framemult);
    rendstate.camrot[0] += lookx * lookspeed[1];
    rendstate.camrot[1] += looky * lookspeed[0];
    //rendstate.camrot[2] =  22.5f;
    if (rendstate.camrot[0] > 90.0f) rendstate.camrot[0] = 90.0f;
    else if (rendstate.camrot[0] < -90.0f) rendstate.camrot[0] = -90.0f;
    rendstate.camrot[1] = fwrap(rendstate.camrot[1], 360.0f);
    audiostate.cam.rot[0] = rendstate.camrot[0];
    audiostate.cam.rot[1] = rendstate.camrot[1];
    audiostate.cam.rot[2] = rendstate.camrot[2];

    updateSounds(framemult);
    render();
    display();

    if (screenshot) {
        int sz;
        void* d = takeScreenshot(NULL, NULL, &sz);
        FILE* f = fopen("screenshot.data", "wb");
        fwrite(d, 1, sz, f);
        fclose(f);
        free(d);
        screenshot = false;
    }

    uint64_t tmputime = altutime();
    uint64_t frametime = tmputime - framestamp;
    framestamp = tmputime;
    framemult = frametime / 1000000.0;

    if (printfps) {
        static uint64_t fpstime = 0;
        static uint64_t fpsframetime = 0;
        static int fpsframecount = 0;
        fpsframetime += frametime;
        ++fpsframecount;
        if (tmputime >= fpstime) {
            fpstime += 200000;
            if (tmputime >= fpstime) fpstime = tmputime + 200000;
            uint64_t avgframetime = fpsframetime / fpsframecount;
            float avgfps = 1000000.0 / (uint64_t)avgframetime;
            printf("FPS: %.03f (%"PRId64"us)\n", avgfps, avgframetime);
            fpsframetime = 0;
            fpsframecount = 0;
        }
    }
}

void quitLoop(void) {
    plog(LL_INFO, "Quit requested");
    #if PLATFORM == PLAT_NXDK
    plog__nodraw = false;
    #endif

    #if PLATFORM == PLAT_NXDK && !defined(PSRC_NOMT)
    armWatchdog(5);
    #endif
    plog(LL_INFO, "Stopping audio manager...");
    stopAudio();
    #if PLATFORM == PLAT_NXDK && !defined(PSRC_NOMT)
    rearmWatchdog(5);
    #endif
    plog(LL_INFO, "Stopping renderer...");
    stopRenderer();
    #if PLATFORM == PLAT_NXDK && !defined(PSRC_NOMT)
    cancelWatchdog();
    #endif

    plog(LL_INFO, "Quitting audio manager...");
    quitAudio();
    plog(LL_INFO, "Quitting input manager...");
    quitInput();
    plog(LL_INFO, "Quitting renderer...");
    quitRenderer();
}

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
        fputs(LP_ERROR "Failed to init logging\n", stderr);
        return 1;
    }

    if (options.maindir) {
        maindir = mkpath(options.maindir, NULL);
        free(options.maindir);
    } else {
        maindir = mkmaindir();
    }

    char* tmp;

    if (options.config) {
        tmp = mkpath(options.config, NULL);
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
        plog(LL_INFO, "Main directory: %s", maindir);
        plog(LL_CRIT | LF_MSGBOX, "Could not find game directory for '%s'", gamedir);
        free(tmp);
        return 1;
    }
    free(tmp);

    tmp = mkpath(maindir, "games", gamedir, "game.cfg", NULL);
    gameconfig = cfg_open(tmp);
    free(tmp);
    if (!gameconfig) {
        plog(LL_INFO, "Main directory: %s", maindir);
        plog(LL_INFO, "Game directory: %s" PATHSEPSTR "games" PATHSEPSTR "%s", maindir, gamedir);
        plog(LL_CRIT | LF_MSGBOX, "Could not read game.cfg for '%s'", gamedir);
        return 1;
    }

    if (options.userdir) {
        userdir = mkpath(options.userdir, NULL);
        free(options.userdir);
    } else {
        char* tmp = cfg_getvar(gameconfig, NULL, "userdir");
        if (tmp) {
            userdir = mkuserdir(maindir, tmp);
            free(tmp);
        } else {
            userdir = mkuserdir(maindir, gamedir);
        }
    }
    #if PLATFORM == PLAT_NXDK
    {
        uint32_t titleid = CURRENT_XBE_HEADER->CertificateHeader->TitleID;
        char titleidstr[9];
        sprintf(titleidstr, "%08x", (unsigned)titleid);
        savedir = mkpath("E:\\UDATA", titleidstr, NULL);
    }
    #elif PLATFORM == PLAT_DREAMCAST
    userdir = mkpath("/rd", NULL);
    #elif PLATFORM != PLAT_DREAMCAST
    savedir = mkpath(userdir, "saves", NULL);
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
    plog(LL_INFO, "Game directory: %s" PATHSEPSTR "games" PATHSEPSTR "%s", maindir, gamedir);
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
    makeVerStrs();
    static int ret;
    #if PLATFORM != PLAT_EMSCR
    #if (PLATFLAGS & PLATFLAG_UNIXLIKE) || PLATFORM == PLAT_WIN32
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
                puts("    -help                       Display this help text.");
                puts("    -version                    Display version information.");
                puts("    -game=NAME                  Set the game to run.");
                puts("    -mods=NAME[,NAME]...        More mods to load.");
                puts("    -icon=PATH                  Override the game's icon.");
                puts("    -maindir=DIR                Set the main directory.");
                puts("    -userdir=DIR                Set the user directory.");
                puts("    -{set|s} [SECT|VAR=VAL]...  Override config values.");
                puts("    -{config|cfg|c}=FILE        Set the config file path.");
                puts("    -{nouserconfig|nousercfg}   Do not load the user config.");
                puts("    -nocontroller               Do not init controllers.");
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
                #ifdef PSRC_USEDISCORDGAMESDK
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
    #else
    (void)argc; (void)argv;
    #if PLATFORM == PLAT_WII
    fatInitDefault();
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
    glClearColor(0.0f, 0.25f, 0.0f, 1.0f);
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
            while (!quitreq) {
                doLoop();
            }
            quitLoop();
        }
        unstrap();
    }
    #if PLATFORM == PLAT_WIN32
    timeEndPeriod(tmrres);
    #elif PLATFORM == PLAT_NXDK
    if (ret) Sleep(5000);
    pbgl_shutdown();
    HalReturnToFirmware(HalQuickRebootRoutine);
    #elif PLATFORM == PLAT_DREAMCAST
    arch_menu();
    #endif
    #else
    // TODO: pass in output from SDL_GetPrefPath
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
