#include "renderer.h"

#include "../version.h"

#include "../utils/logging.h"
//#include "../utils/threads.h"

#include "../../stb/stb_image.h"

#if PLATFORM != PLAT_NXDK
    // stuff to make sure i don't accidentally use gl things newer than 1.1 (will remove later)
    #if 0
    #include "../../.glad11/gl.h"
    #else
    #include "../../glad/gl.h"
    #endif
#else
    #include <pbkit/pbkit.h>
    #include <pbgl.h>
    #include <GL/gl.h>
    #include <GL/glext.h>
#endif
#ifndef GL_KHR_debug
    #define GL_KHR_debug 0
#endif

#include <string.h>
#include <stddef.h>
#include <math.h>
#include <stdarg.h>

struct rendstate rendstate;

const char* rendapi_ids[] = {
    "gl11",
    #if PLATFORM != PLAT_NXDK
    "gl33",
    "gles30"
    #endif
};
const char* rendapi_names[] = {
    "OpenGL 1.1",
    #if PLATFORM != PLAT_NXDK
    "OpenGL 3.3",
    "OpenGL ES 3.0"
    #endif
};

static void swapBuffers(void) {
    #if PLATFORM != PLAT_NXDK
    SDL_GL_SwapWindow(rendstate.window);
    #else
    pbgl_swap_buffers();
    #endif
}

static void gl11_cleardepth(void) {
    if (rendstate.gl.gl11.depthstate) {
        glDepthRange(0.0, 0.5);
        glDepthFunc(GL_LEQUAL);
    } else {
        glDepthRange(1.0, 0.5);
        glDepthFunc(GL_GEQUAL);
    }
    rendstate.gl.gl11.depthstate = !rendstate.gl.gl11.depthstate;
}

