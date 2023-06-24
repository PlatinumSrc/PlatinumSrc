#include "renderer.h"
#include <logging.h>
#include <glad/gl.h>

#include <pthread.h>
#include <stddef.h>

const char* rendapi_ids[] = {
    "software",
    "gl_legacy",
    "gl_advanced",
    "gles_legacy",
    "gles_advanced"
};
const char* rendapi_names[] = {
    "Software renderer",
    "Legacy OpenGL (1.1)",
    "Advanced Opengl (3.3)",
    "Legacy OpenGL ES (2.0)",
    "Advanced OpenGL ES (3.0)"
};

#define SDL_GL_SetAttribute(a, v) if (SDL_GL_SetAttribute((a), (v))) plog(LOGLVL_WARN, "Failed to set " #a " to " #v ": %s", (char*)SDL_GetError())
#define SDL_SetHint(n, v) if (!SDL_SetHint((n), (v))) plog(LOGLVL_WARN, "Failed to set " #n " to %s: %s", (char*)(v), (char*)SDL_GetError())
#define SDL_SetHintWithPriority(n, v, p) if (!SDL_SetHintWithPriority((n), (v), (p))) plog(LOGLVL_WARN, "Failed to set " #n " to %s using " #p ": %s", (char*)(v), (char*)SDL_GetError())

void render_gl_legacy(struct rendstate* r) {
    if (r->evenframe) {
        glDepthRange(0.0, 0.5);
        glDepthFunc(GL_LEQUAL);
    } else {
        glDepthRange(1.0, 0.5);
        glDepthFunc(GL_GEQUAL);
    }
    glLoadIdentity();
    glBegin(GL_QUADS);
        glColor3f(1.0, 0.0, 0.0);
        glVertex3f(-0.5, 0.5, 0.0);
        glColor3f(0.5, 1.0, 0.0);
        glVertex3f(0.5, 0.5, 0.0);
        glColor3f(0.0, 1.0, 1.0);
        glVertex3f(0.5, -0.5, 0.0);
        glColor3f(0.5, 0.0, 1.0);
        glVertex3f(-0.5, -0.5, 0.0);
    glEnd();
}

