#include "../engine/renderer.h"
#include "../engine/input.h"
#include "../engine/ui.h"
#include "../engine/audio.h"
#include "../common/arg.h"
#if DEBUG(1)
    #include "../common/profiling.h"
#endif

struct rc_script* mainscript;

static struct rc_sound* test;
static int testemt_map;
static int testemt_obj;
static float lookspeed[2];
static uint64_t toff;
static uint64_t framestamp;
#if DEBUG(1)
    static bool printfps;
    static bool printprof;
#endif

PACKEDENUM action {
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

#if DEBUG(1)
static struct profile dbgprof;
enum {
    DBGPROF_EVENTS,
    DBGPROF_SCRIPTS,
    DBGPROF_SERVER,
    DBGPROF_AUDIO,
    DBGPROF_RENDERER,
    DBGPROF_RENDSWAP,
    DBGPROF__COUNT
};
static const char* dbgprofstr[DBGPROF__COUNT] = {
    "Event handling",
    "Scripts",
    "Server",
    "Audio",
    "Renderer",
    "Swap buffers",
};
static inline void printprofpoint(uint8_t r, uint8_t g, uint8_t b, unsigned t, unsigned p, const char* n) {
    printf("\e[38;2;%u;%u;%um\u2588\u2588 - %5.02f%% (%.03fms) - %s\e[0m\n", r, g, b, p / 100.0, t / 1000.0, n);
}
#endif

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

    // TODO: move into the renderer
    char* tmp;
    if (options.icon) {
        rendstate.icon = strpath(options.icon);
    } else {
        if (gameinfo.icon) {
            rendstate.icon = getRcPath(RC_TEXTURE, gameinfo.icon, NULL, NULL, NULL);
            if (!rendstate.icon) rendstate.icon = getRcPath(RC_TEXTURE, STR(PSRC_DEFAULTLOGO), NULL, NULL, NULL);
        } else {
            rendstate.icon = getRcPath(RC_TEXTURE, STR(PSRC_DEFAULTLOGO), NULL, NULL, NULL);
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

    {
        struct charbuf e;
        cb_init(&e, 128);
        mainscript = loadResource(RC_SCRIPT, "main", NULL, &e);
        if (!mainscript) {
            plog(LL_CRIT | LF_MSGBOX, "Could not start main script: %s", cb_peek(&e));
            //return 1;
        }
        cb_dump(&e);
    }

    testemt_map = newAudioEmitter(1, true, SOUNDFX_END);
    testemt_obj = newAudioEmitter(1, false, 0.0, 0.0, 4.0, SOUNDFX_END);

    test = loadResource(RC_SOUND, "sounds/ambient/wind1", &audiostate.soundrcopt, NULL);
    if (test) setAmbientSound(test);
    freeResource(test);
    test = loadResource(RC_SOUND, "sounds/ac1", &audiostate.soundrcopt, NULL);
    if (test) playSound(testemt_map, test, SOUNDFLAG_LOOP | SOUNDFLAG_WRAP, SOUNDFX_POS, 0.0, 0.0, 2.0, SOUNDFX_END);
    freeResource(test);
    test = loadResource(RC_SOUND, "sounds/healthstation", &audiostate.soundrcopt, NULL);
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

    tmp = cfg_getvar(&config, "Input", "lookspeed");
    if (tmp) {
        sscanf(tmp, "%f,%f", &lookspeed[0], &lookspeed[1]);
        free(tmp);
    } else {
        lookspeed[0] = 2.0f;
        lookspeed[1] = 2.0f;
    }

    #if DEBUG(1)
        tmp = cfg_getvar(&config, "Debug", "printfps");
        printfps = strbool(tmp, true);
        free(tmp);
        tmp = cfg_getvar(&config, "Debug", "printprof");
        printprof = strbool(tmp, false);
        free(tmp);
        prof_init(&dbgprof, DBGPROF__COUNT, dbgprofstr);
        prof_start(&dbgprof);
        rendstate.dbgprof = &dbgprof;
    #endif

    plog(LL_INFO, "All systems go!");
    toff = SDL_GetTicks();
    framestamp = altutime();

    return 0;
}

static float fwrap(float n, float d) {
    float tmp = n - (int)(n / d) * d;
    if (tmp < 0.0f) tmp += d;
    return tmp;
}
void doLoop(void) {
    #if DEBUG(1)
    prof_start(&dbgprof);
    #endif

    static float runspeed = 4.0f;
    static float walkspeed = 2.0f;
    static double framemult = 0.0;

    #if DEBUG(1)
    prof_begin(&dbgprof, DBGPROF_EVENTS);
    #endif
    pollInput();
    #if PLATFORM == PLAT_3DS
    if (!aptMainLoop()) ++quitreq;
    #endif
    #if DEBUG(1)
    prof_end(&dbgprof);
    #endif

    if (quitreq) return;

    // TODO: run scripts here

    long lt = SDL_GetTicks() - toff;
    double dt = (double)(lt % 1000) / 1000.0;
    double t = (double)(lt / 1000) + dt;
    #if DEBUG(1)
    prof_begin(&dbgprof, DBGPROF_AUDIO);
    #endif
    editAudioEmitter(testemt_obj, false, SOUNDFX_POS, sin(t * 2.5) * 4.0, 0.0, cos(t * 2.5) * 4.0, SOUNDFX_END);
    #if DEBUG(1)
    prof_end(&dbgprof);
    #endif

    static bool screenshot = false;

    bool walk = false;
    float movex = 0.0f, movez = 0.0f, movey = 0.0f;
    float lookx = 0.0f, looky = 0.0f;
    #if DEBUG(1)
    prof_begin(&dbgprof, DBGPROF_EVENTS);
    #endif
    struct inputaction a;
    while (getNextInputAction(&a)) {
        //printf("action!: %s: %f\n", a.data->name, (float)a.amount / 32767.0f);
        switch ((enum action)(uintptr_t)a.userdata) {
            case ACTION_MENU: ++quitreq; break;
            case ACTION_FULLSCREEN: updateRendererConfig(RENDOPT_FULLSCREEN, -1, RENDOPT_END); break;
            case ACTION_SCREENSHOT: screenshot = true; break;
            case ACTION_MOVE_FORWARDS: movez += (float)a.amount / 32767.0f; break;
            case ACTION_MOVE_BACKWARDS: movez -= (float)a.amount / 32767.0f; break;
            case ACTION_MOVE_LEFT: movex -= (float)a.amount / 32767.0f; break;
            case ACTION_MOVE_RIGHT: movex += (float)a.amount / 32767.0f; break;
            case ACTION_LOOK_UP: lookx += (float)a.amount * ((a.constant) ? (float)framemult * 50.0f : 1.0f) / 32767.0f; break;
            case ACTION_LOOK_DOWN: lookx -= (float)a.amount * ((a.constant) ? (float)framemult * 50.0f : 1.0f) / 32767.0f; break;
            case ACTION_LOOK_LEFT: looky -= (float)a.amount * ((a.constant) ? (float)framemult * 50.0f : 1.0f) / 32767.0f; break;
            case ACTION_LOOK_RIGHT: looky += (float)a.amount * ((a.constant) ? (float)framemult * 50.0f : 1.0f) / 32767.0f; break;
            case ACTION_WALK: walk = true; break;
            case ACTION_JUMP: movey += (float)a.amount / 32767.0f; break;
            case ACTION_CROUCH: movey -= (float)a.amount / 32767.0f; break;
            default: break;
        }
    }
    #if DEBUG(1)
    prof_end(&dbgprof);
    #endif
    float speed = (walk) ? walkspeed : runspeed;
    float jumpspeed = (walk) ? 1.0f : 2.5f;

    {
        // this can probably be optimized
        float mul = atan2f(fabs(movex), fabs(movez));
        mul = fabs(1 / (cosf(mul) + sinf(mul)));
        movex *= mul;
        movez *= mul;
        float yrotrad = rendstate.camrot[1] * (float)M_PI / 180.0f;
        float tmp[4];
        tmp[0] = sinf(yrotrad);
        tmp[1] = cosf(yrotrad);
        tmp[2] = tmp[1] * movez;
        tmp[3] = -tmp[0] * movex;
        tmp[0] *= movez;
        tmp[1] *= movex;
        movex = tmp[0] + tmp[1];
        movez = tmp[2] + tmp[3];
    }
    audiostate.cam.pos[0] = (rendstate.campos[0] += movex * speed * (float)framemult);
    audiostate.cam.pos[2] = (rendstate.campos[2] += movez * speed * (float)framemult);
    audiostate.cam.pos[1] = (rendstate.campos[1] += movey * jumpspeed * (float)framemult);
    rendstate.camrot[0] += lookx * lookspeed[1];
    rendstate.camrot[1] += looky * lookspeed[0];
    //rendstate.camrot[2] =  22.5f;
    if (rendstate.camrot[0] > 90.0f) rendstate.camrot[0] = 90.0f;
    else if (rendstate.camrot[0] < -90.0f) rendstate.camrot[0] = -90.0f;
    rendstate.camrot[1] = fwrap(rendstate.camrot[1], 360.0f);
    audiostate.cam.rot[0] = rendstate.camrot[0];
    audiostate.cam.rot[1] = rendstate.camrot[1];
    audiostate.cam.rot[2] = rendstate.camrot[2];

    #if DEBUG(1)
    prof_begin(&dbgprof, DBGPROF_AUDIO);
    #endif
    updateSounds(framemult);
    #if DEBUG(1)
    prof_begin(&dbgprof, DBGPROF_RENDERER);
    #endif
    render();
    #if DEBUG(1)
    prof_begin(&dbgprof, DBGPROF_RENDSWAP);
    #endif
    display();
    #if DEBUG(1)
    prof_end(&dbgprof);
    #endif

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

    #if DEBUG(1)
        static uint64_t fpstime = 0;
        static uint64_t fpsframetime = 0;
        static int fpsframecount = 0;
        static int profcurwait = -1;
        static const int profwait = 2;
        fpsframetime += frametime;
        ++fpsframecount;
        if (tmputime >= fpstime) {
            fpstime += 500000;
            if (tmputime >= fpstime) fpstime = tmputime + 500000;
            if (printfps) {
                uint64_t avgframetime = fpsframetime / fpsframecount;
                double avgfps = 1000000.0 / (uint64_t)avgframetime;
                printf("FPS: %.03f (%.03fms)\n", avgfps, avgframetime / 1000.0);
            }
            fpsframetime = 0;
            fpsframecount = 0;
            if (profcurwait <= 0) {
                prof_calc(&dbgprof);
                if (printprof) {
                    for (int i = 0; i < DBGPROF__COUNT; ++i) {
                        printprofpoint(
                            dbgprof.colors[i].r, dbgprof.colors[i].g, dbgprof.colors[i].b,
                            dbgprof.time[i], dbgprof.percent[i],
                            dbgprofstr[i]
                        );
                    }
                    printprofpoint(192, 192, 192, dbgprof.time[-1], dbgprof.percent[-1], "Other");
                    for (int i = 0; i < profwait; ++i) {
                        putchar('\n');
                    }
                    for (int i = 0; i < profwait; ++i) {
                        putchar('\e');
                        putchar('[');
                        putchar('A');
                    }
                }
            }
            if (profcurwait == profwait) profcurwait = 0;
            else ++profcurwait;
        }
    #endif
}

void quitLoop(void) {
    plog(LL_INFO, "Quit requested");

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

static int parseargs(int argc, char** argv) {
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
    int ret = -1;
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
            puts("    -portable                   Run in portable mode.");
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
            if (!options.set__setup) {
                 cfg_open(&options.set, NULL);
                 options.set__setup = true;
             }
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
                    cfg_setvar(&options.set, sect, cb_peek(&var), cb_peek(&val), true);
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
    return ret;
}
