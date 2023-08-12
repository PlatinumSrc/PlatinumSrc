#include "renderer.h"
#include "../psrc_aux/logging.h"
//#include "../psrc_aux/threads.h"
#if PLATFORM != PLAT_XBOX
    // stuff to make sure i don't accidentally use gl things newer than 1.1
    #if 1
    #include "../.glad11/gl.h"
    #else
    #include "../glad/gl.h"
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
#include "../stb/stb_image.h"

#include <string.h>
#include <stddef.h>
#include <math.h>

const char* rendapi_ids[] = {
    "gl11",
    "gl33",
    "gles30"
};
const char* rendapi_names[] = {
    "OpenGL 1.1 (Legacy)",
    "OpenGL 3.3 (Advanced)",
    "OpenGL ES 3.0"
};

static void swapBuffers(struct rendstate* r) {
    #if PLATFORM != PLAT_XBOX
    SDL_GL_SwapWindow(r->window);
    #else
    (void)r;
    pbgl_swap_buffers();
    #endif
}

static void gl11_cleardepth(struct rendstate* r) {
    if (r->gl.gl11.depthstate) {
        glDepthRange(0.0, 0.5);
        glDepthFunc(GL_LEQUAL);
    } else {
        glDepthRange(1.0, 0.5);
        glDepthFunc(GL_GEQUAL);
    }
    r->gl.gl11.depthstate = !r->gl.gl11.depthstate;
}

