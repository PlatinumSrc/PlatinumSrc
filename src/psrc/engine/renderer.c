#include "renderer.h"

#include "../version.h"
#include "../debug.h"
#include "../common.h"

#include "../common/logging.h"
#include "../common/string.h"
//#include "../common/threads.h"
#include "../common/p3m.h"

#include "../../stb/stb_image.h"

#if PLATFORM == PLAT_EMSCR
    #include <emscripten/html5.h>
#endif

#include <string.h>
#include <stddef.h>
#include <math.h>
#include <stdarg.h>

struct rendstate rendstate;

const char* rendapi_ids[] = {
    #ifdef PSRC_USEGL
    #ifdef PSRC_USEGL11
    "gl11",
    #endif
    #ifdef PSRC_USEGL33
    "gl33",
    #endif
    #ifdef PSRC_USEGLES30
    "gles30"
    #endif
    #endif
};
const char* rendapi_names[] = {
    #ifdef PSRC_USEGL
    #ifdef PSRC_USEGL11
    "OpenGL 1.1",
    #endif
    #ifdef PSRC_USEGL33
    "OpenGL 3.3",
    #endif
    #ifdef PSRC_USEGLES30
    "OpenGL ES 3.0"
    #endif
    #endif
};

static struct rc_model* testmodel;

#ifdef PSRC_USEGL
    #include "renderer/gl.c"
#endif

void (*render)(void);
void (*display)(void);
void* (*takeScreenshot)(int* w, int* h, int* sz);
static bool (*beforeCreateWindow)(unsigned*);
static bool (*afterCreateWindow)(void);
static bool (*prepRenderer)(void);
static void (*beforeDestroyWindow)(void);
static void (*calcProjMat)(void);
static void (*updateFrame)(void);
static void (*updateVSync)(void);

static void destroyWindow(void) {
    #ifndef PSRC_USESDL1
    if (rendstate.window != NULL) {
        beforeDestroyWindow();
        SDL_Window* w = rendstate.window;
        rendstate.window = NULL;
        SDL_DestroyWindow(w);
    }
    #else
    beforeDestroyWindow();
    #endif
}

