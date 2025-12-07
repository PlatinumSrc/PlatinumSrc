#ifdef PSRC_MODULE_ENGINE

#include "common.h"
#include "renderer.h"
#include "input.h"
#include "ui.h"
#include "audio.h"
#include "client.h"

#include "../version.h"
#include "../platform.h"
#include "../debug.h"
#include "../common.h"
#include "../logging.h"
#include "../string.h"
#include "../filesystem.h"
#include "../resource.h"
#include "../time.h"
#include "../util.h"
#include "../arg.h"
#if DEBUG(1)
    #include "../profiling.h"
#endif
#include "../incsdl.h"

#include "../common/world.h"

#include "../../stb/stb_image_write.h"

#ifndef PSRC_DEFAULTGAME
    #define PSRC_DEFAULTGAME default
#endif
#ifndef PSRC_DEFAULTLOGO
    #define PSRC_DEFAULTLOGO internal:engine/icon
#endif

struct rc_script* mainscript;

static float lookspeed[2];
static uint64_t tmptoff;
static uint64_t framestamp;
#if DEBUG(1)
    static bool printfps;
    static bool printprof;
#endif

static int e3d[16];
static int e2d[16];

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
    DBGPROF_RCMGR,
    DBGPROF_RENDSWAP,
    DBGPROF__COUNT
};
static const char* dbgprofstr[DBGPROF__COUNT] = {
    "Event handling",
    "Scripts",
    "Server",
    "Audio",
    "Renderer",
    "Resource Manager",
    "Swap buffers"
};
static inline void printprofpoint(uint8_t r, uint8_t g, uint8_t b, unsigned t, unsigned p, const char* n) {
    printf("\x1b[38;2;%u;%u;%um\u2588\u2588 - %5.02f%% (%.03fms) - %s\x1b[0m\n", r, g, b, p / 100.0, t / 1000.0, n);
}
#endif