#if 0
void render_gl_legacy(void) {
    gl11_cleardepth();
    glLoadIdentity();

    if (rendstate.lighting >= 1) {
        // TODO: render opaque materials front to back
    } else {
        // TODO: render opaque materials front to back with basic lighting
    }

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    if (rendstate.lighting >= 1) {
        glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
        // TODO: render opaque light map front to back
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // TODO: render transparent materials back to front with basic lighting

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // TODO: render UI
}
#else
void render_gl_legacy(void) {
    long lt = SDL_GetTicks();
    double dt = (double)(lt % 1000) / 1000.0;
    double t = (double)(lt / 1000) + dt;
    float tsin = sin(t * 0.827535 * M_PI);
    float tsin2 = sin(t * 0.628591 * M_PI);
    float tsinn = sin(t * M_PI) * 0.5 + 0.5;
    float tcosn = cos(t * M_PI) * 0.5 + 0.5;
    float tsini = 1.0 - tsinn;
    float tcosi = 1.0 - tcosn;
    gl11_cleardepth();
    glLoadIdentity();
    glDisable(GL_BLEND);
    glBegin(GL_QUADS);
        #if 1
        glColor3f(tsini, tcosn, tsinn);
        glVertex3f(-1.0, 1.0, 0.0);
        glColor3f(tcosi, tsini, tcosn);
        glVertex3f(1.0, 1.0, 0.0);
        glColor3f(tsinn, tcosi, tsini);
        glVertex3f(1.0, -1.0, 0.0);
        glColor3f(tcosn, tsinn, tcosi);
        glVertex3f(-1.0, -1.0, 0.0);
        #endif
        glColor3f(1.0, 0.0, 0.0);
        glVertex3f(-0.5, 0.5, 0.0);
        glColor3f(0.5, 1.0, 0.0);
        glVertex3f(0.5, 0.5, 0.0);
        glColor3f(0.0, 1.0, 1.0);
        glVertex3f(0.5, -0.5, 0.0);
        glColor3f(0.5, 0.0, 1.0);
        glVertex3f(-0.5, -0.5, 0.0);
        glColor3f(0.0, 0.0, 0.0);
        glVertex3f(-1.0, 0.025 + tsin2, 0.0);
        glVertex3f(1.0, 0.025 + tsin2, 0.0);
        glVertex3f(1.0, -0.025 + tsin2, 0.0);
        glVertex3f(-1.0, -0.025 + tsin2, 0.0);
        glColor3f(1.0, 1.0, 1.0);
        glVertex3f(-0.025 + tsin, 1.0, 0.0);
        glVertex3f(0.025 + tsin, 1.0, 0.0);
        glVertex3f(0.025 + tsin, -1.0, 0.0);
        glVertex3f(-0.025 + tsin, -1.0, 0.0);
    glEnd();
    glEnable(GL_BLEND);
    glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
        glColor3f(1.0, 1.0, 1.0);
        glVertex3f(-0.75, 0.75, 0.0);
        glColor3f(0.5, 0.5, 0.5);
        glVertex3f(0.75, 0.75, 0.0);
        glColor3f(0.0, 0.0, 0.0);
        glVertex3f(0.75, -0.75, 0.0);
        glColor3f(0.5, 0.5, 0.5);
        glVertex3f(-0.75, -0.75, 0.0);
    glEnd();
}
#endif

#if PLATFORM != PLAT_NXDK
void render_gl_advanced(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (rendstate.lighting >= 1) {
        // TODO: render opaque materials front to back with light mapping
    } else {
        // TODO: render opaque materials front to back with basic lighting
    }

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    if (rendstate.lighting >= 2) {
        // TODO: render transparent materials back to front with light mapping
    } else {
        // TODO: render transparent materials back to front with basic lighting
    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    // TODO: render UI
}
#endif

void render(void) {
    switch (rendstate.apigroup) {
        case RENDAPIGROUP_GL: {
            switch (rendstate.api) {
                case RENDAPI_GL11:
                    render_gl_legacy();
                    break;
                #if PLATFORM != PLAT_NXDK
                case RENDAPI_GL33:
                case RENDAPI_GLES30:
                    render_gl_advanced();
                    break;
                #endif
                default:
                    break;
            }
        } break;
        default: {
        } break;
    }
    #if PLATFORM == PLAT_NXDK
    static uint64_t ticks = 0;
    static bool cleared = false;
    if (plog__wrote) {
        plog__wrote = false;
        ticks = SDL_GetTicks() + 5000;
        cleared = false;
    }
    if (SDL_TICKS_PASSED(SDL_GetTicks(), ticks)) {
        if (!cleared) {
            pb_erase_text_screen();
            cleared = true;
        }
    }
    pb_draw_text_screen();
    #endif
    swapBuffers();
}

static void destroyWindow(void) {
    if (rendstate.window != NULL) {
        switch (rendstate.apigroup) {
            case RENDAPIGROUP_GL:
                #if PLATFORM != PLAT_NXDK
                SDL_GL_DeleteContext(rendstate.gl.ctx);
                #endif
                break;
            default:
                break;
        }
        SDL_Window* w = rendstate.window;
        rendstate.window = NULL;
        SDL_DestroyWindow(w);
    }
}

static void updateWindowRes(void) {
    switch (rendstate.mode) {
        case RENDMODE_WINDOWED:
            rendstate.res.current = rendstate.res.windowed;
            SDL_SetWindowFullscreen(rendstate.window, 0);
            SDL_SetWindowSize(rendstate.window, rendstate.res.windowed.width, rendstate.res.windowed.height);
            break;
        case RENDMODE_BORDERLESS:
            rendstate.res.current = rendstate.res.fullscr;
            SDL_SetWindowSize(rendstate.window, rendstate.res.fullscr.width, rendstate.res.fullscr.height);
            SDL_SetWindowFullscreen(rendstate.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            break;
        case RENDMODE_FULLSCREEN:
            rendstate.res.current = rendstate.res.fullscr;
            SDL_SetWindowSize(rendstate.window, rendstate.res.fullscr.width, rendstate.res.fullscr.height);
            SDL_SetWindowFullscreen(rendstate.window, SDL_WINDOW_FULLSCREEN);
            break;
    }
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
        SDL_SetWindowIcon(rendstate.window, s);
        SDL_FreeSurface(s);
        stbi_image_free(data);
    } else {
        plog(LL_WARN, "Failed to set window icon");
    }
}
#endif

#define SDL_GL_SetAttribute(a, v) if (SDL_GL_SetAttribute((a), (v))) plog(LL_WARN, "Failed to set " #a " to " #v ": %s", SDL_GetError())
#define SDL_SetHint(n, v) if (!SDL_SetHint((n), (v))) plog(LL_WARN, "Failed to set " #n " to %s: %s", (char*)(v), SDL_GetError())
#define SDL_SetHintWithPriority(n, v, p) if (!SDL_SetHintWithPriority((n), (v), (p))) plog(LL_WARN, "Failed to set " #n " to %s using " #p ": %s", (char*)(v), SDL_GetError())

static bool createWindow(void) {
    if (rendstate.api <= RENDAPI__INVAL || rendstate.api >= RENDAPI__COUNT) {
        plog(LL_CRIT, "Invalid rendering API (%d)", (int)rendstate.api);
        return false;
    }
    plog(LL_INFO, "Creating window for %s...", rendapi_names[rendstate.api]);
    switch (rendstate.api) {
        #if 1
        case RENDAPI_GL11:
            rendstate.apigroup = RENDAPIGROUP_GL;
            #if PLATFORM != PLAT_NXDK
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            #endif
            break;
        #endif
        #if PLATFORM != PLAT_NXDK
        #if 0
        case RENDAPI_GL33:
            rendstate.apigroup = RENDAPIGROUP_GL;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            break;
        #endif
        #if 0
        case RENDAPI_GLES30:
            rendstate.apigroup = RENDAPIGROUP_GL;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
            break;
        #endif
        #endif
        default:
            plog(LL_CRIT, "%s not implemented", rendapi_names[rendstate.api]);
            return false;
            break;
    }
    #if PLATFORM != PLAT_NXDK
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    #endif
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1", SDL_HINT_OVERRIDE);
    //SDL_SetRelativeMouseMode(SDL_TRUE);
    uint32_t flags = 0;
    #if PLATFORM != PLAT_NXDK
    flags |= SDL_WINDOW_RESIZABLE;
    if (rendstate.apigroup == RENDAPIGROUP_GL) flags |= SDL_WINDOW_OPENGL;
    #endif
    rendstate.window = SDL_CreateWindow(
        titlestr,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        rendstate.res.windowed.width, rendstate.res.windowed.height,
        SDL_WINDOW_SHOWN | flags
    );
    if (!rendstate.window) {
        plog(LL_CRIT | LF_FUNCLN, "Failed to create window: %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(rendstate.window, 320, 240);
    #if PLATFORM != PLAT_NXDK
    updateWindowIcon();
    #endif
    SDL_DisplayMode dtmode;
    SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(rendstate.window), &dtmode);
    rendstate.res.fullscr = (struct rendres){dtmode.w, dtmode.h, dtmode.refresh_rate};
    updateWindowRes();
    switch (rendstate.apigroup) {
        case RENDAPIGROUP_GL: {
            #if PLATFORM != PLAT_NXDK
            rendstate.gl.ctx = SDL_GL_CreateContext(rendstate.window);
            if (!rendstate.gl.ctx) {
                plog(LL_CRIT | LF_FUNCLN, "Failed to create OpenGL context: %s", SDL_GetError());
                rendstate.apigroup = RENDAPIGROUP__INVAL;
                destroyWindow();
                return false;
            }
            if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
                plog(LL_CRIT | LF_FUNCLN, "Failed to load GLAD");
                destroyWindow();
                return false;
            }
            SDL_GL_SetSwapInterval(rendstate.vsync);
            #else
            pbgl_set_swap_interval(rendstate.vsync);
            #endif
            plog(LL_INFO, "OpenGL info:");
            bool cond[4];
            int tmpint[4];
            char* tmpstr[1];
            cond[0] = !SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &tmpint[0]);
            cond[1] = !SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &tmpint[1]);
            cond[2] = !SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &tmpint[2]);
            if (cond[2]) {
                switch (tmpint[2]) {
                    default: tmpstr[0] = ""; break;
                    case SDL_GL_CONTEXT_PROFILE_CORE: tmpstr[0] = " core"; break;
                    case SDL_GL_CONTEXT_PROFILE_COMPATIBILITY: tmpstr[0] = " compat"; break;
                    case SDL_GL_CONTEXT_PROFILE_ES: tmpstr[0] = " ES"; break;
                }
            }
            if (cond[0] && cond[1]) {
                plog(LL_INFO, "  Requested OpenGL version: %d.%d%s", tmpint[0], tmpint[1], tmpstr[0]);
            }
            tmpstr[0] = (char*)glGetString(GL_VERSION);
            plog(LL_INFO, "  OpenGL version: %s", (tmpstr[0]) ? tmpstr[0] : "?");
            #ifdef GL_SHADING_LANGUAGE_VERSION
            tmpstr[0] = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
            if (tmpstr[0]) plog(LL_INFO, "  GLSL version: %s", tmpstr[0]);
            #endif
            tmpstr[0] = (char*)glGetString(GL_VENDOR);
            if (tmpstr[0]) plog(LL_INFO, "  Vendor string: %s", tmpstr[0]);
            tmpstr[0] = (char*)glGetString(GL_RENDERER);
            if (tmpstr[0]) plog(LL_INFO, "  Renderer string: %s", tmpstr[0]);
            cond[0] = !SDL_GL_GetAttribute(SDL_GL_ACCELERATED_VISUAL, &tmpint[0]);
            if (cond[0]) plog(LL_INFO, "  Hardware acceleration is %s", (tmpint[0]) ? "enabled" : "disabled");
            cond[0] = !SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &tmpint[0]);
            if (cond[0]) plog(LL_INFO, "  Double-buffering is %s", (tmpint[0]) ? "enabled" : "disabled");
            cond[0] = !SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &tmpint[0]);
            cond[1] = !SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &tmpint[1]);
            cond[2] = !SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &tmpint[2]);
            cond[3] = !SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &tmpint[3]);
            if (cond[0] && cond[1] && cond[2] && cond[3]) {
                plog(LL_INFO, "  Color buffer format: R%dG%dB%dA%d", tmpint[0], tmpint[1], tmpint[2], tmpint[3]);
            }
            cond[0] = !SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &tmpint[0]);
            if (cond[0]) {
                plog(LL_INFO, "  Depth buffer format: D%d", tmpint[0]);
            }
            if (GL_KHR_debug) plog(LL_INFO, "  GL_KHR_debug is supported");
            glClearColor(0.0, 0.0, 0.1, 1.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            swapBuffers();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            swapBuffers();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
        } break;
        default: {
        } break;
    }
    return true;
}

