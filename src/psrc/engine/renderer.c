#include "renderer.h"

#include "../version.h"
#include "../debug.h"

#include "../utils/logging.h"
#include "../utils/string.h"
//#include "../utils/threads.h"

#include "../common/common.h"
#include "../common/p3m.h"

#include "../../stb/stb_image.h"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#if PLATFORM == PLAT_NXDK
    #include <pbkit/pbkit.h>
    #include <pbkit/nv_regs.h>
    #include <pbgl.h>
#endif
#ifndef GL_KHR_debug
    #define GL_KHR_debug 0
#endif
#if GL_KHR_debug
#pragma weak glDebugMessageCallback
static void APIENTRY gl_dbgcb(GLenum src, GLenum type, GLuint id, GLenum sev, GLsizei l, const GLchar *m, const void *u) {
    (void)l; (void)u;
    int ll = -1;
    char* sevstr;
    switch (sev) {
        case GL_DEBUG_SEVERITY_HIGH: ll = LL_ERROR; sevstr = "error"; break;
        case GL_DEBUG_SEVERITY_MEDIUM: ll = LL_WARN; sevstr = "warning"; break;
        #if DEBUG(1)
        case GL_DEBUG_SEVERITY_LOW: ll = LL_WARN; sevstr = "warning"; break;
        #if DEBUG(2)
        case GL_DEBUG_SEVERITY_NOTIFICATION: ll = LL_INFO; sevstr = "info"; break;
        #endif
        #endif
        default: break;
    }
    if (ll == -1) return;
    char* srcstr;
    switch (src) {
        case GL_DEBUG_SOURCE_API: srcstr = "API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: srcstr = "Windowing system"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: srcstr = "Shader compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY: srcstr = "Third-party"; break;
        case GL_DEBUG_SOURCE_APPLICATION: srcstr = "Application"; break;
        case GL_DEBUG_SOURCE_OTHER: srcstr = "Other"; break;
        default: srcstr = "Unknown"; break;
    }
    char* typestr;
    switch (type) {
        case GL_DEBUG_TYPE_ERROR: typestr = "error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typestr = "deprecation"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: typestr = "undefined behavior"; break;
        case GL_DEBUG_TYPE_PORTABILITY: typestr = "portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE: typestr = "performance"; break;
        case GL_DEBUG_TYPE_MARKER: typestr = "marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP: typestr = "push group"; break;
        case GL_DEBUG_TYPE_POP_GROUP: typestr = "pop group"; break;
        case GL_DEBUG_TYPE_OTHER: typestr = "other"; break;
        default: typestr = "unknown"; break;
    }
    plog(ll, "OpenGL %s 0x%08X (%s %s): %s", sevstr, id, srcstr, typestr, m);
}
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

void swapBuffers(void) {
    #if PLATFORM != PLAT_NXDK
    SDL_GL_SwapWindow(rendstate.window);
    #else
    pbgl_swap_buffers();
    #endif
}

static void gl_calcProjMat(void) {
    glm_perspective(
        rendstate.fov * M_PI / 180.0, -rendstate.aspect,
        #if PLATFORM != PLAT_NXDK
        rendstate.gl.nearplane, rendstate.gl.farplane,
        #else
        rendstate.gl.nearplane * rendstate.gl.scale * rendstate.gl.scale,
        rendstate.gl.farplane * rendstate.gl.scale * rendstate.gl.scale,
        #endif
        rendstate.gl.projmat
    );
}

static void gl_calcViewMat(void) {
    static float campos[3];
    static float front[3];
    static float up[3];
    #if PLATFORM != PLAT_NXDK
    campos[0] = rendstate.campos[0];
    campos[1] = rendstate.campos[1];
    campos[2] = rendstate.campos[2];
    #else
    campos[0] = rendstate.campos[0] * rendstate.gl.scale;
    campos[1] = rendstate.campos[1] * rendstate.gl.scale;
    campos[2] = rendstate.campos[2] * rendstate.gl.scale;
    #endif
    float rotradx = rendstate.camrot[0] * M_PI / 180.0;
    float rotrady = rendstate.camrot[1] * -M_PI / 180.0;
    float rotradz = rendstate.camrot[2] * M_PI / 180.0;
    front[0] = (-sin(rotrady)) * cos(rotradx);
    front[1] = sin(rotradx);
    front[2] = cos(rotrady) * cos(rotradx);
    up[0] = sin(rotrady) * sin(rotradx) * cos(rotradz) + sin(rotradz) * cos(rotrady);
    up[1] = cos(rotradx) * cos(rotradz);
    up[2] = (-cos(rotrady)) * sin(rotradx) * cos(rotradz) + sin(rotradz) * sin(rotrady);
    glm_vec3_add(campos, front, front);
    glm_lookat(campos, front, up, rendstate.gl.viewmat);
}