static void updateWindowMode(enum rendmode newmode) {
    switch (newmode) {
        case RENDMODE_WINDOWED: {
            if (rendstate.mode != RENDMODE_WINDOWED) {
                rendstate.res.current = rendstate.res.windowed;
                #if PLATFORM != PLAT_EMSCR
                #ifndef PSRC_USESDL1
                SDL_SetWindowFullscreen(rendstate.window, 0);
                #endif
                #else
                emscripten_exit_fullscreen();
                #endif
                rendstate.mode = RENDMODE_WINDOWED;
            }
            #ifndef PSRC_USESDL1
            SDL_SetWindowSize(rendstate.window, rendstate.res.current.width, rendstate.res.current.height);
            #else
            SDL_SetVideoMode(rendstate.res.current.width, rendstate.res.current.height, rendstate.bpp, rendstate.flags);
            #endif
        } break;
        case RENDMODE_BORDERLESS: {
            #if PLATFORM != PLAT_EMSCR
            if (rendstate.mode != RENDMODE_BORDERLESS) {
                rendstate.res.current = rendstate.res.fullscr;
                rendstate.mode = RENDMODE_BORDERLESS;
            }
            #ifndef PSRC_USESDL1
            SDL_DisplayMode mode;
            SDL_GetCurrentDisplayMode(0, &mode);
            mode.w = rendstate.res.current.width;
            mode.h = rendstate.res.current.height;
            SDL_SetWindowDisplayMode(rendstate.window, &mode);
            SDL_SetWindowFullscreen(rendstate.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            #else
            SDL_SetVideoMode(rendstate.res.current.width, rendstate.res.current.height, rendstate.bpp, rendstate.flags | SDL_NOFRAME);
            #endif
            #else
            if (emscripten_request_fullscreen("#canvas", false) == EMSCRIPTEN_RESULT_SUCCESS) {
                rendstate.res.current = rendstate.res.fullscr;
                SDL_SetWindowSize(rendstate.window, rendstate.res.current.width, rendstate.res.current.height);
            } else {
                plog(LL_WARN, "Failed to go to fullscreen (canvas has probably not acquired an input lock)");
                rendstate.mode = RENDMODE_WINDOWED;
            }
            #endif
        } break;
        case RENDMODE_FULLSCREEN: {
            #if PLATFORM != PLAT_EMSCR
            if (rendstate.mode != RENDMODE_FULLSCREEN) {
                rendstate.res.current = rendstate.res.fullscr;
                rendstate.mode = RENDMODE_FULLSCREEN;
            }
            #ifndef PSRC_USESDL1
            SDL_DisplayMode mode;
            SDL_GetCurrentDisplayMode(0, &mode);
            mode.w = rendstate.res.current.width;
            mode.h = rendstate.res.current.height;
            SDL_SetWindowDisplayMode(rendstate.window, &mode);
            SDL_SetWindowFullscreen(rendstate.window, SDL_WINDOW_FULLSCREEN);
            #else
            SDL_SetVideoMode(rendstate.res.current.width, rendstate.res.current.height, rendstate.bpp, rendstate.flags | SDL_FULLSCREEN);
            #endif
            #else
            if (emscripten_request_fullscreen("#canvas", false) == EMSCRIPTEN_RESULT_SUCCESS) {
                rendstate.res.current = rendstate.res.fullscr;
                SDL_SetWindowSize(rendstate.window, rendstate.res.current.width, rendstate.res.current.height);
            } else {
                plog(LL_WARN, "Failed to go to fullscreen (canvas has probably not acquired an input lock)");
                rendstate.mode = RENDMODE_WINDOWED;
            }
            #endif
        } break;
    }
    rendstate.aspect = (double)rendstate.res.current.width / (double)rendstate.res.current.height;
}

#if PLATFORM != PLAT_NXDK
static void updateWindowIcon(void) {
    int w, h, c;
    void* data = stbi_load(rendstate.icon, &w, &h, &c, STBI_rgb_alpha);
    if (data) {
        SDL_Surface* s = SDL_CreateRGBSurfaceFrom(
            data, w, h, 32, w * 4,
            0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
        );
        #ifndef PSRC_USESDL1
        SDL_SetWindowIcon(rendstate.window, s);
        #else
        SDL_WM_SetIcon(s, NULL);
        #endif
        SDL_FreeSurface(s);
        stbi_image_free(data);
    } else {
        plog(LL_WARN, "Failed to set window icon");
    }
}
#endif

#ifndef PSRC_USESDL1
#define SDL_SetHint(n, v) if (!SDL_SetHint((n), (v))) plog(LL_WARN, "Failed to set " #n " to %s: %s", (char*)(v), SDL_GetError())
#define SDL_SetHintWithPriority(n, v, p) if (!SDL_SetHintWithPriority((n), (v), (p))) plog(LL_WARN, "Failed to set " #n " to %s using " #p ": %s", (char*)(v), SDL_GetError())
#endif
static bool createWindow(void) {
    if (rendstate.api <= RENDAPI__INVAL || rendstate.api >= RENDAPI__COUNT) {
        plog(LL_CRIT, "Invalid rendering API (%d)", (int)rendstate.api);
        return false;
    }
    plog(LL_INFO, "Creating window for %s...", rendapi_names[rendstate.api]);
    #ifndef PSRC_USESDL1
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    //SDL_SetRelativeMouseMode(SDL_TRUE);
    #endif
    unsigned flags;
    #ifndef PSRC_USESDL1
    #if PLATFORM != PLAT_NXDK
    flags = SDL_WINDOW_RESIZABLE;
    #else
    flags = 0;
    #endif
    {
        SDL_DisplayMode dtmode;
        SDL_GetDesktopDisplayMode(0, &dtmode);
        if (rendstate.res.fullscr.width < 0) rendstate.res.fullscr.width = dtmode.w;
        if (rendstate.res.fullscr.height < 0) rendstate.res.fullscr.height = dtmode.h;
        if (rendstate.fps < 0) rendstate.fps = dtmode.refresh_rate;
    }
    switch (rendstate.mode) {
        case RENDMODE_WINDOWED:
            rendstate.res.current = rendstate.res.windowed;
            break;
        case RENDMODE_BORDERLESS:
            rendstate.res.current = rendstate.res.fullscr;
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            break;
        case RENDMODE_FULLSCREEN:
            rendstate.res.current = rendstate.res.fullscr;
            flags |= SDL_WINDOW_FULLSCREEN;
            break;
    }
    if (!beforeCreateWindow(&flags)) {
        rendstate.apigroup = RENDAPIGROUP__INVAL;
        return false;
    }
    #else
    flags = 0;
    rendstate.flags = SDL_ANYFORMAT;
    switch (rendstate.mode) {
        case RENDMODE_WINDOWED:
            rendstate.res.current = rendstate.res.windowed;
            break;
        case RENDMODE_BORDERLESS:
            rendstate.res.current = rendstate.res.fullscr;
            flags |= SDL_NOFRAME;
            break;
        case RENDMODE_FULLSCREEN:
            rendstate.res.current = rendstate.res.fullscr;
            flags |= SDL_FULLSCREEN;
            break;
    }
    if (!beforeCreateWindow(&rendstate.flags)) {
        rendstate.apigroup = RENDAPIGROUP__INVAL;
        return false;
    }
    #endif
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG, "Windowed resolution: %dx%d", rendstate.res.windowed.width, rendstate.res.windowed.height);
    plog(LL_INFO | LF_DEBUG, "Fullscreen resolution: %dx%d", rendstate.res.fullscr.width, rendstate.res.fullscr.height);
    #endif
    rendstate.aspect = (double)rendstate.res.current.width / (double)rendstate.res.current.height;
    #ifndef PSRC_USESDL1
    rendstate.window = SDL_CreateWindow(
        titlestr,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        rendstate.res.current.width, rendstate.res.current.height,
        SDL_WINDOW_SHOWN | flags
    );
    if (!rendstate.window) {
        plog(LL_CRIT | LF_FUNCLN, "Failed to create window: %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(rendstate.window, 320, 240);
    #else
    if (!SDL_SetVideoMode(rendstate.res.current.width, rendstate.res.current.height, rendstate.bpp, rendstate.flags | flags)) {
        plog(LL_CRIT | LF_FUNCLN, "Failed to create window: %s", SDL_GetError());
        return false;
    }
    SDL_WM_SetCaption(titlestr, NULL);
    #endif
    #if PLATFORM != PLAT_NXDK
    updateWindowIcon();
    #endif
    if (!afterCreateWindow()) {
        rendstate.apigroup = RENDAPIGROUP__INVAL;
        destroyWindow();
        return false;
    }
    return true;
}

static bool startRenderer_internal(void) {
    switch (rendstate.api) {
        #ifdef PSRC_USEGL
        #ifdef PSRC_USEGL11
        case RENDAPI_GL11: rendstate.apigroup = RENDAPIGROUP_GL; break;
        #endif
        #ifdef PSRC_USEGL33
        case RENDAPI_GL33: rendstate.apigroup = RENDAPIGROUP_GL; break;
        #endif
        #ifdef PSRC_USEGLES30
        case RENDAPI_GLES30: rendstate.apigroup = RENDAPIGROUP_GL; break;
        #endif
        #endif
        default: return false;
    }
    switch (rendstate.apigroup) {
        #ifdef PSRC_USEGL
        case RENDAPIGROUP_GL:
            render = gl_render;
            display = gl_display;
            takeScreenshot = gl_takeScreenshot;
            beforeCreateWindow = gl_beforeCreateWindow;
            afterCreateWindow = gl_afterCreateWindow;
            prepRenderer = gl_prepRenderer;
            beforeDestroyWindow = gl_beforeDestroyWindow;
            calcProjMat = gl_calcProjMat;
            updateFrame = gl_updateFrame;
            updateVSync = gl_updateVSync;
            break;
        #endif
        default:
            return false;
    }
    if (!createWindow()) return false;
    if (!prepRenderer()) {
        rendstate.apigroup = RENDAPIGROUP__INVAL;
        destroyWindow();
        return false;
    }
    return true;
}

static void stopRenderer_internal(void) {
    destroyWindow();
}

bool reloadRenderer(void) {
    stopRenderer_internal();
    return startRenderer_internal();
}

bool startRenderer(void) {
    return startRenderer_internal();
}

void stopRenderer(void) {
    stopRenderer_internal();
}

bool updateRendererConfig(enum rendopt opt, ...) {
    va_list args;
    va_start(args, opt);
    while (1) {
        switch (opt) {
            case RENDOPT_END: {
                goto rettrue;
            } break;
            case RENDOPT_ICON: {
                free(rendstate.icon);
                rendstate.icon = strdup(va_arg(args, char*));
                #if PLATFORM != PLAT_NXDK
                updateWindowIcon();
                #endif
            } break;
            case RENDOPT_API: {
                enum rendapi oldapi = rendstate.api;
                stopRenderer_internal();
                rendstate.api = va_arg(args, enum rendapi);
                if (!startRenderer_internal()) {
                    plog(
                        LL_WARN,
                        "Failed to restart renderer after changing API to %s. Reverting to %s...",
                        rendapi_names[rendstate.api], rendapi_names[oldapi]
                    );
                    rendstate.api = oldapi;
                    if (!startRenderer_internal()) {
                        plog(LL_ERROR, "Failed to restart renderer after reverting API to %s.", rendapi_names[rendstate.api]);
                        goto retfalse;
                    }
                }
            } break;
            case RENDOPT_FULLSCREEN: {
                int tmp = va_arg(args, int);
                enum rendmode newmode;
                if (tmp < 0) {
                    newmode = (rendstate.mode == RENDMODE_WINDOWED) ?
                        ((rendstate.borderless) ? RENDMODE_BORDERLESS : RENDMODE_FULLSCREEN) :
                        RENDMODE_WINDOWED;
                } else if (tmp) {
                    newmode = (rendstate.borderless) ? RENDMODE_BORDERLESS : RENDMODE_FULLSCREEN;
                } else {
                    newmode = RENDMODE_WINDOWED;
                }
                updateWindowMode(newmode);
                updateFrame();
            } break;
            case RENDOPT_BORDERLESS: {
                rendstate.borderless = va_arg(args, int);
                enum rendmode newmode = (rendstate.mode == RENDMODE_BORDERLESS || rendstate.mode == RENDMODE_FULLSCREEN) ?
                    ((rendstate.borderless) ? RENDMODE_BORDERLESS : RENDMODE_FULLSCREEN) :
                    RENDMODE_WINDOWED;
                updateWindowMode(newmode);
                updateFrame();
            } break;
            case RENDOPT_VSYNC: {
                rendstate.vsync = va_arg(args, int);
                updateVSync();
            } break;
            case RENDOPT_FOV: {
                rendstate.fov = va_arg(args, double);
                calcProjMat();
            } break;
            case RENDOPT_RES: {
                struct rendres* res = va_arg(args, struct rendres*);
                if (res->width >= 0) rendstate.res.current.width = res->width;
                if (res->height >= 0) rendstate.res.current.height = res->height;
                switch (rendstate.mode) {
                    case RENDMODE_WINDOWED:
                        if (rendstate.res.current.width != rendstate.res.windowed.width ||
                            rendstate.res.current.height != rendstate.res.windowed.height) {
                            rendstate.res.windowed = rendstate.res.current;
                            updateWindowMode(rendstate.mode);
                            updateFrame();
                        }
                        break;
                    case RENDMODE_BORDERLESS:
                    case RENDMODE_FULLSCREEN:
                        if (rendstate.res.current.width != rendstate.res.fullscr.width ||
                            rendstate.res.current.height != rendstate.res.fullscr.height) {
                            rendstate.res.fullscr = rendstate.res.current;
                            updateWindowMode(rendstate.mode);
                            updateFrame();
                        }
                        break;
                }
            } break;
            case RENDOPT_LIGHTING: {
                rendstate.lighting = va_arg(args, enum rendlighting);
            } break;
            case RENDOPT_TEXTUREQLT: {
                rendstate.texqlt = va_arg(args, enum rcopt_texture_qlt);
            } break;
        }
        opt = va_arg(args, enum rendopt);
    }
    rettrue:;
    va_end(args);
    return true;
    retfalse:;
    va_end(args);
    return false;
}

bool initRenderer(void) {
    if (SDL_Init(SDL_INIT_VIDEO)) {
        plog(LL_CRIT | LF_FUNCLN, "Failed to init video: %s", SDL_GetError());
        return false;
    }
    #if defined(PSRC_USEGL)
    #if defined(PSRC_USEGL11)
    rendstate.api = RENDAPI_GL11;
    #elif defined(PSRC_USEGLES30)
    rendstate.api = RENDAPI_GLES30;
    #else
    rendstate.api = RENDAPI_GL33;
    #endif
    #else
    rendstate.api = RENDAPI__INVAL;
    #endif
    char* tmp = cfg_getvar(config, "Renderer", "resolution.windowed");
    #if PLATFORM == PLAT_EMSCR
    rendstate.res.windowed = (struct rendres){1024, 768};
    #elif PLATFORM == PLAT_NXDK || PLATFORM == PLAT_DREAMCAST
    rendstate.res.windowed = (struct rendres){640, 480};
    #else
    rendstate.res.windowed = (struct rendres){800, 600};
    #endif
    if (tmp) {
        sscanf(
            tmp, "%dx%d",
            &rendstate.res.windowed.width,
            &rendstate.res.windowed.height
        );
        free(tmp);
    }
    tmp = cfg_getvar(config, "Renderer", "resolution.fullscreen");
    rendstate.res.fullscr = (struct rendres){-1, -1};
    if (tmp) {
        sscanf(
            tmp, "%dx%d",
            &rendstate.res.fullscr.width,
            &rendstate.res.fullscr.height
        );
        free(tmp);
    }
    tmp = cfg_getvar(config, "Renderer", "fps");
    rendstate.fps = -1;
    if (tmp) {
        sscanf(tmp, "%d", &rendstate.fps);
        free(tmp);
    }
    #ifdef PSRC_USESDL1
    {
        const SDL_VideoInfo* vinf = SDL_GetVideoInfo();
        rendstate.bpp = vinf->vfmt->BitsPerPixel;
        if (rendstate.res.fullscr.width < 0) rendstate.res.fullscr.width = vinf->current_w;
        if (rendstate.res.fullscr.height < 0) rendstate.res.fullscr.height = vinf->current_h;
        if (rendstate.fps < 0) rendstate.fps = 60; // TODO: get the actual hz somehow?
    }
    #endif
    tmp = cfg_getvar(config, "Renderer", "borderless");
    if (tmp) {
        rendstate.borderless = strbool(tmp, false);
        free(tmp);
    } else {
        rendstate.borderless = false;
    }
    tmp = cfg_getvar(config, "Renderer", "fullscreen");
    rendstate.mode = (strbool(tmp, false)) ?
        ((rendstate.borderless) ? RENDMODE_BORDERLESS : RENDMODE_FULLSCREEN) :
        RENDMODE_WINDOWED;
    free(tmp);
    tmp = cfg_getvar(config, "Renderer", "vsync");
    if (tmp) {
        rendstate.vsync = strbool(tmp, true);
        free(tmp);
    } else {
        rendstate.vsync = true;
    }
    tmp = cfg_getvar(config, "Renderer", "fov");
    if (tmp) {
        rendstate.fov = atof(tmp);
        free(tmp);
    } else {
        rendstate.fov = 90.0;
    }
    testmodel = loadResource(RC_MODEL, "game:test/test_model", NULL);
    return true;
}

void quitRenderer(void) {
    freeResource(testmodel);
    free(rendstate.icon);
}