#if 0
void render_gl_legacy(struct rendstate* r) {
    gl11_cleardepth(r);
    glLoadIdentity();

    if (r->cfg.lighting >= 1) {
        // TODO: render opaque materials front to back
    } else {
        // TODO: render opaque materials front to back with basic lighting
    }

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    if (r->cfg.lighting >= 1) {
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
void render_gl_legacy(struct rendstate* r) {
    long lt = SDL_GetTicks();
    double dt = (double)(lt % 1000) / 1000.0;
    double t = (double)(lt / 1000) + dt;
    float tsinn = sin(t * M_PI) * 0.5 + 0.5;
    float tcosn = cos(t * M_PI) * 0.5 + 0.5;
    float tsini = 1.0 - tsinn;
    float tcosi = 1.0 - tcosn;
    gl11_cleardepth(r);
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

#if PLATFORM != PLAT_XBOX
void render_gl_advanced(struct rendstate* r) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (r->cfg.lighting >= 1) {
        // TODO: render opaque materials front to back with light mapping
    } else {
        // TODO: render opaque materials front to back with basic lighting
    }

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    if (r->cfg.lighting >= 2) {
        // TODO: render transparent materials back to front with light mapping
    } else {
        // TODO: render transparent materials back to front with basic lighting
    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    // TODO: render UI
}
#endif

void render(struct rendstate* r) {
    switch (r->apigroup) {
        case RENDAPIGROUP_GL:; {
            switch (r->cfg.api) {
                case RENDAPI_GL11:;
                    render_gl_legacy(r);
                    break;
                #if PLATFORM != PLAT_XBOX
                case RENDAPI_GL33:;
                case RENDAPI_GLES30:;
                    render_gl_advanced(r);
                    break;
                #endif
                default:;
                    break;
            }
        } break;
        default:; {
        } break;
    }
    #if PLATFORM == PLAT_XBOX
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
    while (pb_busy()) {}
    #endif
    swapBuffers(r);
}

static void destroyWindow(struct rendstate* r) {
    if (r->window != NULL) {
        switch (r->apigroup) {
            case RENDAPIGROUP_GL:;
                #if PLATFORM != PLAT_XBOX
                SDL_GL_DeleteContext(r->gl.ctx);
                #endif
                break;
            default:;
                break;
        }
        SDL_Window* w = r->window;
        r->window = NULL;
        SDL_DestroyWindow(w);
    }
}

static void updateWindowRes(struct rendstate* r) {
    switch (r->cfg.mode) {
        case RENDMODE_WINDOWED:;
            r->cfg.res.current = r->cfg.res.windowed;
            SDL_SetWindowFullscreen(r->window, 0);
            SDL_SetWindowSize(r->window, r->cfg.res.windowed.width, r->cfg.res.windowed.height);
            break;
        case RENDMODE_BORDERLESS:;
            r->cfg.res.current = r->cfg.res.fullscr;
            SDL_SetWindowSize(r->window, r->cfg.res.fullscr.width, r->cfg.res.fullscr.height);
            SDL_SetWindowFullscreen(r->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            break;
        case RENDMODE_FULLSCREEN:;
            r->cfg.res.current = r->cfg.res.fullscr;
            SDL_SetWindowSize(r->window, r->cfg.res.fullscr.width, r->cfg.res.fullscr.height);
            SDL_SetWindowFullscreen(r->window, SDL_WINDOW_FULLSCREEN);
            break;
    }
}

#if PLATFORM != PLAT_XBOX
static void updateWindowIcon(struct rendstate* r) {
    int w, h, c;
    void* data = stbi_load(r->cfg.icon, &w, &h, &c, STBI_rgb_alpha);
    SDL_Surface* s = SDL_CreateRGBSurfaceFrom(
        data, w, h, 32, w * 4,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
    );
    SDL_SetWindowIcon(r->window, s);
    SDL_FreeSurface(s);
    stbi_image_free(data);
}
#endif

#define SDL_GL_SetAttribute(a, v) if (SDL_GL_SetAttribute((a), (v))) plog(LL_WARN, "Failed to set " #a " to " #v ": %s", SDL_GetError())
#define SDL_SetHint(n, v) if (!SDL_SetHint((n), (v))) plog(LL_WARN, "Failed to set " #n " to %s: %s", (char*)(v), SDL_GetError())
#define SDL_SetHintWithPriority(n, v, p) if (!SDL_SetHintWithPriority((n), (v), (p))) plog(LL_WARN, "Failed to set " #n " to %s using " #p ": %s", (char*)(v), SDL_GetError())

static bool createWindow(struct rendstate* r) {
    if (r->cfg.api <= RENDAPI__INVAL || r->cfg.api >= RENDAPI__COUNT) {
        plog(LL_CRIT, "Invalid rendering API (%d)", (int)r->cfg.api);
        return false;
    }
    plog(LL_INFO, "Creating window for %s...", rendapi_names[r->cfg.api]);
    switch (r->cfg.api) {
        #if 1
        case RENDAPI_GL11:;
            r->apigroup = RENDAPIGROUP_GL;
            #if PLATFORM != PLAT_XBOX
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            #endif
            break;
        #endif
        #if PLATFORM != PLAT_XBOX
        #if 0
        case RENDAPI_GL33:;
            r->apigroup = RENDAPIGROUP_GL;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            break;
        #endif
        #if 0
        case RENDAPI_GLES30:;
            r->apigroup = RENDAPIGROUP_GL;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
            break;
        #endif
        #endif
        default:;
            plog(LL_CRIT, "%s not implemented", rendapi_names[r->cfg.api]);
            return false;
            break;
    }
    #if PLATFORM != PLAT_XBOX
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    #endif
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1", SDL_HINT_OVERRIDE);
    //SDL_SetRelativeMouseMode(SDL_TRUE);
    uint32_t flags = 0;
    #if PLATFORM != PLAT_XBOX
    flags |= SDL_WINDOW_RESIZABLE;
    if (r->apigroup == RENDAPIGROUP_GL) flags |= SDL_WINDOW_OPENGL;
    #endif
    r->window = SDL_CreateWindow(
        "PlatinumSrc",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        r->cfg.res.windowed.width, r->cfg.res.windowed.height,
        SDL_WINDOW_SHOWN | flags
    );
    if (!r->window) {
        plog(LL_CRIT, "Failed to create window: %s", SDL_GetError());
        return false;
    }
    #if PLATFORM != PLAT_XBOX
    updateWindowIcon(r);
    #endif
    SDL_DisplayMode dtmode;
    SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(r->window), &dtmode);
    r->cfg.res.fullscr = (struct rendres){dtmode.w, dtmode.h, dtmode.refresh_rate};
    updateWindowRes(r);
    switch (r->apigroup) {
        case RENDAPIGROUP_GL:; {
            #if PLATFORM != PLAT_XBOX
            r->gl.ctx = SDL_GL_CreateContext(r->window);
            if (!r->gl.ctx) {
                plog(LL_CRIT, "Failed to create OpenGL context: %s", SDL_GetError());
                r->apigroup = RENDAPIGROUP__INVAL;
                destroyWindow(r);
                return false;
            }
            if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
                plog(LL_CRIT, "Failed to load GLAD");
                destroyWindow(r);
                return false;
            }
            #endif
            plog(LL_INFO, "OpenGL info:");
            bool cond[4];
            int tmpint[4];
            char* tmpstr[1] = {""};
            cond[0] = !SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &tmpint[0]);
            cond[1] = !SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &tmpint[1]);
            cond[2] = !SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &tmpint[2]);
            if (cond[2]) {
                switch (tmpint[2]) {
                    case SDL_GL_CONTEXT_PROFILE_CORE: tmpstr[0] = " core"; break;
                    case SDL_GL_CONTEXT_PROFILE_COMPATIBILITY: tmpstr[0] = " compat"; break;
                    case SDL_GL_CONTEXT_PROFILE_ES: tmpstr[0] = " ES"; break;
                }
            }
            if (cond[0] && cond[1]) {
                plog(LL_INFO, "  Requested OpenGL version: %d.%d%s", tmpint[0], tmpint[1], tmpstr[0]);
            } else {
                plog(LL_INFO, "  Requested OpenGL version: ?");
            }
            tmpstr[0] = (char*)glGetString(GL_VERSION);
            plog(LL_INFO, "  OpenGL version: %s", (tmpstr[0]) ? tmpstr[0] : "?");
            #ifdef GL_SHADING_LANGUAGE_VERSION
            tmpstr[0] = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
            plog(LL_INFO, "  GLSL version: %s", (tmpstr[0]) ? tmpstr[0] : "?");
            #endif
            tmpstr[0] = (char*)glGetString(GL_VENDOR);
            plog(LL_INFO, "  Vendor string: %s", (tmpstr[0]) ? tmpstr[0] : "?");
            tmpstr[0] = (char*)glGetString(GL_RENDERER);
            plog(LL_INFO, "  Renderer string: %s", (tmpstr[0]) ? tmpstr[0] : "?");
            cond[0] = !SDL_GL_GetAttribute(SDL_GL_ACCELERATED_VISUAL, &tmpint[0]);
            plog(LL_INFO, "  Hardware acceleration is %s", (cond[0]) ? ((tmpint[0]) ? "enabled" : "disabled") : "?");
            cond[0] = !SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &tmpint[0]);
            plog(LL_INFO, "  Double-buffering is %s", (cond[0]) ? ((tmpint[0]) ? "enabled" : "disabled") : "?");
            cond[0] = !SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &tmpint[0]);
            cond[1] = !SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &tmpint[1]);
            cond[2] = !SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &tmpint[2]);
            cond[3] = !SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &tmpint[3]);
            if (cond[0] && cond[1] && cond[2] && cond[3]) {
                plog(LL_INFO, "  Color buffer format: R%dG%dB%dA%d", tmpint[0], tmpint[1], tmpint[2], tmpint[3]);
            } else {
                plog(LL_INFO, "  Color buffer format: ?");
            }
            cond[0] = !SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &tmpint[0]);
            if (cond[0]) {
                plog(LL_INFO, "  Depth buffer format: D%d", tmpint[0]);
            } else {
                plog(LL_INFO, "  Depth buffer format: ?");
            }
            plog(LL_INFO, "  GL_KHR_debug is%s supported", (GL_KHR_debug) ? "" : " not");
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            swapBuffers(r);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            swapBuffers(r);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
        } break;
        default:; {
        } break;
    }
    return true;
}

