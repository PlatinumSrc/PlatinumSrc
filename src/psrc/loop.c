#include "engine/renderer.h"
#include "engine/input.h"
#include "engine/ui.h"
#include "engine/audio.h"
#include "common/logging.h"
#include "common/config.h"
#include "common/filesystem.h"
#include "common/common.h"
#include "common/time.h"

static struct rc_sound* test;
static uint64_t testsound = -1;
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
        return 1;
    }
    plog(LL_INFO, "Starting audio manager...");
    if (!startAudio()) {
        plog(LL_CRIT | LF_MSGBOX | LF_FUNCLN, "Failed to start audio manager");
        return 1;
    }

    plog(LL_INFO, "Almost there...");

    test = loadResource(RC_SOUND, "common:sounds/wind1", &audiostate.soundrcopt);
    if (test) playSound(false, test, SOUNDFLAG_LOOP, SOUNDFX_VOL, 0.5, 0.5, SOUNDFX_END);
    freeResource(test);
    test = loadResource(RC_SOUND, "sounds/ac1", &audiostate.soundrcopt);
    if (test) playSound(
        false, test,
        SOUNDFLAG_POSEFFECT | SOUNDFLAG_FORCEMONO | SOUNDFLAG_LOOP,
        SOUNDFX_POS, 0.0, 0.0, 2.0, SOUNDFX_END
    );
    freeResource(test);
    test = loadResource(RC_SOUND, "sounds/health", &audiostate.soundrcopt);
    if (test) testsound = playSound(
        false, test,
        SOUNDFLAG_POSEFFECT | SOUNDFLAG_FORCEMONO | SOUNDFLAG_LOOP,
        SOUNDFX_POS, 0.0, 0.0, 4.0, SOUNDFX_END
    );
    freeResource(test);

    setInputMode(INPUTMODE_INGAME);
    struct inputkey* k;
    #if PLATFORM != PLAT_EMSCR
    k = inputKeysFromStr("k,escape;g,b,start");
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
    k = inputKeysFromStr("k,w;g,a,-lefty");
    newInputAction(INPUTACTIONTYPE_MULTI, "move_forwards", k, (void*)ACTION_MOVE_FORWARDS);
    free(k);
    k = inputKeysFromStr("k,a;g,a,-leftx");
    newInputAction(INPUTACTIONTYPE_MULTI, "move_left", k, (void*)ACTION_MOVE_LEFT);
    free(k);
    k = inputKeysFromStr("k,s;g,a,+lefty");
    newInputAction(INPUTACTIONTYPE_MULTI, "move_backwards", k, (void*)ACTION_MOVE_BACKWARDS);
    free(k);
    k = inputKeysFromStr("k,d;g,a,+leftx");
    newInputAction(INPUTACTIONTYPE_MULTI, "move_right", k, (void*)ACTION_MOVE_RIGHT);
    free(k);
    k = inputKeysFromStr("m,m,+y;k,up;g,a,-righty");
    newInputAction(INPUTACTIONTYPE_MULTI, "look_up", k, (void*)ACTION_LOOK_UP);
    free(k);
    k = inputKeysFromStr("m,m,-x;k,left;g,a,-rightx");
    newInputAction(INPUTACTIONTYPE_MULTI, "look_left", k, (void*)ACTION_LOOK_LEFT);
    free(k);
    k = inputKeysFromStr("m,m,-y;k,down;g,a,+righty");
    newInputAction(INPUTACTIONTYPE_MULTI, "look_down", k, (void*)ACTION_LOOK_DOWN);
    free(k);
    k = inputKeysFromStr("m,m,+x;k,right;g,a,+rightx");
    newInputAction(INPUTACTIONTYPE_MULTI, "look_right", k, (void*)ACTION_LOOK_RIGHT);
    free(k);
    #if PLATFORM != PLAT_EMSCR
    k = inputKeysFromStr("k,left ctrl;g,b,leftstick");
    #else
    k = inputKeysFromStr("k,c;g,b,leftstick");
    #endif
    newInputAction(INPUTACTIONTYPE_MULTI, "walk", k, (void*)ACTION_WALK);
    free(k);
    k = inputKeysFromStr("k,space;g,b,a");
    newInputAction(INPUTACTIONTYPE_MULTI, "jump", k, (void*)ACTION_JUMP);
    free(k);
    k = inputKeysFromStr("k,left shift;g,a,+lefttrigger");
    newInputAction(INPUTACTIONTYPE_MULTI, "crouch", k, (void*)ACTION_CROUCH);
    free(k);

    tmp = cfg_getvar(config, "Input", "lookspeed");
    if (tmp) {
        sscanf(tmp, "%f,%f", &lookspeed[0], &lookspeed[1]);
        free(tmp);
    } else {
        lookspeed[0] = 2.0;
        lookspeed[1] = 2.0;
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
    static float runspeed = 3.75;
    static float walkspeed = 1.75;
    static double framemult = 0.0;

    pollInput();
    if (quitreq) return;

    // TODO: run scripts here

    long lt = SDL_GetTicks() - toff;
    double dt = (double)(lt % 1000) / 1000.0;
    double t = (double)(lt / 1000) + dt;
    changeSoundFX(testsound, false, SOUNDFX_POS, sin(t * 2.5) * 4.0, 0.0, cos(t * 2.5) * 4.0, SOUNDFX_END);

    static bool screenshot = false;

    bool walk = false;
    float movex = 0.0, movez = 0.0, movey = 0.0;
    float lookx = 0.0, looky = 0.0;
    struct inputaction a;
    while (getNextInputAction(&a)) {
        //printf("action!: %s: %f\n", a.data->name, (float)a.amount / 32767.0);
        switch ((enum action)a.userdata) {
            case ACTION_MENU: ++quitreq; break;
            case ACTION_FULLSCREEN: updateRendererConfig(RENDOPT_FULLSCREEN, -1, RENDOPT_END); break;
            case ACTION_SCREENSHOT: screenshot = true; break;
            case ACTION_MOVE_FORWARDS: movez += (float)a.amount / 32767.0; break;
            case ACTION_MOVE_BACKWARDS: movez -= (float)a.amount / 32767.0; break;
            case ACTION_MOVE_LEFT: movex -= (float)a.amount / 32767.0; break;
            case ACTION_MOVE_RIGHT: movex += (float)a.amount / 32767.0; break;
            case ACTION_LOOK_UP: lookx += (float)a.amount * ((a.constant) ? framemult * 50.0 : 1.0) / 32767.0; break;
            case ACTION_LOOK_DOWN: lookx -= (float)a.amount * ((a.constant) ? framemult * 50.0 : 1.0) / 32767.0; break;
            case ACTION_LOOK_LEFT: looky -= (float)a.amount * ((a.constant) ? framemult * 50.0 : 1.0) / 32767.0; break;
            case ACTION_LOOK_RIGHT: looky += (float)a.amount * ((a.constant) ? framemult * 50.0 : 1.0) / 32767.0; break;
            case ACTION_WALK: walk = true; break;
            case ACTION_JUMP: movey += (float)a.amount / 32767.0; break;
            case ACTION_CROUCH: movey -= (float)a.amount / 32767.0; break;
            default: break;
        }
    }
    float speed = (walk) ? walkspeed : runspeed;
    float jumpspeed = (walk) ? 1.0 : 2.5;

    {
        // this can probably be optimized
        float mul = atan2(fabs(movex), fabs(movez));
        mul = fabs(1 / (cos(mul) + sin(mul)));
        movex *= mul;
        movez *= mul;
        float yrotrad = rendstate.camrot[1] * M_PI / 180.0;
        float tmp[4] = {
            movez * sinf(yrotrad),
            movex * cosf(yrotrad),
            movez * cosf(yrotrad),
            movex * -sinf(yrotrad),
        };
        movex = tmp[0] + tmp[1];
        movez = tmp[2] + tmp[3];
    }
    audiostate.campos[0] = (rendstate.campos[0] += movex * speed * framemult);
    audiostate.campos[2] = (rendstate.campos[2] += movez * speed * framemult);
    audiostate.campos[1] = (rendstate.campos[1] += movey * jumpspeed * framemult);
    rendstate.camrot[0] += lookx * lookspeed[1];
    rendstate.camrot[1] += looky * lookspeed[0];
    if (rendstate.camrot[0] > 90.0) rendstate.camrot[0] = 90.0;
    else if (rendstate.camrot[0] < -90.0) rendstate.camrot[0] = -90.0;
    rendstate.camrot[1] = fwrap(rendstate.camrot[1], 360.0);
    audiostate.camrot[0] = rendstate.camrot[0];
    audiostate.camrot[1] = rendstate.camrot[1];
    audiostate.camrot[2] = rendstate.camrot[2];

    updateSounds();
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

    uint64_t tmp = altutime();
    uint64_t frametime = tmp - framestamp;
    framestamp = tmp;
    framemult = frametime / 1000000.0;
}

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

void termLoop(void) {
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

    plog(LL_INFO, "Terminating audio manager...");
    termAudio();
    plog(LL_INFO, "Terminating UI manager...");
    termUI();
    plog(LL_INFO, "Terminating input manager...");
    termInput();
    plog(LL_INFO, "Terminating renderer...");
    termRenderer();
}