int bootstrap(void) {
    plog(LL_MS, "Starting engine...");

    setupBaseDirs();

    char* tmp = (engine.opt.config) ? strpath(engine.opt.config) : mkpath(dirs[DIR_INTERNAL], "engine", "config.cfg", NULL);
    {
        struct datastream ds;
        bool ret = ds_openfile(tmp, NULL, false, 0, &ds);
        free(tmp);
        if (ret) {
            cfg_open(&ds, &config);
            ds_close(&ds);
        } else {
            plog(LL_WARN, "Failed to load main config");
            cfg_open(NULL, &config);
        }
    }
    if (engine.opt.set__setup) cfg_mergemem(&config, &engine.opt.set, true);

    {
        struct charbuf err;
        cb_init(&err, 256);
        if (engine.opt.game) {
            if (!setGame(engine.opt.game, true, &err)) {
                plog(LL_CRIT | LF_MSGBOX, "%s", cb_peek(&err));
                cb_dump(&err);
                return 1;
            }
            free(engine.opt.game);
            engine.opt.game = NULL;
        } else if ((tmp = cfg_getvar(&config, NULL, "defaultgame"))) {
            if (!setGame(tmp, false, &err)) {
                plog(LL_CRIT | LF_MSGBOX, "%s", cb_peek(&err));
                cb_dump(&err);
                return 1;
            }
            free(tmp);
        } else {
            plog(LL_WARN, "No default game specified, falling back to '%s'", STR(PSRC_DEFAULTGAME));
            if (!setGame(STR(PSRC_DEFAULTGAME), false, &err)) {
                plog(LL_CRIT | LF_MSGBOX, "%s", cb_peek(&err));
                cb_dump(&err);
                return 1;
            }
        }
        cb_dump(&err);
    }

    if (dirs[DIR_USER]) {
        if (!engine.opt.nouserconfig) {
            tmp = mkpath(dirs[DIR_USER], "config.cfg", NULL);
            struct datastream ds;
            bool ret = ds_openfile(tmp, NULL, false, 0, &ds);
            free(tmp);
            if (ret) {
                cfg_merge(&config, &ds, true);
                ds_close(&ds);
                if (engine.opt.set__setup) cfg_mergemem(&config, &engine.opt.set, true);
            } else {
                plog(LL_WARN, "Failed to load user config");
            }
        }
    }

    plog(LL_INFO, "Initializing resource manager...");
    if (!initRcMgr()) {
        plog(LL_CRIT | LF_MSGBOX | LF_FUNCLN, "Failed to init resource manager");
        return 1;
    }

    {
        tmp = cfg_getvar(&config, NULL, "mods");
        if (engine.opt.mods) {
            if (tmp) {
                size_t ct1, ct2;
                char** l1;
                char** l2;
                l1 = splitstrlist(tmp, ',', false, &ct1);
                free(tmp);
                l2 = splitstrlist(engine.opt.mods, ',', false, &ct2);
                free(engine.opt.mods);
                engine.opt.mods = NULL;
                l1 = realloc(l1, (ct1 + ct2) * sizeof(*l1));
                for (size_t i = 0; i < ct2; ++i) {
                    l1[i + ct1] = l2[i];
                }
                loadMods((const char* const *)l1, ct1 + ct2);
                free(*l2);
                free(l2);
                free(*l1);
                free(l1);
            } else {
                size_t ct;
                char** l = splitstrlist(engine.opt.mods, ',', false, &ct);
                free(engine.opt.mods);
                engine.opt.mods = NULL;
                loadMods((const char* const *)l, ct);
                free(*l);
                free(l);
            }
        } else if (tmp) {
            size_t ct;
            char** l = splitstrlist(tmp, ',', false, &ct);
            free(tmp);
            loadMods((const char* const *)l, ct);
            free(*l);
            free(l);
        }
        size_t ct;
        struct modinfo* data = queryMods(&ct);
        if (data) {
            plog(LL_INFO, "Mod info:");
            for (size_t i = 0; i < ct; ++i) {
                plog(LL_INFO, "  %s (%s)", data[i].name, data[i].dir);
                plog(LL_INFO, "    Path: %s", data[i].path);
                if (data[i].author) plog(LL_INFO, "    Author: %s", data[i].author);
                if (data[i].desc) plog(LL_INFO, "    Description: %s", data[i].desc);
                char s[16];
                vertostr(&data[i].version, s);
                plog(LL_INFO, "    Version: %s", s);
            }
            freeModList(data);
        } else {
            plog(LL_INFO, "No mods laoded");
        }
    }

    plog(LL_INFO, "Initializing client...");
    if (!initClient()) {
        plog(LL_CRIT | LF_MSGBOX | LF_FUNCLN, "Failed to init client");
        return 1;
    }
    setPlayer(0, "Player 1");
    tmp = cfg_getvar(&config, "Renderer", "fov");
    if (tmp) {
        playerdata.data[0].camera.fov = atof(tmp);
        free(tmp);
    } else {
        playerdata.data[0].camera.fov = 90.0f;
    }

    plog(LL_INFO, "Initializing renderer...");
    if (!initRenderer()) {
        plog(LL_CRIT | LF_MSGBOX | LF_FUNCLN, "Failed to init renderer");
        return 1;
    }
    plog(LL_INFO, "Initializing UI manager...");
    if (!initUI()) {
        plog(LL_CRIT | LF_MSGBOX | LF_FUNCLN, "Failed to init UI manager");
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
    if (engine.opt.icon) {
        rendstate.icon = strpath(engine.opt.icon);
    } else {
        // TODO: set icon from gameinfo.icon
        /*
        if (gameinfo.icon) {
            rendstate.icon = getRcPath(RC_TEXTURE, gameinfo.icon, NULL, NULL, NULL);
            if (!rendstate.icon) rendstate.icon = getRcPath(RC_TEXTURE, "internal:engine/icons/engine", NULL, NULL, NULL);
        } else {
            rendstate.icon = getRcPath(RC_TEXTURE, "internal:engine/icons/engine", NULL, NULL, NULL);
        }
        */
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

    plog(LL_MS, "Almost there...");

    {
        struct charbuf e;
        cb_init(&e, 128);
        mainscript = getRc(RC_SCRIPT, "main", &(struct rcopt_script){.pb = &engine.pb.pb, .compopt = &engine.pb.compopt}, 0, &e);
        if (!mainscript) {
            cb_undo(&e, 1);
            plog(LL_CRIT | LF_MSGBOX, "Could not start main script:\n%s", cb_peek(&e));
            cb_dump(&e);
            return 1;
        }
        if (e.len) {
            --e.len;
            plog(LL_WARN, "Main script compiled with warnings:\n%s", cb_peek(&e));
        }
        cb_dump(&e);
    }

    setAudioEnv(0, AUDIOENVMASK_REVERB, &(struct audioenv){.reverb = {0.07, 0.75, 0.99, 0.2, 0.6, 0.15}}, 0);
    //setAudioEnv(0, AUDIOENVMASK_REVERB, &(struct audioenv){.reverb = {0.1552, 0.65, 1.0, 0.05, 0.6, 0.1}}, 0);
    //setAudioEnv(0, AUDIOENVMASK_REVERB, &(struct audioenv){.reverb = {0.35, 0.8, 0.9, 0.2, 0.2, 0.2}}, 0);
    //setAudioEnv(0, AUDIOENVMASK_REVERB, &(struct audioenv){.reverb = {0.01, 0.5, 1.0, 0.0, 0.25, 0.1}}, 0);
    //setAudioEnv(0, AUDIOENVMASK_REVERB, &(struct audioenv){.reverb = {0.065, 0.5, 1.0, 0.0, 0.3, 0.0}}, 0);
    e2d[0] = new2DAudioEmitter(
        0, AUDIOPRIO_MAX, -1, 0,
        AUDIOFXMASK_VOL, &(struct audiofx){.vol = {0.75f, 0.75f}}
    );
    e3d[0] = new3DAudioEmitter(
        0, AUDIOPRIO_DEFAULT, -1, AUDIOEMITTER3DFLAG_NOENV,
        0, NULL,
        AUDIO3DFXMASK_POS | AUDIO3DFXMASK_RELPOS, &(struct audio3dfx){.pos = WORLDCOORD(0, 0, 0, 0.0f, -0.5f, 0.0f), .relpos = 1}
    );
    e3d[1] = new3DAudioEmitter(
        0, AUDIOPRIO_DEFAULT, -1, 0,
        #if 0
        AUDIOFXMASK_VOL, &(struct audiofx){.vol = {3.0f, 3.0f}},
        #else
        AUDIOFXMASK_VOL, &(struct audiofx){.vol = {0.75f, 0.75f}},
        #endif
        AUDIO3DFXMASK_POS | AUDIO3DFXMASK_RANGE | AUDIO3DFXMASK_RADIUS,
        &(struct audio3dfx){.pos = WORLDCOORD(0, 0, 0, 0.0f, 0.0f, 2.0f), .range = 40.0f, .radius = {0.5f, 0.5f, 0.5f}}
    );
    e3d[2] = new3DAudioEmitter(
        0, AUDIOPRIO_DEFAULT, -1, 0,
        AUDIOFXMASK_VOL, &(struct audiofx){.vol = {2.0f, 2.0f}},
        AUDIO3DFXMASK_POS, &(struct audio3dfx){.pos = WORLDCOORD(0, 0, 0, 0.0f, 0.0f, 3.0f)}
    );
    e3d[3] = new3DAudioEmitter(
        0, AUDIOPRIO_DEFAULT, -1, 0,
        AUDIOFXMASK_VOL, &(struct audiofx){.vol = {2.0f, 2.0f}},
        AUDIO3DFXMASK_POS | AUDIO3DFXMASK_RANGE, &(struct audio3dfx){.pos = WORLDCOORD(0, 0, 0, -10.0f, 2.0f, 10.0f), .range = 40.0f}
    );
    e3d[4] = new3DAudioEmitter(
        0, AUDIOPRIO_DEFAULT, -1, 0,
        AUDIOFXMASK_VOL, &(struct audiofx){.vol = {2.0f, 2.0f}},
        AUDIO3DFXMASK_POS | AUDIO3DFXMASK_RANGE, &(struct audio3dfx){.pos = WORLDCOORD(0, 0, 0, 10.0f, 2.0f, 10.0f), .range = 40.0f}
    );
    e3d[5] = new3DAudioEmitter(
        0, AUDIOPRIO_DEFAULT, -1, 0,
        AUDIOFXMASK_VOL, &(struct audiofx){.vol = {2.0f, 2.0f}},
        AUDIO3DFXMASK_POS, &(struct audio3dfx){.pos = WORLDCOORD(0, 0, 0, 0.0f, -0.5f, -20.0f)}
    );
    e3d[6] = new3DAudioEmitter(
        0, AUDIOPRIO_DEFAULT, -1, AUDIOEMITTER3DFLAG_NOENV,
        AUDIOFXMASK_VOL, &(struct audiofx){.vol = {2.0f, 2.0f}},
        AUDIO3DFXMASK_POS, &(struct audio3dfx){.pos = WORLDCOORD(0, 0, 0, -5.0f, 1.0f, -20.0f)}
    );
    e3d[7] = new3DAudioEmitter(
        0, AUDIOPRIO_DEFAULT, -1, AUDIOEMITTER3DFLAG_NOENV,
        AUDIOFXMASK_VOL, &(struct audiofx){.vol = {2.0f, 2.0f}},
        AUDIO3DFXMASK_POS, &(struct audio3dfx){.pos = WORLDCOORD(0, 0, 0, 5.0f, 1.0f, -20.0f)}
    );
    {
        struct rc_sound* tmpsnd = getRc(RC_SOUND, "sounds/ambient/wind1", &audiostate.soundrcopt, 0, NULL);
        if (tmpsnd) {
            play2DSound(e2d[0], tmpsnd, AUDIOPRIO_DEFAULT, SOUNDFLAG_WRAP | SOUNDFLAG_LOOP, 0, NULL);
            rlsRc(tmpsnd, false);
        }
        tmpsnd = getRc(RC_SOUND, "sounds/spawn", &audiostate.soundrcopt, 0, NULL);
        if (tmpsnd) {
            //play3DSound(e3d[0], tmpsnd, AUDIOPRIO_DEFAULT, 0, 0, NULL);
            rlsRc(tmpsnd, false);
        }
        #if 1
        tmpsnd = getRc(RC_SOUND, "sounds/env/bigmotor2start", &audiostate.soundrcopt, 0, NULL);
        if (tmpsnd) {
            int64_t toff = 5000000;
            play3DSound(e3d[1], tmpsnd, AUDIOPRIO_DEFAULT, 0, AUDIOFXMASK_TOFF | AUDIOFXMASK_VOL, &(struct audiofx){.toff = toff, .vol = {0.8f, 0.8f}});
            toff += 1000000LL * (tmpsnd->len + 1) / tmpsnd->freq;
            rlsRc(tmpsnd, false);
            tmpsnd = getRc(RC_SOUND, "sounds/env/bigmotor2", &audiostate.soundrcopt, 0, NULL);
            if (tmpsnd) {
                play3DSound(e3d[1], tmpsnd, AUDIOPRIO_DEFAULT, SOUNDFLAG_LOOP, AUDIOFXMASK_TOFF | AUDIOFXMASK_VOL, &(struct audiofx){.toff = toff, .vol = {0.8f, 0.8f}});
                rlsRc(tmpsnd, false);
            }
            tmpsnd = getRc(RC_SOUND, "sounds/env/air4start", &audiostate.soundrcopt, 0, NULL);
            if (tmpsnd) {
                play3DSound(e3d[1], tmpsnd, AUDIOPRIO_DEFAULT, 0, AUDIOFXMASK_TOFF, &(struct audiofx){.toff = toff - 1000000LL * (tmpsnd->len + 1) / tmpsnd->freq});
                rlsRc(tmpsnd, false);
                tmpsnd = getRc(RC_SOUND, "sounds/env/air4", &audiostate.soundrcopt, 0, NULL);
                if (tmpsnd) {
                    play3DSound(e3d[1], tmpsnd, AUDIOPRIO_DEFAULT, SOUNDFLAG_LOOP, AUDIOFXMASK_TOFF, &(struct audiofx){.toff = toff});
                    rlsRc(tmpsnd, false);
                }
            }
        }
        #endif
        tmpsnd = getRc(RC_SOUND, "sounds/env/drip2", &audiostate.soundrcopt, 0, NULL);
        if (tmpsnd) {
            play3DSound(e3d[2], tmpsnd, AUDIOPRIO_DEFAULT, SOUNDFLAG_WRAP | SOUNDFLAG_LOOP, 0, NULL);
            rlsRc(tmpsnd, false);
        }
        #if 1
        tmpsnd = getRc(RC_SOUND, "sounds/siren", &audiostate.soundrcopt, 0, NULL);
        if (tmpsnd) {
            play3DSound(e3d[3], tmpsnd, AUDIOPRIO_DEFAULT, SOUNDFLAG_WRAP, AUDIOFXMASK_TOFF, &(struct audiofx){.toff = 3000000});
            //play3DSound(e3d[4], tmpsnd, AUDIOPRIO_DEFAULT, SOUNDFLAG_WRAP, AUDIOFXMASK_TOFF, &(struct audiofx){.toff = 4000000});
            rlsRc(tmpsnd, false);
        }
        #endif
        tmpsnd = getRc(RC_SOUND, "sounds/env/drip1", &audiostate.soundrcopt, 0, NULL);
        if (tmpsnd) {
            play3DSound(e3d[5], tmpsnd, AUDIOPRIO_DEFAULT, SOUNDFLAG_WRAP | SOUNDFLAG_LOOP, 0, NULL);
            rlsRc(tmpsnd, false);
        }
        tmpsnd = getRc(RC_SOUND, "sounds/env/fan1", &audiostate.soundrcopt, 0, NULL);
        if (tmpsnd) {
            play3DSound(e3d[6], tmpsnd, AUDIOPRIO_DEFAULT, SOUNDFLAG_WRAP | SOUNDFLAG_LOOP, 0, NULL);
            rlsRc(tmpsnd, false);
        }
        tmpsnd = getRc(RC_SOUND, "sounds/env/vent1", &audiostate.soundrcopt, 0, NULL);
        if (tmpsnd) {
            play3DSound(e3d[6], tmpsnd, AUDIOPRIO_DEFAULT, SOUNDFLAG_WRAP | SOUNDFLAG_LOOP, 0, NULL);
            rlsRc(tmpsnd, false);
        }
        tmpsnd = getRc(RC_SOUND, "sounds/env/buzz1", &audiostate.soundrcopt, 0, NULL);
        if (tmpsnd) {
            play3DSound(e3d[7], tmpsnd, AUDIOPRIO_DEFAULT, SOUNDFLAG_WRAP | SOUNDFLAG_LOOP, 0, NULL);
            rlsRc(tmpsnd, false);
        }
    }

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

    stbi_write_png_compression_level = 9;

    tmptoff = SDL_GetTicks();
    framestamp = altutime();

    plog(LL_MS, "All systems go!");

    return 0;
}

void unstrap(void) {
    plog(LL_MS, "Stopping engine...");

    #if PLATFORM == PLAT_NXDK && PSRC_MTLVL >= 2
    armWatchdog(5);
    #endif
    plog(LL_INFO, "Stopping audio manager...");
    stopAudio();
    #if PLATFORM == PLAT_NXDK && PSRC_MTLVL >= 2
    rearmWatchdog(5);
    #endif
    plog(LL_INFO, "Stopping renderer...");
    stopRenderer();
    #if PLATFORM == PLAT_NXDK && PSRC_MTLVL >= 2
    cancelWatchdog();
    #endif

    rlsRc(mainscript, false);

    plog(LL_INFO, "Deinitializing audio manager...");
    quitAudio(true);
    plog(LL_INFO, "Deinitializing input manager...");
    quitInput();
    plog(LL_INFO, "Deinitializing UI manager...");
    quitUI();
    plog(LL_INFO, "Deinitializing renderer...");
    quitRenderer();
    plog(LL_INFO, "Deinitializing client...");
    quitClient();

    plog(LL_INFO, "Deinitializing resource manager...");
    quitRcMgr(true);

    if (dirs[DIR_USER]) {
        char* tmp = mkpath(dirs[DIR_USER], "config.cfg", NULL);
        //cfg_write(config, tmp);
        free(tmp);
        cfg_close(&config);
    }

    #if PLATFORM == PLAT_NXDK && PSRC_MTLVL >= 2
    armWatchdog(5);
    #endif
    SDL_Quit();
    #if PLATFORM == PLAT_NXDK && PSRC_MTLVL >= 2
    cancelWatchdog();
    #endif

    plog(LL_MS, "Done");
}

static inline float fwrap(float n, float d) {
    float tmp = n - (int)(n / d) * d;
    if (tmp < 0.0f) tmp += d;
    return tmp;
}
void loop(void) {
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

    long lt = SDL_GetTicks() - tmptoff;
    double dt = (double)(lt % 1000) / 1000.0;
    double t = (double)(lt / 1000) + dt;
    #if DEBUG(1)
    prof_begin(&dbgprof, DBGPROF_AUDIO);
    #endif
    edit3DAudioEmitter(
        e3d[2], 0, 0,
        0, NULL,
        AUDIO3DFXMASK_POS, &(struct audio3dfx){.pos = WORLDCOORD(0, 0, 0, (float)sin(t * 2.5) * 3.0f, 0.0, (float)cos(t * 2.5) * 3.0f)},
        0
    );
    //editAudioEmitter(testemt_obj, false, SOUNDFX_POS(sin(t * 2.5) * 3.0, 0.0, cos(t * 2.5) * 3.0));
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

    #if PSRC_MTLVL >= 2
    acquireWriteAccess(&playerdata.lock);
    #endif

    playerdata.data[0].camera.rot[0] += lookx * lookspeed[1];
    playerdata.data[0].camera.rot[1] += looky * lookspeed[0];
    //playerdata.data[0].camera.rot[2] = 90;
    if (playerdata.data[0].camera.rot[0] > 90.0f) playerdata.data[0].camera.rot[0] = 90.0f;
    else if (playerdata.data[0].camera.rot[0] < -90.0f) playerdata.data[0].camera.rot[0] = -90.0f;
    playerdata.data[0].camera.rot[1] = fwrap(playerdata.data[0].camera.rot[1], 360.0f);
    calcClientCameraAngles(&playerdata.data[0]);
    {
        // this can probably be optimized
        float tmp[2];
        tmp[0] = atan2f(fabsf(movex), fabsf(movez));
        tmp[0] = fabsf(1.0f / (cosf(tmp[0]) + sinf(tmp[0])));
        movex *= tmp[0];
        movez *= tmp[0];
        tmp[0] = movex * playerdata.data[0].common.cameracalc.cos[1] + movez * playerdata.data[0].common.cameracalc.sin[1];
        tmp[1] = movez * playerdata.data[0].common.cameracalc.cos[1] - movex * playerdata.data[0].common.cameracalc.sin[1];
        movex = tmp[0];
        movez = tmp[1];
    }
    playerdata.data[0].camera.pos.pos[0] += movex * speed * (float)framemult;
    playerdata.data[0].camera.pos.pos[2] += movez * speed * (float)framemult;
    playerdata.data[0].camera.pos.pos[1] += movey * jumpspeed * (float)framemult;

    updateClient(framemult);

    #if DEBUG(1)
    prof_begin(&dbgprof, DBGPROF_AUDIO);
    #endif
    updateAudio_unlocked(framemult);
    #if DEBUG(1)
    prof_begin(&dbgprof, DBGPROF_RENDERER);
    #endif
    render();

    #if PSRC_MTLVL >= 2
    releaseWriteAccess(&playerdata.lock);
    #endif

    #if DEBUG(1)
    prof_begin(&dbgprof, DBGPROF_RCMGR);
    #endif
    runRcMgr(altutime());
    #if DEBUG(1)
    prof_begin(&dbgprof, DBGPROF_RENDSWAP);
    #endif
    display();
    #if DEBUG(1)
    prof_end(&dbgprof);
    #endif

    if (screenshot) {
        unsigned w, h, ch;
        void* d = takeScreenshot(&w, &h, &ch);
        if (d) {
            stbi_write_png("screenshot.png", w, h, ch, d, w * ch);
            free(d);
        }
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
                        putchar('\x1b');
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

int parseargs(int argc, char** argv) {
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
            puts("    -gamesdir=DIR               Set the games directory.");
            puts("    -modsdir=DIR                Set the mods directory.");
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
            #if PSRC_MTLVL == 0
                puts("No multithreading");
            #elif PSRC_MTLVL == 1
                puts("Limited multithreading");
            #endif
            ret = 0;
        } else if (!strcmp(opt.data, "game")) {
            e = args_getoptval(&a, 1, -1, &val, &err);
            if (e == -1) {
                fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                ret = 1;
                break;
            }
            free(engine.opt.game);
            cb_reinit(&val, 256, &engine.opt.game);
        } else if (!strcmp(opt.data, "mods")) {
            e = args_getoptval(&a, 1, -1, &val, &err);
            if (e == -1) {
                fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                ret = 1;
                break;
            }
            free(engine.opt.mods);
            cb_reinit(&val, 256, &engine.opt.mods);
        } else if (!strcmp(opt.data, "icon")) {
            e = args_getoptval(&a, 1, -1, &val, &err);
            if (e == -1) {
                fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                ret = 1;
                break;
            }
            free(engine.opt.icon);
            cb_reinit(&val, 256, &engine.opt.icon);
        } else if (!strcmp(opt.data, "set") || !strcmp(opt.data, "s")) {
            e = args_getoptval(&a, 0, -1, &val, &err);
            if (e == -1) {
                fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                ret = 1;
                break;
            }
            if (!engine.opt.set__setup) {
                 cfg_open(NULL, &engine.opt.set);
                 engine.opt.set__setup = true;
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
                    cb_reinit(&var, 32, &sect);
                } else {
                    cfg_setvar(&engine.opt.set, sect, cb_peek(&var), cb_peek(&val), true);
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
            free(engine.opt.maindir);
            cb_reinit(&val, 256, &engine.opt.maindir);
        } else if (!strcmp(opt.data, "gamesdir")) {
            e = args_getoptval(&a, 1, -1, &val, &err);
            if (e == -1) {
                fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                ret = 1;
                break;
            }
            free(engine.opt.gamesdir);
            cb_reinit(&val, 256, &engine.opt.gamesdir);
        } else if (!strcmp(opt.data, "modsdir")) {
            e = args_getoptval(&a, 1, -1, &val, &err);
            if (e == -1) {
                fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                ret = 1;
                break;
            }
            free(engine.opt.modsdir);
            cb_reinit(&val, 256, &engine.opt.modsdir);
        } else if (!strcmp(opt.data, "userdir")) {
            e = args_getoptval(&a, 1, -1, &val, &err);
            if (e == -1) {
                fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                ret = 1;
                break;
            }
            free(engine.opt.userdir);
            cb_reinit(&val, 256, &engine.opt.userdir);
        } else if (!strcmp(opt.data, "config") || !strcmp(opt.data, "cfg") || !strcmp(opt.data, "c")) {
            e = args_getoptval(&a, 1, -1, &val, &err);
            if (e == -1) {
                fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                ret = 1;
                break;
            }
            free(engine.opt.config);
            cb_reinit(&val, 256, &engine.opt.config);
        } else if (!strcmp(opt.data, "nouserconfig") || !strcmp(opt.data, "nousercfg")) {
            e = args_getoptval(&a, 0, -1, &val, &err);
            if (e == -1) {
                fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                ret = 1;
                break;
            }
            engine.opt.nouserconfig = true;
        } else if (!strcmp(opt.data, "nocontroller")) {
            e = args_getoptval(&a, 0, -1, &val, &err);
            if (e == -1) {
                fprintf(stderr, "-%s: %s\n", opt.data, cb_peek(&err));
                ret = 1;
                break;
            }
            engine.opt.nocontroller = true;
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

#else

typedef int empty;

#endif