static bool prepRenderer(void) {
    return true;
}

static bool startRenderer_internal(void) {
    if (!createWindow()) return false;
    return prepRenderer();
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
            case RENDOPT_MODE: {
                rendstate.mode = va_arg(args, enum rendmode);
                updateWindowRes();
            } break;
            case RENDOPT_VSYNC: {
                bool vsync = va_arg(args, int);
                rendstate.vsync = vsync;
                if (rendstate.apigroup == RENDAPIGROUP_GL) {
                    #if PLATFORM != PLAT_NXDK
                    SDL_GL_SetSwapInterval(vsync);
                    #else
                    pbgl_set_swap_interval(vsync);
                    #endif
                }
            } break;
            case RENDOPT_RES: {
                struct rendres* res = va_arg(args, struct rendres*);
                if (res->width >= 0) rendstate.res.current.width = res->width;
                if (res->height >= 0) rendstate.res.current.height = res->height;
                if (res->hz >= 0) rendstate.res.current.hz = res->hz;
                if (rendstate.apigroup == RENDAPIGROUP_GL) {
                    glViewport(0, 0, res->width, res->height);
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
    rendstate.api = RENDAPI_GL11;
    #if PLATFORM != PLAT_NXDK
    rendstate.res.windowed = (struct rendres){800, 600, 60};
    #else
    rendstate.res.windowed = (struct rendres){640, 480, 60};
    #endif
    rendstate.vsync = false;
    if (SDL_Init(SDL_INIT_VIDEO)) {
        plog(LL_CRIT | LF_FUNCLN, "Failed to init video: %s", SDL_GetError());
        return false;
    }
    return true;
}

void termRenderer(void) {
    free(rendstate.icon);
}