void render_gl_advanced(struct rendstate* r) {
    (void)r;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void render(struct rendstate* r) {
    switch (r->apigroup) {
        case RENDAPIGROUP_GL:; {
            switch (r->api) {
                case RENDAPI_GL_LEGACY:;
                case RENDAPI_GLES_LEGACY:;
                    render_gl_legacy(r);
                    break;
                case RENDAPI_GL_ADVANCED:;
                case RENDAPI_GLES_ADVANCED:;
                    render_gl_advanced(r);
                    break;
                default:;
                    break;
            }
            SDL_GL_SwapWindow(r->window);
        } break;
        default:; {
        } break;
    }
    r->evenframe = !r->evenframe;
}

static void destroyWindow(struct rendstate* r) {
    if (r->window != NULL) {
        switch (r->apigroup) {
            case RENDAPIGROUP_GL:;
                SDL_GL_DeleteContext(r->glctx);
                break;
            default:;
                break;
        }
        SDL_DestroyWindow(r->window);
    }
}

static bool createWindow(struct rendstate* r) {
    plog(LOGLVL_INFO, "Creating window for %s...", rendapi_names[r->api]);
    if (r->api <= RENDAPI__INVAL || r->api >= RENDAPI__COUNT) {
        plog(LOGLVL_CRIT, "Invalid rendering API (%d)", (int)r->api);
        return false;
    }
    switch (r->api) {
        #if 1
        case RENDAPI_GL_LEGACY:;
            r->apigroup = RENDAPIGROUP_GL;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            break;
        #endif
        #if 0
        case RENDAPI_GL_ADVANCED:;
            r->apigroup = RENDAPIGROUP_GL;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            break;
        #endif
        #if 0
        case RENDAPI_GLES_LEGACY:;
            r->apigroup = RENDAPIGROUP_GL;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
            break;
        #endif
        #if 0
        case RENDAPI_GLES_ADVANCED:;
            r->apigroup = RENDAPIGROUP_GL;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
            break;
        #endif
        default:;
            plog(LOGLVL_CRIT, "%s not implemented", rendapi_names[r->api]);
            return false;
            break;
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1", SDL_HINT_OVERRIDE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    uint32_t flags = 0;
    if (r->apigroup == RENDAPIGROUP_GL) flags |= SDL_WINDOW_OPENGL;
    r->window = SDL_CreateWindow(
        "PlatinumSrc",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        r->res.current.width, r->res.current.height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | flags
    );
    if (r->window == NULL) {
        plog(LOGLVL_CRIT, "Failed to create window: %s", (char*)SDL_GetError());
        return false;
    }
    switch (r->apigroup) {
        case RENDAPIGROUP_GL:; {
            r->glctx = SDL_GL_CreateContext(r->window);
            if (r->glctx == NULL) {
                plog(LOGLVL_CRIT, "Failed to create OpenGL context: %s", (char*)SDL_GetError());
                r->apigroup = RENDAPIGROUP__INVAL;
                destroyWindow(r);
                return false;
            }
            if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
                plog(LOGLVL_CRIT, "Failed to load GLAD");
                destroyWindow(r);
                return false;
            }
            plog(LOGLVL_INFO, "OpenGL info:");
            int tmpint[4];
            SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &tmpint[0]);
            SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &tmpint[1]);
            SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &tmpint[2]);
            char* glprofile = "";
            switch (tmpint[2]) {
                case SDL_GL_CONTEXT_PROFILE_CORE: glprofile = " core"; break;
                case SDL_GL_CONTEXT_PROFILE_COMPATIBILITY: glprofile = " compat"; break;
                case SDL_GL_CONTEXT_PROFILE_ES: glprofile = " ES"; break;
            }
            plog(LOGLVL_INFO, "  OpenGL version: %d.%d%s", tmpint[0], tmpint[1], glprofile);
            SDL_GL_GetAttribute(SDL_GL_ACCELERATED_VISUAL, &tmpint[0]);
            plog(LOGLVL_INFO, "  Hardware acceleration is %s", (tmpint[0]) ? "enabled" : "disabled");
            SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &tmpint[0]);
            plog(LOGLVL_INFO, "  Double-buffering is %s", (tmpint[0]) ? "enabled" : "disabled");
            SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &tmpint[0]);
            SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &tmpint[1]);
            SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &tmpint[2]);
            SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &tmpint[3]);
            plog(LOGLVL_INFO, "  Color buffer bit depth: R%dG%dB%dA%d", tmpint[0], tmpint[1], tmpint[2], tmpint[3]);
            SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &tmpint[0]);
            plog(LOGLVL_INFO, "  Depth buffer bit depth: D%d", tmpint[0]);
            plog(LOGLVL_INFO, "  GL_KHR_debug is%s supported", (GL_KHR_debug) ? "" : " not");
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
    plog(LOGLVL_TASK, "Reloading renderer...");
    stopRenderer_internal(r);
    return startRenderer_internal(r);
}

bool startRenderer(struct rendstate* r) {
    plog(LOGLVL_TASK, "Starting renderer...");
    return startRenderer_internal(r);
}

void stopRenderer(struct rendstate* r) {
    plog(LOGLVL_TASK, "Stopping renderer...");
    stopRenderer_internal(r);
}

bool updateRendererConfig(struct rendstate* r, bool reload) {
    if (reload) stopRenderer_internal(r);
    //
    if (reload) if (!startRenderer_internal(r)) return false;
    if (r->apigroup == RENDAPIGROUP_GL) {
        SDL_GL_SetSwapInterval(r->vsync);
    }
    return true;
}

bool initRenderer(struct rendstate* r) {
    plog(LOGLVL_TASK, "Initializing renderer...");
    r->window = NULL;
    r->api = RENDAPI_GL_LEGACY;
    r->res.current = (struct res){800, 600, 60};
    r->vsync = false;
    r->evenframe = true;
    if (SDL_Init(SDL_INIT_VIDEO)) {
        plog(LOGLVL_CRIT, "Failed to init video: %s", (char*)SDL_GetError());
        return false;
    }
    return true;
}

void termRenderer(struct rendstate* r) {
    plog(LOGLVL_TASK, "Terminating renderer...");
    stopRenderer_internal(r);
}