static void gl_updateWindow(void) {
    glViewport(0, 0, rendstate.res.current.width, rendstate.res.current.height);
    if (rendstate.apigroup == RENDAPIGROUP_GL) gl_calcProjMat();
}

static inline __attribute__((always_inline)) void gl_clearScreen(void) {
    if (rendstate.gl.fastclear) {
        glClear(GL_DEPTH_BUFFER_BIT);
    } else {
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }
}

struct rc_model* testmodel;

void rendermodel_gl_legacy(struct p3m* m, struct p3m_vertex* verts) {
    if (!verts) verts = m->vertices;
    for (int ig = 0; ig < m->indexgroupcount; ++ig) {
        uint16_t indcount = m->indexgroups[ig].indexcount;
        uint16_t* inds = m->indexgroups[ig].indices;
        glBegin(GL_TRIANGLES);
        glColor3f(1.0, 1.0, 1.0);
        long lt = SDL_GetTicks();
        double dt = (double)(lt % 1000) / 1000.0;
        double t = (double)(lt / 1000) + dt;
        float tsin = sin(t * 0.179254 * M_PI) * 2.0;
        float tsin2 = fabs(sin(t * 0.374124 * M_PI));
        float tcos = cos(t * 0.214682 * M_PI) * 0.5;
        for (uint16_t i = 0; i < indcount; ++i) {
            float tmp1 = verts[*inds].z * 7.5;
            #if 1
            int ci = (*inds * 0x10492851) ^ *inds;
            uint8_t c[3] = {ci >> 16, ci >> 8, ci};
            glColor3f(c[0] / 255.0, c[1] / 255.0, c[2] / 255.0);
            #else
            //glColor3f(tmp1, tmp1, tmp1);
            int tmpi = i - (i % 3);
            int ci = (tmpi * 0x10632151);
            ci ^= (tmpi >> 16) * 0x234120B4;
            ci -= 0x12E23827;
            uint8_t c[3] = {ci >> 16, ci >> 8, ci};
            #endif
            glColor3f(c[0] / 255.0 * tmp1, c[1] / 255.0 * tmp1, c[2] / 255.0 * tmp1);
            glVertex3f(-verts[*inds].x + tsin, verts[*inds].y - 1.8 + tsin2, -verts[*inds].z + 1.75 + tcos);
            ++inds;
        }
        glEnd();
    }
}
#if 0
void render_gl_legacy(void) {
    gl_clearScreen();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((float*)rendstate.gl.projmat);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf((float*)rendstate.gl.viewmat);

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

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

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

    glDepthMask(GL_TRUE);
    gl_clearScreen();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((float*)rendstate.gl.projmat);
    #if PLATFORM == PLAT_NXDK
    // workaround for some depth buffer issues
    glScalef(rendstate.gl.scale, rendstate.gl.scale, rendstate.gl.scale);
    #endif
    glMatrixMode(GL_MODELVIEW);
    gl_calcViewMat();
    glLoadMatrixf((float*)rendstate.gl.viewmat);
    #if PLATFORM == PLAT_NXDK
    glScalef(rendstate.gl.scale, rendstate.gl.scale, rendstate.gl.scale);
    #endif

    float z = 2.0;

    glBegin(GL_QUADS);
        #if 1
        glColor3f(tsini, tcosn, tsinn);
        glVertex3f(-1.0, 1.0, z);
        glColor3f(tcosi, tsini, tcosn);
        glVertex3f(-1.0, -1.0, z);
        glColor3f(tsinn, tcosi, tsini);
        glVertex3f(1.0, -1.0, z);
        glColor3f(tcosn, tsinn, tcosi);
        glVertex3f(1.0, 1.0, z);
        #endif
        glColor3f(1.0, 0.0, 0.0);
        glVertex3f(-0.5, 0.5, z);
        glColor3f(0.5, 1.0, 0.0);
        glVertex3f(-0.5, -0.5, z);
        glColor3f(0.0, 1.0, 1.0);
        glVertex3f(0.5, -0.5, z);
        glColor3f(0.5, 0.0, 1.0);
        glVertex3f(0.5, 0.5, z);
        glColor3f(0.0, 0.0, 0.0);
        glVertex3f(-1.0, 0.025 + tsin2, z);
        glVertex3f(-1.0, -0.025 + tsin2, z);
        glVertex3f(1.0, -0.025 + tsin2, z);
        glVertex3f(1.0, 0.025 + tsin2, z);
        glColor3f(1.0, 1.0, 1.0);
        glVertex3f(-0.025 + tsin, 1.0, z);
        glVertex3f(-0.025 + tsin, -1.0, z);
        glVertex3f(0.025 + tsin, -1.0, z);
        glVertex3f(0.025 + tsin, 1.0, z);
        glColor3f(0.0, 0.5, 0.0);
        glVertex3f(-0.5, -1.0, z);
        glVertex3f(-0.5, -1.0, z - 1.0);
        glVertex3f(0.5, -1.0, z - 1.0);
        glVertex3f(0.5, -1.0, z);
    glEnd();

    if (testmodel) rendermodel_gl_legacy(testmodel->model, NULL);

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS);
        glColor3f(1.0, 1.0, 1.0);
        glVertex3f(-1.0, 1.0, z);
        glColor3f(0.5, 0.5, 0.5);
        glVertex3f(-1.0, -1.0, z);
        glColor3f(0.0, 0.0, 0.0);
        glVertex3f(1.0, -1.0, z);
        glColor3f(0.5, 0.5, 0.5);
        glVertex3f(1.0, 1.0, z);
    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
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
            glFinish();
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
    } else {
        GLboolean tmp;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &tmp);
        glDepthMask(false);
        pb_draw_text_screen();
        uint32_t* p = pb_begin();
        pb_push(p++, NV097_SET_CLEAR_RECT_HORIZONTAL, 2);
        *(p++) = (rendstate.res.current.width - 1) << 16;
        *(p++) = (rendstate.res.current.height - 1) << 16;
        pb_end(p);
        glDepthMask(tmp);
    }
    #endif
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