static bool prepRenderer(struct rendstate* r) {
    (void)r;
    //
    return true;
}

static bool startRenderer_internal(struct rendstate* r) {
    if (!createWindow(r)) return false;
    return prepRenderer(r);
}

static void stopRenderer_internal(struct rendstate* r) {
    destroyWindow(r);
}

bool reloadRenderer(struct rendstate* r) {
    plog(LL_TASK, "Reloading renderer...");
    stopRenderer_internal(r);
    return startRenderer_internal(r);
}

bool startRenderer(struct rendstate* r) {
    plog(LL_TASK, "Starting renderer...");
    return startRenderer_internal(r);
}

void stopRenderer(struct rendstate* r) {
    plog(LL_TASK, "Stopping renderer...");
    stopRenderer_internal(r);
}

bool updateRendererConfig(struct rendstate* r, struct rendupdate* u, struct rendconfig* c) {
    if (u->api) {
        enum rendapi oldapi = r->cfg.api;
        stopRenderer_internal(r);
        r->cfg.api = c->api;
        if (!startRenderer_internal(r)) {
            r->cfg.api = oldapi;
            if (!startRenderer_internal(r)) return false;
        }
    }
    if (u->mode) {
        r->cfg.mode = c->mode;
        updateWindowRes(r);
    }
    if (u->vsync) {
        r->cfg.vsync = c->vsync;
        if (r->apigroup == RENDAPIGROUP_GL) {
            #if PLATFORM != PLAT_XBOX
            SDL_GL_SetSwapInterval(c->vsync);
            #else
            pbgl_set_swap_interval(c->vsync);
            #endif
        }
    }
    if (u->res) {
        r->cfg.res.current = c->res.current;
        if (r->apigroup == RENDAPIGROUP_GL) {
            glViewport(0, 0, c->res.current.width, c->res.current.height);
        }
    }
    if (u->icon && c->icon) {
        free(r->cfg.icon);
        r->cfg.icon = strdup(c->icon);
        #if PLATFORM != PLAT_XBOX
        updateWindowIcon(r);
        #endif
    }
    return true;
}

bool initRenderer(struct rendstate* r) {
    plog(LL_TASK, "Initializing renderer...");
    memset(r, 0, sizeof(*r));
    r->cfg.api = RENDAPI_GL11;
    #if PLATFORM != PLAT_XBOX
    r->cfg.res.windowed = (struct rendres){800, 600, 60};
    #else
    r->cfg.res.windowed = (struct rendres){640, 480, 60};
    #endif
    r->cfg.vsync = false;
    if (SDL_Init(SDL_INIT_VIDEO)) {
        plog(LL_CRIT, "Failed to init video: %s", SDL_GetError());
        return false;
    }
    return true;
}

void termRenderer(struct rendstate* r) {
    plog(LL_TASK, "Terminating renderer...");
    stopRenderer_internal(r);
    free(r->cfg.icon);
}