static void updateWindowMode(void) {
    switch (rendstate.mode) {
        case RENDMODE_WINDOWED: {
            rendstate.res.current = rendstate.res.windowed;
            SDL_SetWindowFullscreen(rendstate.window, 0);
            SDL_SetWindowSize(rendstate.window, rendstate.res.windowed.width, rendstate.res.windowed.height);
        } break;
        case RENDMODE_BORDERLESS: {
            rendstate.res.current = rendstate.res.fullscr;
            SDL_DisplayMode mode;
            SDL_GetCurrentDisplayMode(0, &mode);
            mode.w = rendstate.res.fullscr.width;
            mode.h = rendstate.res.fullscr.height;
            SDL_SetWindowDisplayMode(rendstate.window, &mode);
            SDL_SetWindowFullscreen(rendstate.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        } break;
        case RENDMODE_FULLSCREEN: {
            rendstate.res.current = rendstate.res.fullscr;
            SDL_DisplayMode mode;
            SDL_GetCurrentDisplayMode(0, &mode);
            mode.w = rendstate.res.fullscr.width;
            mode.h = rendstate.res.fullscr.height;
            SDL_SetWindowDisplayMode(rendstate.window, &mode);
            SDL_SetWindowFullscreen(rendstate.window, SDL_WINDOW_FULLSCREEN);
        } break;
    }
    rendstate.aspect = (float)rendstate.res.current.width / (float)rendstate.res.current.height;
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
        case RENDAPI_GL11: {
            rendstate.apigroup = RENDAPIGROUP_GL;
            #if PLATFORM != PLAT_NXDK
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            #endif
        } break;
        #endif
        #if PLATFORM != PLAT_NXDK // TODO: make some HAS_ macros
        #if 0
        case RENDAPI_GL33: {
            rendstate.apigroup = RENDAPIGROUP_GL;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        } break;
        #endif
        #if 0
        case RENDAPI_GLES30: {
            rendstate.apigroup = RENDAPIGROUP_GL;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        } break;
        #endif
        #endif
        default: {
            plog(LL_CRIT, "%s not implemented", rendapi_names[rendstate.api]);
            return false;
        } break;
    }
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    //SDL_SetRelativeMouseMode(SDL_TRUE);
    uint32_t flags = 0;
    #if PLATFORM != PLAT_NXDK
    flags |= SDL_WINDOW_RESIZABLE;
    if (rendstate.apigroup == RENDAPIGROUP_GL) flags |= SDL_WINDOW_OPENGL;
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
    rendstate.aspect = (float)rendstate.res.current.width / (float)rendstate.res.current.height;
    rendstate.window = SDL_CreateWindow(
        titlestr,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        rendstate.res.current.width, rendstate.res.current.height,
        SDL_WINDOW_SHOWN | flags
    );
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG, "Windowed resolution: %dx%d", rendstate.res.windowed.width, rendstate.res.windowed.height);
    plog(LL_INFO | LF_DEBUG, "Fullscreen resolution: %dx%d", rendstate.res.fullscr.width, rendstate.res.fullscr.height);
    #endif
    if (!rendstate.window) {
        plog(LL_CRIT | LF_FUNCLN, "Failed to create window: %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(rendstate.window, 320, 240);
    #if PLATFORM != PLAT_NXDK
    updateWindowIcon();
    #endif
    switch (rendstate.apigroup) {
        case RENDAPIGROUP_GL: {
            #if PLATFORM != PLAT_NXDK
            {
                char* tmp;
                tmp = cfg_getvar(config, "Renderer", "gl.doublebuffer");
                if (tmp) {
                    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, strbool(tmp, 1));
                    free(tmp);
                } else {
                    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
                }
                {
                    unsigned flags;
                    tmp = cfg_getvar(config, "Renderer", "gl.forwardcompat");
                    if (strbool(tmp, 0)) flags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
                    else flags = 0;
                    free(tmp);
                    tmp = cfg_getvar(config, "Renderer", "gl.debug");
                    if (strbool(tmp, 1)) flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
                    free(tmp);
                    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, flags);
                }
            }
            rendstate.gl.ctx = SDL_GL_CreateContext(rendstate.window);
            if (!rendstate.gl.ctx) {
                plog(LL_CRIT | LF_FUNCLN, "Failed to create OpenGL context: %s", SDL_GetError());
                rendstate.apigroup = RENDAPIGROUP__INVAL;
                destroyWindow();
                return false;
            }
            SDL_GL_MakeCurrent(rendstate.window, rendstate.gl.ctx);
            if (rendstate.vsync) {
                if (SDL_GL_SetSwapInterval(-1) == -1) SDL_GL_SetSwapInterval(1);
            } else {
                SDL_GL_SetSwapInterval(0);
            }
            #else
            //pbgl_set_swap_interval(rendstate.vsync);
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
            #if PLATFORM != PLAT_NXDK && GL_KHR_debug
            if (glDebugMessageCallback) plog(LL_INFO, "  glDebugMessageCallback is supported");
            #endif
            tmpstr[0] = cfg_getvar(config, "Renderer", "gl.nearplane");
            if (tmpstr[0]) {
                rendstate.gl.nearplane = atof(tmpstr[0]);
                free(tmpstr[0]);
            } else {
                rendstate.gl.nearplane = 0.1;
            }
            tmpstr[0] = cfg_getvar(config, "Renderer", "gl.farplane");
            if (tmpstr[0]) {
                rendstate.gl.farplane = atof(tmpstr[0]);
                free(tmpstr[0]);
            } else {
                rendstate.gl.farplane = 1000.0;
            }
            #if PLATFORM == PLAT_NXDK
            tmpstr[0] = cfg_getvar(config, "Renderer", "nxdk.gl.scale");
            if (tmpstr[0]) {
                rendstate.gl.scale = atof(tmpstr[0]);
                free(tmpstr[0]);
            } else {
                rendstate.gl.scale = 25.0;
            }
            #endif
            tmpstr[0] = cfg_getvar(config, "Renderer", "gl.fastclear");
            #if DEBUG(1)
            // makes debugging easier
            rendstate.gl.fastclear = strbool(tmpstr[0], false);
            #else
            rendstate.gl.fastclear = strbool(tmpstr[0], true);
            #endif
            free(tmpstr[0]);
            #if PLATFORM != PLAT_NXDK && GL_KHR_debug
            if (glDebugMessageCallback) glDebugMessageCallback(gl_dbgcb, NULL);
            #endif
            glClearColor(0.0, 0.0, 0.1, 1.0);
            gl_updateWindow();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
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
            case RENDOPT_FULLSCREEN: {
                int tmp = va_arg(args, int);
                if (tmp < 0) {
                    rendstate.mode = (rendstate.mode == RENDMODE_WINDOWED) ?
                        ((rendstate.borderless) ? RENDMODE_BORDERLESS : RENDMODE_FULLSCREEN) :
                        RENDMODE_WINDOWED;
                } else if (tmp) {
                    rendstate.mode = (rendstate.borderless) ? RENDMODE_BORDERLESS : RENDMODE_FULLSCREEN;
                } else {
                    rendstate.mode = RENDMODE_WINDOWED;
                }
                updateWindowMode();
                if (rendstate.apigroup == RENDAPIGROUP_GL) gl_updateWindow();
            } break;
            case RENDOPT_BORDERLESS: {
                rendstate.borderless = va_arg(args, int);
                rendstate.mode = (rendstate.mode == RENDMODE_BORDERLESS || rendstate.mode == RENDMODE_FULLSCREEN) ?
                    ((rendstate.borderless) ? RENDMODE_BORDERLESS : RENDMODE_FULLSCREEN) :
                    RENDMODE_WINDOWED;
                updateWindowMode();
                if (rendstate.apigroup == RENDAPIGROUP_GL) gl_updateWindow();
            } break;
            case RENDOPT_VSYNC: {
                rendstate.vsync = va_arg(args, int);
                if (rendstate.apigroup == RENDAPIGROUP_GL) {
                    #if PLATFORM != PLAT_NXDK
                    if (rendstate.vsync) {
                        if (SDL_GL_SetSwapInterval(-1) == -1) SDL_GL_SetSwapInterval(1);
                    } else {
                        SDL_GL_SetSwapInterval(0);
                    }
                    #else
                    //pbgl_set_swap_interval(rendstate.vsync);
                    #endif
                }
            } break;
            case RENDOPT_FOV: {
                rendstate.fov = va_arg(args, double);
                if (rendstate.apigroup == RENDAPIGROUP_GL) gl_calcProjMat();
            } break;
            case RENDOPT_RES: {
                struct rendres* res = va_arg(args, struct rendres*);
                if (res->width >= 0) rendstate.res.current.width = res->width;
                if (res->height >= 0) rendstate.res.current.height = res->height;
                switch (rendstate.mode) {
                    case RENDMODE_WINDOWED:
                        if (rendstate.res.current.width == rendstate.res.windowed.width &&
                            rendstate.res.current.height == rendstate.res.windowed.height) goto rettrue;
                        rendstate.res.windowed = rendstate.res.current;
                        break;
                    case RENDMODE_BORDERLESS:
                    case RENDMODE_FULLSCREEN:
                        if (rendstate.res.current.width == rendstate.res.fullscr.width &&
                            rendstate.res.current.height == rendstate.res.fullscr.height) goto rettrue;
                        rendstate.res.fullscr = rendstate.res.current;
                        break;
                }
                SDL_SetWindowSize(rendstate.window, rendstate.res.current.width, rendstate.res.current.height);
                rendstate.aspect = (float)rendstate.res.current.width / (float)rendstate.res.current.height;
                if (rendstate.apigroup == RENDAPIGROUP_GL) gl_updateWindow();
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
    rendstate.api = RENDAPI_GL11;
    char* tmp = cfg_getvar(config, "Renderer", "resolution.windowed");
    #if PLATFORM != PLAT_NXDK
    rendstate.res.windowed = (struct rendres){800, 600};
    #else
    rendstate.res.windowed = (struct rendres){640, 480};
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
    rendstate.fov = (tmp) ? atof(tmp) : 90.0;
    testmodel = loadResource(RC_MODEL, "game:test/test_model", NULL);
    return true;
}

void termRenderer(void) {
    freeResource(testmodel);
    free(rendstate.icon);
}
