#include "renderer.h"
#include "renderer_gl.h"
#include "client.h"

#include "../logging.h"
#include "../common.h"
#include "../time.h"

#include <stdlib.h>

#if PLATFORM == PLAT_EMSCR
    #include <GL/gl.h>
    #ifdef GL_KHR_debug
        #undef GL_KHR_debug
    #endif
    #define GL_KHR_debug 0
#else
    #ifndef PSRC_ENGINE_RENDERER_GL_USEGLAD
        #define GL_GLEXT_PROTOTYPES
        #if PLATFORM != PLAT_MACOS
            #include <GL/gl.h>
            #include <GL/glext.h>
        #else
            #include <OpenGL/gl.h>
            #include <OpenGL/glext.h>
        #endif
        #if defined(GLAPIENTRY)
            #define GLDBGCB GLAPIENTRY
        #elif defined(APIENTRY)
            #define GLDBGCB APIENTRY
        #else
            #define GLDBGCB
        #endif
    #else
        #include "../../glad/gl.h"
        #define GLDBGCB GLAD_API_PTR
    #endif
    #ifndef GL_KHR_debug
        #define GL_KHR_debug 0
    #endif
#endif

#if GL_KHR_debug
    #ifndef PSRC_ENGINE_RENDERER_GL_USEGLAD
        #pragma weak glDebugMessageCallback
    #endif
    static void GLDBGCB r_gl_dbgcb(GLenum src, GLenum type, GLuint id, GLenum sev, GLsizei l, const GLchar *m, const void *u) {
        (void)l; (void)u;
        int ll;
        char* sevstr;
        switch (sev) {
            case GL_DEBUG_SEVERITY_HIGH: ll = LL_ERROR; sevstr = "error"; break;
            case GL_DEBUG_SEVERITY_MEDIUM: ll = LL_WARN; sevstr = "warning"; break;
            #if DEBUG(1)
            case GL_DEBUG_SEVERITY_LOW: ll = LL_WARN; sevstr = "warning"; break;
            #if DEBUG(2)
            case GL_DEBUG_SEVERITY_NOTIFICATION: ll = LL_INFO; sevstr = "debug message"; break;
            #endif
            #endif
            default: return;
        }
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

static struct rc_model* testmodel;
static struct rc_texture* testskybox[6];
GLuint testskyboxids[6];
static struct rc_texture* testclouds[2];
GLuint testcloudids[2];

struct r_gl_playerdata {
    bool valid;
    float nearplane;
    float farplane;
    float fov;
    float aspect;
    float stereoinsetmul;
    float projmat[4][4];
    float viewmat[4][4];
};
static struct {
    #ifndef PSRC_USESDL1
    SDL_GLContext ctx;
    #endif
    uint8_t fastclear : 1;
    uint8_t updateframe : 1;
    union {
        #ifdef PSRC_ENGINE_RENDERER_GL_USEGL11
        struct {
            uint_fast8_t oddframe : 1;
            uint_fast8_t has_ARB_multitexture : 1;
            uint_fast8_t has_ARB_texture_border_clamp : 1;
            uint_fast8_t has_SGIS_texture_edge_clamp : 1;
            int maxlights;
            int maxtexunits;
        } gl11;
        #endif
        #ifdef PSRC_ENGINE_RENDERER_GL_USEGL33
        struct {
            char placeholder;
        } gl33;
        #endif
        #ifdef PSRC_ENGINE_RENDERER_GL_USEGLES30
        struct {
            char placeholder;
        } gles30;
        #endif
    };
    struct {
        struct r_gl_playerdata* data;
        unsigned len;
        unsigned size;
    } playerdata;
} r_gl_data;

void (*r_gl_render)(void);

void r_gl_display(void) {
    glFinish();
    #ifndef PSRC_USESDL1
    SDL_GL_SwapWindow(rendstate.window);
    #else
    SDL_GL_SwapBuffers();
    #endif
}

static void r_gl_calcProjMat(struct player* pldata, struct r_gl_playerdata* rpldata) {
    float tmp1 = 1.0f / tanf(pldata->camera.fov * (float)M_PI / 180.0f * 0.5f);
    float tmp2 = 1.0f / (rpldata->nearplane - rpldata->farplane);
    rpldata->projmat[0][0] = -(tmp1 / rpldata->aspect);
    rpldata->projmat[1][1] = tmp1;
    rpldata->projmat[2][2] = (rpldata->nearplane + rpldata->farplane) * tmp2;
    rpldata->projmat[3][2] = 2.0f * rpldata->nearplane * rpldata->farplane * tmp2;
}

static void r_gl_calcViewMat(struct player* pldata, struct r_gl_playerdata* rpldata) {
    float up[3];
    float front[3];
    {
        float sinx = pldata->common.cameracalc.isin[0];
        float siny = pldata->common.cameracalc.isin[1];
        float sinz = pldata->common.cameracalc.isin[2];
        float cosx = pldata->common.cameracalc.icos[0];
        float cosy = pldata->common.cameracalc.icos[1];
        float cosz = pldata->common.cameracalc.icos[2];
        up[0] = -sinx * siny * cosz - cosy * sinz;
        up[1] = cosx * cosz;
        up[2] = sinx * cosy * cosz - siny * sinz;
        front[0] = cosx * -siny;
        front[1] = -sinx;
        front[2] = cosx * cosy;
    }
    float pos[3];
    pos[0] = pldata->common.cameracalc.pos.pos[0];
    pos[1] = pldata->common.cameracalc.pos.pos[1];
    pos[2] = pldata->common.cameracalc.pos.pos[2];
    rpldata->viewmat[0][0] = front[1] * up[2] - front[2] * up[1];
    rpldata->viewmat[1][0] = front[2] * up[0] - front[0] * up[2];
    rpldata->viewmat[2][0] = front[0] * up[1] - front[1] * up[0];
    rpldata->viewmat[3][0] = -(rpldata->viewmat[0][0] * pos[0] + rpldata->viewmat[1][0] * pos[1] + rpldata->viewmat[2][0] * pos[2]);
    rpldata->viewmat[0][1] = up[0];
    rpldata->viewmat[1][1] = up[1];
    rpldata->viewmat[2][1] = up[2];
    rpldata->viewmat[3][1] = -(up[0] * pos[0] + up[1] * pos[1] + up[2] * pos[2]);
    rpldata->viewmat[0][2] = -front[0];
    rpldata->viewmat[1][2] = -front[1];
    rpldata->viewmat[2][2] = -front[2];
    rpldata->viewmat[3][2] = front[0] * pos[0] + front[1] * pos[1] + front[2] * pos[2];
}

static inline void r_gl_initPlayerData(struct r_gl_playerdata* pldata) {
    pldata->valid = true;
    pldata->projmat[0][1] = 0.0f;
    pldata->projmat[0][2] = 0.0f;
    pldata->projmat[0][3] = 0.0f;
    pldata->projmat[1][0] = 0.0f;
    pldata->projmat[1][2] = 0.0f;
    pldata->projmat[1][3] = 0.0f;
    pldata->projmat[2][0] = 0.0f;
    pldata->projmat[2][1] = 0.0f;
    pldata->projmat[2][3] = -1.0f;
    pldata->projmat[3][0] = 0.0f;
    pldata->projmat[3][1] = 0.0f;
    pldata->projmat[3][3] = 0.0f;
    pldata->viewmat[0][3] = 0.0f;
    pldata->viewmat[1][3] = 0.0f;
    pldata->viewmat[2][3] = 0.0f;
    pldata->viewmat[3][3] = 1.0f;
    char* tmp = cfg_getvar(&config, "Renderer", "gl.near");
    if (tmp) {
        pldata->nearplane = atof(tmp);
        free(tmp);
    } else {
        pldata->nearplane = 0.1f;
    }
    tmp = cfg_getvar(&config, "Renderer", "gl.far");
    if (tmp) {
        pldata->farplane = atof(tmp);
        free(tmp);
    } else {
        pldata->farplane = 100.0f;
    }
}
static inline void r_gl_freePlayerData(struct r_gl_playerdata* pldata) {
    pldata->valid = false;
}
static inline void r_gl_syncPlayerData(struct player* pl, struct r_gl_playerdata* out) {
    r_gl_calcViewMat(pl, out);
    if (r_gl_data.updateframe || pl->screen.changed.dim) {
        if (rendstate.stereo.mode != RENDSTEREO_SIDEBYSIDE) {
            out->aspect = (float)rendstate.res.current.width / (float)rendstate.res.current.height;
        } else {
            out->aspect = (float)rendstate.res.current.width * 0.5f / (float)rendstate.res.current.height;
            out->stereoinsetmul = pl->camera.fov * (1.0f / (200.0f - pl->camera.fov));
        }
        out->fov = pl->camera.fov;
        r_gl_calcProjMat(pl, out);
    }
}

static void r_gl_updatePlayerData(void) {
    register unsigned syncmax;
    if (playerdata.len > r_gl_data.playerdata.len) {
        syncmax = r_gl_data.playerdata.len;
        VLB_EXPANDTO(r_gl_data.playerdata, playerdata.len, 3, 2, VLB_OOM_NOP);
        for (register unsigned i = syncmax; i < r_gl_data.playerdata.len; ++i) {
            r_gl_initPlayerData(&r_gl_data.playerdata.data[i]);
            r_gl_syncPlayerData(&playerdata.data[i], &r_gl_data.playerdata.data[i]);
        }
    } else if (playerdata.len < r_gl_data.playerdata.len) {
        syncmax = playerdata.len;
        for (register unsigned i = playerdata.len; i < r_gl_data.playerdata.len; ++i) {
            r_gl_freePlayerData(&r_gl_data.playerdata.data[i]);
        }
        r_gl_data.playerdata.len = playerdata.len;
        VLB_SHRINK(r_gl_data.playerdata, VLB_OOM_NOP);
    } else {
        syncmax = r_gl_data.playerdata.len;
    }
    for (register unsigned i = 0; i < syncmax; ++i) {
        if (playerdata.data[i].priv.username) {
            if (!r_gl_data.playerdata.data[i].valid) r_gl_initPlayerData(&r_gl_data.playerdata.data[i]);
            r_gl_syncPlayerData(&playerdata.data[i], &r_gl_data.playerdata.data[i]);
        } else {
            if (r_gl_data.playerdata.data[i].valid) r_gl_freePlayerData(&r_gl_data.playerdata.data[i]);
        }
    }
}

void r_gl_updateFrame(void) {
    r_gl_data.updateframe = 1;
}

void r_gl_updateVSync(void) {
    #ifndef PSRC_USESDL1
    if (rendstate.vsync) {
        if (SDL_GL_SetSwapInterval(-1) == -1) SDL_GL_SetSwapInterval(1);
    } else {
        SDL_GL_SetSwapInterval(0);
    }
    #else
    SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, rendstate.vsync);
    #endif
}

#ifdef PSRC_ENGINE_RENDERER_GL_USEGL11
static void r_gl_rendermodel_legacy(struct p3m* m, struct p3m_vertex** transverts) {
    long lt = SDL_GetTicks();
    #if 0
    int vertct = 0;
    for (int mc = 0; mc < 5; ++mc) {
    #endif
        double dt = (double)(lt % 1000) / 1000.0;
        double t = (double)(lt / 1000) + dt;
        float tsin = (float)sin(t * 0.179254 * M_PI) * 2.0f;
        float tsin2 = (float)fabs(sin(t * 0.374124 * M_PI));
        float tcos = (float)cos(t * 0.214682 * M_PI) * 0.5f;
        for (int p = 0; p < m->partcount; ++p) {
            uint16_t indcount = m->parts[p].indexcount;
            uint16_t* inds = m->parts[p].indices;
            struct p3m_vertex* verts = (transverts) ? transverts[p] : m->parts[p].vertices;
            glBegin(GL_TRIANGLES);
            //glColor3f(1.0f, 1.0f, 1.0f);
            for (uint16_t i = 0; i < indcount; ++i) {
                float tmp1 = (verts[*inds].z - tcos) * 3.25f + 1.5f;
                if (tmp1 < 0.0f) tmp1 = 0.0f;
                else if (tmp1 > 1.0f) tmp1 = 1.0f;
                #if 1
                int ci = (*inds * 0x10492851) ^ *inds;
                uint8_t c[3] = {ci >> 16, ci >> 8, ci};
                //glColor3f(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f);
                #else
                //glColor3f(tmp1, tmp1, tmp1);
                int tmpi = i - (i % 3);
                ci ^= 0x2875BC92;
                ci >>= 3;
                ci -= 0xAE2848BC;
                uint8_t c[3] = {ci >> 16, ci >> 8, ci};
                #endif
                glColor3f(c[0] / 255.0f * tmp1, c[1] / 255.0f * tmp1, c[2] / 255.0f * tmp1);
                glVertex3f(-verts[*inds].x + tsin, verts[*inds].y - 1.8f + tsin2, -verts[*inds].z + 1.75f + tcos);
                //glVertex3f(-verts[*inds].x, verts[*inds].y - 1.8f, -verts[*inds].z + 1.75f);
                //++vertct;
                ++inds;
            }
            glEnd();
        }
    #if 0
        lt += 100;
    }
    printf("verts: %d, tris: %d\n", vertct, vertct / 3);
    #endif
}
#if 0
static void r_gl_render_legacy(void) {
    r_gl_updatePlayerData();

    r_gl_clearScreen();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((float*)r_gl_data.projmat);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf((float*)r_gl_data.viewmat);

    if (rendstate.lighting >= 1) {
        // TODO: render opaque materials front to back
    } else {
        // TODO: render opaque materials front to back with basic lighting
    }

    glEnable(GL_CULL_FACE);

    // TODO: render entities

    glDisable(GL_CULL_FACE);
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

    glFinish();
}
#else
static void r_gl_render_legacy(void) {
    r_gl_updatePlayerData();

    long lt = SDL_GetTicks();
    double dt = (double)(lt % 1000) / 1000.0;
    double t = (double)(lt / 1000) + dt;
    float tsin = (float)sin(t * 0.827535 * M_PI);
    float tsin2 = (float)sin(t * 0.628591 * M_PI);
    float tsinn = (float)sin(t * M_PI) * 0.5f + 0.5f;
    float tcosn = (float)cos(t * M_PI) * 0.5f + 0.5f;
    float tsini = 1.0f - tsinn;
    float tcosi = 1.0f - tcosn;

    r_gl_data.gl11.oddframe = !r_gl_data.gl11.oddframe;

    if (!r_gl_data.fastclear) {
        glViewport(0, 0, rendstate.res.current.width, rendstate.res.current.height);
        glDepthMask(GL_TRUE);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }

    for (unsigned pli = 0; pli < r_gl_data.playerdata.len; ++pli) {

        struct r_gl_playerdata* rpldata = &r_gl_data.playerdata.data[pli];

        struct {
            uint_fast8_t eye;
            float eyeoff;
            unsigned w_div_2;
        } stereo;
        if (rendstate.stereo.mode != RENDSTEREO_SIDEBYSIDE) {
            // TODO: use userdata screen data
            glViewport(0, 0, rendstate.res.current.width, rendstate.res.current.height);
            glMatrixMode(GL_PROJECTION);
            glLoadMatrixf((float*)rpldata->projmat);
            glMatrixMode(GL_MODELVIEW);
            glLoadMatrixf((float*)rpldata->viewmat);
        } else {
            stereo.eye = 0;
            stereo.eyeoff = rendstate.stereo.eyedist * 0.5f;
            if (rendstate.stereo.inset) stereo.eyeoff = -stereo.eyeoff;
            stereo.w_div_2 = rendstate.res.current.width / 2;
            glViewport(0, 0, stereo.w_div_2, rendstate.res.current.height);
            goto sidebyside_skipnexteye;

            sidebyside_nexteye:;
            stereo.eyeoff = -stereo.eyeoff;
            glViewport(stereo.w_div_2, 0, rendstate.res.current.width - stereo.w_div_2, rendstate.res.current.height);

            sidebyside_skipnexteye:;

            if (!rendstate.stereo.inset) {
                glMatrixMode(GL_PROJECTION);
                glLoadMatrixf((float*)rpldata->projmat);
                glMatrixMode(GL_MODELVIEW);
                glLoadMatrixf((float*)rpldata->viewmat);
                glTranslatef(stereo.eyeoff, 0.0f, 0.0f);
            } else {
                glMatrixMode(GL_PROJECTION);
                float tmpmat[4][4] = {
                    {1.0f, 0.0f, 0.0f, 0.0f},
                    {0.0f, 1.0f, 0.0f, 0.0f},
                    {stereo.eyeoff, 0.0f, 1.0f, 0.0f},
                    {0.0f, 0.0f, 0.0f, 1.0f}
                };
                glLoadMatrixf((float*)tmpmat);
                glMultMatrixf((float*)rpldata->projmat);
                glMatrixMode(GL_MODELVIEW);
                glLoadMatrixf((float*)rpldata->viewmat);
            }
        }

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_TEXTURE_2D);
        if (!r_gl_data.fastclear) {
            glDepthFunc(GL_LEQUAL);
            glDepthRange(0.0, 1.0);
        } else if (r_gl_data.gl11.oddframe) {
            glDepthFunc(GL_LEQUAL);
            glDepthRange(0.0, 0.45);
        } else {
            glDepthFunc(GL_GEQUAL);
            glDepthRange(1.0, 0.55);
        }
        glDepthMask(GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        float z = 2.0f;

        // opaque geometry
        glBegin(GL_QUADS);
            #if 1
            glColor3f(tsini, tcosn, tsinn);
            glVertex3f(-1.0f, 1.0f, z);
            glColor3f(tcosi, tsini, tcosn);
            glVertex3f(-1.0f, -1.0f, z);
            glColor3f(tsinn, tcosi, tsini);
            glVertex3f(1.0f, -1.0f, z);
            glColor3f(tcosn, tsinn, tcosi);
            glVertex3f(1.0f, 1.0f, z);
            #endif
            glColor3f(1.0f, 0.0f, 0.0f);
            glVertex3f(-0.5f, 0.5f, z);
            glColor3f(0.5f, 1.0f, 0.0f);
            glVertex3f(-0.5f, -0.5f, z);
            glColor3f(0.0f, 1.0f, 1.0f);
            glVertex3f(0.5f, -0.5f, z);
            glColor3f(0.5f, 0.0f, 1.0f);
            glVertex3f(0.5f, 0.5f, z);
            glColor3f(0.0f, 0.0f, 0.0f);
            glVertex3f(-1.0f, 0.025f + tsin2, z);
            glColor3f(0.0f, 0.0f, 0.0f);
            glVertex3f(-1.0f, -0.025f + tsin2, z);
            glColor3f(0.0f, 0.0f, 0.0f);
            glVertex3f(1.0f, -0.025f + tsin2, z);
            glColor3f(0.0f, 0.0f, 0.0f);
            glVertex3f(1.0f, 0.025f + tsin2, z);
            glColor3f(1.0f, 1.0f, 1.0f);
            glVertex3f(-0.025f + tsin, 1.0f, z);
            glColor3f(1.0f, 1.0f, 1.0f);
            glVertex3f(-0.025f + tsin, -1.0f, z);
            glColor3f(1.0f, 1.0f, 1.0f);
            glVertex3f(0.025f + tsin, -1.0f, z);
            glColor3f(1.0f, 1.0f, 1.0f);
            glVertex3f(0.025f + tsin, 1.0f, z);
            glColor3f(0.0f, 0.5f, 0.0f);
            glVertex3f(-0.5f, -1.0f, z);
            glColor3f(0.0f, 0.5f, 0.0f);
            glVertex3f(-0.5f, -1.0f, z - 1.0f);
            glColor3f(0.0f, 0.5f, 0.0f);
            glVertex3f(0.5f, -1.0f, z - 1.0f);
            glColor3f(0.0f, 0.5f, 0.0f);
            glVertex3f(0.5f, -1.0f, z);
        glEnd();

        glEnable(GL_CULL_FACE);

        if (testmodel) r_gl_rendermodel_legacy(&testmodel->model, NULL);

        glEnable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        // lightmaps
        glBegin(GL_QUADS);
            glColor3f(1.0f, 1.0f, 1.0f);
            glVertex3f(-1.0f, 1.0f, z);
            glColor3f(0.5f, 0.5f, 0.5f);
            glVertex3f(-1.0f, -1.0f, z);
            glColor3f(0.0f, 0.0f, 0.0f);
            glVertex3f(1.0f, -1.0f, z);
            glColor3f(0.5f, 0.5f, 0.5f);
            glVertex3f(1.0f, 1.0f, z);
        glEnd();

        if (!r_gl_data.fastclear) glDepthRange(1.0, 1.0);
        else glDepthRange(0.5, 0.5);

        {
            float tmp[3];
            tmp[0] = rpldata->viewmat[3][0];
            tmp[1] = rpldata->viewmat[3][1];
            tmp[2] = rpldata->viewmat[3][2];
            rpldata->viewmat[3][0] = 0.0f;
            rpldata->viewmat[3][1] = 0.0f;
            rpldata->viewmat[3][2] = 0.0f;
            glMatrixMode(GL_MODELVIEW);
            if (rendstate.stereo.mode != RENDSTEREO_SIDEBYSIDE || !rendstate.stereo.inset) {
                glLoadMatrixf((float*)rpldata->viewmat);
            } else {
                float tmpmat[4][4] = {
                    {1.0f, 0.0f, 0.0f, 0.0f},
                    {0.0f, 1.0f, 0.0f, 0.0f},
                    {stereo.eyeoff * rpldata->stereoinsetmul, 0.0f, 1.0f, 0.0f},
                    {0.0f, 0.0f, 0.0f, 1.0f}
                };
                glLoadMatrixf((float*)tmpmat);
                glMultMatrixf((float*)rpldata->viewmat);
                glMatrixMode(GL_PROJECTION);
                glLoadMatrixf((float*)rpldata->projmat);
                glMatrixMode(GL_MODELVIEW);
            }
            rpldata->viewmat[3][0] = tmp[0];
            rpldata->viewmat[3][1] = tmp[1];
            rpldata->viewmat[3][2] = tmp[2];
            static const float s = 25.0f;
            glScalef(s, s, s);
        }

        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);

        // skybox
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, testskyboxids[4]);
        glBegin(GL_QUADS);
            // top
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(0.0f, 0.0f);
            glVertex3f(-1.0f, 1.0f, -1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(0.0f, 1.0f);
            glVertex3f(-1.0f, 1.0f, 1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(1.0f, 1.0f);
            glVertex3f(1.0f, 1.0f, 1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(1.0f, 0.0f);
            glVertex3f(1.0f, 1.0f, -1.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, testskyboxids[5]);
        glBegin(GL_QUADS);
            // bottom
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(0.0f, 0.0f);
            glVertex3f(-1.0f, -1.0f, 1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(0.0f, 1.0f);
            glVertex3f(-1.0f, -1.0f, -1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(1.0f, 1.0f);
            glVertex3f(1.0f, -1.0f, -1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(1.0f, 0.0f);
            glVertex3f(1.0f, -1.0f, 1.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, testskyboxids[2]);
        glBegin(GL_QUADS);
            // front
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(0.0f, 0.0f);
            glVertex3f(-1.0f, 1.0f, 1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(0.0f, 1.0f);
            glVertex3f(-1.0f, -1.0f, 1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(1.0f, 1.0f);
            glVertex3f(1.0f, -1.0f, 1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(1.0f, 0.0f);
            glVertex3f(1.0f, 1.0f, 1.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, testskyboxids[3]);
        glBegin(GL_QUADS);
            // back
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(0.0f, 1.0f);
            glVertex3f(1.0f, -1.0f, -1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(1.0f, 1.0f);
            glVertex3f(-1.0f, -1.0f, -1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(1.0f, 0.0f);
            glVertex3f(-1.0f, 1.0f, -1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(0.0f, 0.0f);
            glVertex3f(1.0f, 1.0f, -1.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, testskyboxids[1]);
        glBegin(GL_QUADS);
            // left
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(0.0f, 0.0f);
            glVertex3f(-1.0f, 1.0f, -1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(0.0f, 1.0f);
            glVertex3f(-1.0f, -1.0f, -1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(1.0f, 1.0f);
            glVertex3f(-1.0f, -1.0f, 1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(1.0f, 0.0f);
            glVertex3f(-1.0f, 1.0f, 1.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, testskyboxids[0]);
        glBegin(GL_QUADS);
            // right
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(0.0f, 0.0f);
            glVertex3f(1.0f, 1.0f, 1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(0.0f, 1.0f);
            glVertex3f(1.0f, -1.0f, 1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(1.0f, 1.0f);
            glVertex3f(1.0f, -1.0f, -1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(1.0f, 0.0f);
            glVertex3f(1.0f, 1.0f, -1.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);

        glEnable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        // clouds
        #if 1
        static const float cloudheight[2] = {0.01f, 0.01f};
        static const float cloudsize[2] = {35.0f, 43.0f};
        static const double cloudscroll[2][2] = {{0.0003, 0.0015}, {0.005, 0.0075}};
        static const float cloudopacity[2] = {0.25f, 0.07f};
        float cloudcoord[2][2] = {
            {fwrap(t * cloudscroll[0][0], 1.0f), fwrap(t * cloudscroll[0][1], 1.0f)},
            {fwrap(t * cloudscroll[1][0], 1.0f), fwrap(t * cloudscroll[1][1], 1.0f)}
        };
        glBindTexture(GL_TEXTURE_2D, testcloudids[0]);
        glBegin(GL_QUADS);
            // top
            glColor4f(1.0f, 1.0f, 1.0f, cloudopacity[0]);
            glTexCoord2f(cloudcoord[0][0], cloudcoord[0][1]);
            glVertex3f(-1.0f, cloudheight[0], -1.0f);
            glColor4f(1.0f, 1.0f, 1.0f, cloudopacity[0]);
            glTexCoord2f(cloudcoord[0][0], cloudcoord[0][1] + cloudsize[0]);
            glVertex3f(-1.0f, cloudheight[0], 1.0f);
            glColor4f(1.0f, 1.0f, 1.0f, cloudopacity[0]);
            glTexCoord2f(cloudcoord[0][0] + cloudsize[0], cloudcoord[0][1] + cloudsize[0]);
            glVertex3f(1.0f, cloudheight[0], 1.0f);
            glColor4f(1.0f, 1.0f, 1.0f, cloudopacity[0]);
            glTexCoord2f(cloudcoord[0][0] + cloudsize[0], cloudcoord[0][1]);
            glVertex3f(1.0f, cloudheight[0], -1.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, testcloudids[1]);
        glBegin(GL_QUADS);
            // bottom
            glColor4f(1.0f, 1.0f, 1.0f, cloudopacity[1]);
            glTexCoord2f(cloudcoord[1][0], cloudcoord[1][1]);
            glVertex3f(-1.0f, cloudheight[1], -1.0f);
            glColor4f(1.0f, 1.0f, 1.0f, cloudopacity[1]);
            glTexCoord2f(cloudcoord[1][0], cloudcoord[1][1] + cloudsize[1]);
            glVertex3f(-1.0f, cloudheight[1], 1.0f);
            glColor4f(1.0f, 1.0f, 1.0f, cloudopacity[1]);
            glTexCoord2f(cloudcoord[1][0] + cloudsize[1], cloudcoord[1][1] + cloudsize[1]);
            glVertex3f(1.0f, cloudheight[1], 1.0f);
            glColor4f(1.0f, 1.0f, 1.0f, cloudopacity[1]);
            glTexCoord2f(cloudcoord[1][0] + cloudsize[1], cloudcoord[1][1]);
            glVertex3f(1.0f, cloudheight[1], -1.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        #endif

        glDisable(GL_TEXTURE_2D);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // ui
        #if DEBUG(1)
        glBegin(GL_QUADS);
            glColor3f(0.0f, 0.0f, 0.0f);
            glVertex3f(-0.9f, 0.9f, 0.0f);
            glColor3f(0.0f, 0.0f, 0.0f);
            glVertex3f(-0.9f, 0.85f, 0.0f);
            glColor3f(0.0f, 0.0f, 0.0f);
            glVertex3f(-0.1f, 0.85f, 0.0f);
            glColor3f(0.0f, 0.0f, 0.0f);
            glVertex3f(-0.1f, 0.9f, 0.0f);
            glColor3f(0.5f, 0.5f, 0.5f);
            glVertex3f(-0.89f, 0.89f, 0.0f);
            glColor3f(0.5f, 0.5f, 0.5f);
            glVertex3f(-0.89f, 0.86f, 0.0f);
            glColor3f(0.5f, 0.5f, 0.5f);
            glVertex3f(-0.11f, 0.86f, 0.0f);
            glColor3f(0.5f, 0.5f, 0.5f);
            glVertex3f(-0.11f, 0.89f, 0.0f);
            int e = 10000 - rendstate.dbgprof->percent[-1];
            for (int i = rendstate.dbgprof->pointct - 1; i >= 0; --i) {
                float rgb[3];
                rgb[0] = rendstate.dbgprof->colors[i].r / 255.0f;
                rgb[1] = rendstate.dbgprof->colors[i].g / 255.0f;
                rgb[2] = rendstate.dbgprof->colors[i].b / 255.0f;
                float p = -0.89f + e / 10000.f * 0.78f;
                glColor3f(rgb[0], rgb[1], rgb[2]);
                glVertex3f(-0.89f, 0.89f, 0.0f);
                glColor3f(rgb[0], rgb[1], rgb[2]);
                glVertex3f(-0.89f, 0.86f, 0.0f);
                glColor3f(rgb[0], rgb[1], rgb[2]);
                glVertex3f(p, 0.86f, 0.0f);
                glColor3f(rgb[0], rgb[1], rgb[2]);
                glVertex3f(p, 0.89f, 0.0f);
                e -= rendstate.dbgprof->percent[i];
            }
        glEnd();
        #endif

        if (rendstate.stereo.mode == RENDSTEREO_SIDEBYSIDE) {
            if (!stereo.eye) {
                stereo.eye = 1;
                goto sidebyside_nexteye;
            }
        }

    }

    glFlush();
}
#endif
#endif

#if defined(PSRC_ENGINE_RENDERER_GL_USEGL33) || defined(PSRC_ENGINE_RENDERER_GL_USEGLES30)
static void r_gl_render_advanced(void) {
    r_gl_updatePlayerData();

    if (r_gl_data.fastclear) {
        glClear(GL_DEPTH_BUFFER_BIT);
    } else {
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }

    if (rendstate.lighting >= 1) {
        // TODO: render opaque materials front to back with light mapping
    } else {
        // TODO: render opaque materials front to back with basic lighting
    }

    glEnable(GL_CULL_FACE);

    // TODO: render entities

    glDisable(GL_CULL_FACE);
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

    glFinish();
}
#endif

GLuint r_gl_loadTexture(struct rc_texture* tex, GLenum filt, GLenum wrap) {
    //printf("LOADTEX [%u, %u, %u]\n", tex->channels, tex->width, tex->height);
    static const GLenum chtofrmt[4] = {
         GL_LUMINANCE,
         GL_LUMINANCE_ALPHA,
         GL_RGB,
         GL_RGBA
    };
    GLuint id;
    glGenTextures(1, &id);
    if (glGetError()) return 0;
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, chtofrmt[tex->channels - 1], tex->width, tex->height, 0, chtofrmt[tex->channels - 1], GL_UNSIGNED_BYTE, tex->data);
    if (glGetError()) {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &id);
        return 0;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

void* r_gl_takeScreenshot(unsigned* w, unsigned* h, unsigned* c) {
    size_t linesz = rendstate.res.current.width * 3;
    size_t framesz = linesz * rendstate.res.current.height;
    uint8_t* line = rcmgr_malloc(linesz);
    if (!line) return NULL;
    uint8_t* frame = rcmgr_malloc(framesz);
    if (!frame) {
        free(line);
        return NULL;
    }
    uint8_t* top = frame;
    uint8_t* bottom = &frame[linesz * (rendstate.res.current.height - 1)];
    glReadPixels(0, 0, rendstate.res.current.width, rendstate.res.current.height, GL_RGB, GL_UNSIGNED_BYTE, frame);
    register unsigned m = rendstate.res.current.height / 2;
    for (register unsigned i = 0; i < m; ++i) {
        memcpy(line, top, linesz);
        memcpy(top, bottom, linesz);
        memcpy(bottom, line, linesz);
        top += linesz;
        bottom -= linesz;
    }
    free(line);
    *w = rendstate.res.current.width;
    *h = rendstate.res.current.height;
    *c = 3;
    return frame;
}

#define SDL_GL_SetAttribute(a, v) if (SDL_GL_SetAttribute((a), (v))) plog(LL_WARN, "Failed to set " #a " to " #v ": %s", SDL_GetError())
bool r_gl_beforeCreateWindow(unsigned* f) {
    switch (rendstate.api) {
        #if PLATFORM != PLAT_EMSCR && !defined(PSRC_USESDL1)
            #ifdef PSRC_ENGINE_RENDERER_GL_USEGL11
            case RENDAPI_GL11:
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
                break;
            #endif
            #ifdef PSRC_ENGINE_RENDERER_GL_USEGL33
            case RENDAPI_GL33:
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
                break;
            #endif
            #ifdef PSRC_ENGINE_RENDERER_GL_USEGLES30
            case RENDAPI_GLES30:
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
                break;
            #endif
        #endif
        default:
            break;
    }
    #ifndef PSRC_USESDL1
        *f |= SDL_WINDOW_OPENGL;
    #else
        *f |= SDL_OPENGL;
    #endif
    char* tmp;
    tmp = cfg_getvar(&config, "Renderer", "gl.doublebuffer");
    if (tmp) {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, strbool(tmp, 1));
        free(tmp);
    } else {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    }
    #if PLATFORM != PLAT_EMSCR && !defined(PSRC_USESDL1)
        unsigned flags;
        tmp = cfg_getvar(&config, "Renderer", "gl.forwardcompat");
        if (strbool(tmp, 0)) flags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
        else flags = 0;
        free(tmp);
        tmp = cfg_getvar(&config, "Renderer", "gl.debug");
        if (strbool(tmp, 1)) flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
        free(tmp);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, flags);
    #endif
    r_gl_updateVSync();
    return true;
}

bool r_gl_afterCreateWindow(void) {
    #ifndef PSRC_USESDL1
        r_gl_data.ctx = SDL_GL_CreateContext(rendstate.window);
        if (!r_gl_data.ctx) {
            plog(LL_CRIT | LF_FUNC, "Failed to create OpenGL context: %s", SDL_GetError());
            return false;
        }
        SDL_GL_MakeCurrent(rendstate.window, r_gl_data.ctx);
    #endif
    #if PLATFORM != PLAT_EMSCR && defined(PSRC_ENGINE_RENDERER_GL_USEGLAD)
        if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
            plog(LL_CRIT | LF_FUNC, "Failed to load OpenGL");
            return false;
        }
    #endif
    r_gl_updateVSync();
    plog(LL_INFO, "OpenGL info:");
    bool cond[4];
    int tmpint[4];
    char* tmpstr;
    #ifndef PSRC_USESDL1
        cond[0] = !SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &tmpint[0]);
        cond[1] = !SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &tmpint[1]);
        cond[2] = !SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &tmpint[2]);
        if (cond[2]) {
            switch (tmpint[2]) {
                default: tmpstr = ""; break;
                case SDL_GL_CONTEXT_PROFILE_CORE: tmpstr = " core"; break;
                case SDL_GL_CONTEXT_PROFILE_COMPATIBILITY: tmpstr = " compat"; break;
                case SDL_GL_CONTEXT_PROFILE_ES: tmpstr = " ES"; break;
            }
        } else {
            tmpstr = "";
        }
        if (cond[0] && cond[1]) {
            plog(LL_INFO, "  Requested OpenGL version: %d.%d%s", tmpint[0], tmpint[1], tmpstr);
        }
    #endif
    tmpstr = (char*)glGetString(GL_VERSION);
    plog(LL_INFO, "  OpenGL version: %s", (tmpstr) ? tmpstr : "?");
    #ifdef GL_SHADING_LANGUAGE_VERSION
        tmpstr = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
        if (tmpstr) plog(LL_INFO, "  GLSL version: %s", tmpstr);
    #endif
    tmpstr = (char*)glGetString(GL_VENDOR);
    if (tmpstr) plog(LL_INFO, "  Vendor string: %s", tmpstr);
    tmpstr = (char*)glGetString(GL_RENDERER);
    if (tmpstr) plog(LL_INFO, "  Renderer string: %s", tmpstr);
    tmpstr = (char*)glGetString(GL_EXTENSIONS);
    if (tmpstr) {
        plog(LL_INFO, "  Extensions:");
        size_t ct;
        char** tmplist = splitstr(tmpstr, " ", false, &ct);
        char* tmp = cfg_getvar(&config, "Debug", "gl.allext");
        size_t max;
        if (strbool(tmp, false)) {
            max = ct;
        } else {
            #if DEBUG(1)
            max = 16;
            #else
            max = 8;
            #endif
        }
        free(tmp);
        for (size_t i = 0, passed = 0; i < ct; ++i) {
            if (!*tmplist[i]) continue;
            if (passed == max) {
                plog(LL_INFO, "    ... (enable gl.allext under [Debug] to show all)");
                break;
            }
            plog(LL_INFO, "    %s", tmplist[i]);
            ++passed;
        }
        #ifdef PSRC_ENGINE_RENDERER_GL_USEGL11
        if (rendstate.api == RENDAPI_GL11) {
            for (size_t i = 0, foundext = 0; i < ct; ++i) {
                if (!*tmplist[i]) continue;
                if (!strcasecmp(tmplist[i], "GL_ARB_multitexture")) {
                    r_gl_data.gl11.has_ARB_multitexture = 1;
                    ++foundext;
                } else if (!strcasecmp(tmplist[i], "GL_ARB_texture_border_clamp")) {
                    r_gl_data.gl11.has_ARB_texture_border_clamp = 1;
                    ++foundext;
                } else if (!strcasecmp(tmplist[i], "GL_SGIS_texture_edge_clamp")) {
                    r_gl_data.gl11.has_SGIS_texture_edge_clamp = 1;
                    ++foundext;
                }
                if (foundext == 3) break;
            }
        }
        #endif
        free(*tmplist);
        free(tmplist);
    }
    #ifdef PSRC_ENGINE_RENDERER_GL_USEGL11
    if (rendstate.api == RENDAPI_GL11) {
        plog(LL_INFO, "  GL_ARB_multitexture is %ssupported", (r_gl_data.gl11.has_ARB_multitexture) ? "" : "not ");
        plog(LL_INFO, "  GL_ARB_texture_border_clamp is %ssupported", (r_gl_data.gl11.has_ARB_texture_border_clamp) ? "" : "not ");
    }
    #endif
    #if GL_KHR_debug
        plog(LL_INFO, "  GL_KHR_debug is %ssupported", (glDebugMessageCallback) ? "" : "not ");
    #else
        plog(LL_INFO, "  GL_KHR_debug is not supported");
    #endif
    #ifdef PSRC_ENGINE_RENDERER_GL_USEGL11
    if (rendstate.api == RENDAPI_GL11) {
        glGetIntegerv(GL_MAX_LIGHTS, &r_gl_data.gl11.maxlights);
        plog(LL_INFO, "  Max lights: %d", r_gl_data.gl11.maxlights);
        if (r_gl_data.gl11.has_ARB_multitexture) {
            glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &r_gl_data.gl11.maxtexunits);
            plog(LL_INFO, "  Max texture units: %d", r_gl_data.gl11.maxtexunits);
        }
    }
    #endif
    cond[0] = !SDL_GL_GetAttribute(SDL_GL_ACCELERATED_VISUAL, &tmpint[0]);
    if (cond[0]) plog(LL_INFO, "  Hardware acceleration is %s", (tmpint[0]) ? "enabled" : "disabled");
    cond[0] = !SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &tmpint[0]);
    if (cond[0]) plog(LL_INFO, "  Double-buffering is %s", (tmpint[0]) ? "enabled" : "disabled");
    cond[0] = !SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &tmpint[0]);
    cond[1] = !SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &tmpint[1]);
    cond[2] = !SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &tmpint[2]);
    cond[3] = !SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &tmpint[3]);
    if (cond[0] && cond[1] && cond[2]) {
        if (cond[3] && tmpint[3]) {
            plog(LL_INFO, "  Color buffer format: R%dG%dB%dA%d", tmpint[0], tmpint[1], tmpint[2], tmpint[3]);
        } else {
            plog(LL_INFO, "  Color buffer format: R%dG%dB%d", tmpint[0], tmpint[1], tmpint[2]);
        }
    }
    cond[0] = !SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &tmpint[0]);
    if (cond[0]) {
        plog(LL_INFO, "  Depth buffer format: D%d", tmpint[0]);
    }
    tmpstr = cfg_getvar(&config, "Renderer", "gl.fastclear");
    #if DEBUG(1) || PLATFORM == PLAT_EMSCR
        // makes debugging easier
        r_gl_data.fastclear = strbool(tmpstr, false);
    #else
        r_gl_data.fastclear = strbool(tmpstr, true);
    #endif
    free(tmpstr);
    glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
    //glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glViewport(0, 0, rendstate.res.current.width, rendstate.res.current.height);
    r_gl_data.updateframe = 1;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_TEXTURE_2D);

    GLenum clamp;
    #if PLATFORM != PLAT_EMSCR
        clamp = (r_gl_data.gl11.has_SGIS_texture_edge_clamp) ? GL_CLAMP_TO_EDGE_SGIS : GL_CLAMP;
    #else
        clamp = GL_CLAMP_TO_EDGE;
    #endif
    if (testskybox[0]) testskyboxids[0] = r_gl_loadTexture(testskybox[0], GL_LINEAR, clamp);
    if (testskybox[1]) testskyboxids[1] = r_gl_loadTexture(testskybox[1], GL_LINEAR, clamp);
    if (testskybox[2]) testskyboxids[2] = r_gl_loadTexture(testskybox[2], GL_LINEAR, clamp);
    if (testskybox[3]) testskyboxids[3] = r_gl_loadTexture(testskybox[3], GL_LINEAR, clamp);
    if (testskybox[4]) testskyboxids[4] = r_gl_loadTexture(testskybox[4], GL_LINEAR, clamp);
    if (testskybox[5]) testskyboxids[5] = r_gl_loadTexture(testskybox[5], GL_LINEAR, clamp);
    if (testclouds[0]) testcloudids[0] = r_gl_loadTexture(testclouds[0], GL_LINEAR, GL_REPEAT);
    if (testclouds[1]) testcloudids[1] = r_gl_loadTexture(testclouds[1], GL_LINEAR, GL_REPEAT);

    return true;
}

bool r_gl_prepRenderer(void) {
    switch (rendstate.api) {
        #ifdef PSRC_ENGINE_RENDERER_GL_USEGL11
        case RENDAPI_GL11:
            r_gl_render = r_gl_render_legacy;
            break;
        #endif
        #if defined(PSRC_ENGINE_RENDERER_GL_USEGL33) || defined(PSRC_ENGINE_RENDERER_GL_USEGLES30)
        #ifdef PSRC_ENGINE_RENDERER_GL_USEGL33
        case RENDAPI_GL33:
        #endif
        #ifdef PSRC_ENGINE_RENDERER_GL_USEGLES30
        case RENDAPI_GLES30:
        #endif
            r_gl_render = r_gl_render_advanced;
            break;
        #endif
        default:
            break;
    }
    VLB_INIT(r_gl_data.playerdata, 1, VLB_OOM_NOP);
    #if GL_KHR_debug
        if (glDebugMessageCallback) glDebugMessageCallback(r_gl_dbgcb, NULL);
    #endif

    testmodel = getRc(RC_MODEL, "game:test/test_model", NULL, 0, NULL);
    {
        srand(altutime());
        unsigned sbn = rand() % 10;
        struct charbuf cb;
        cb_init(&cb, 128);
        cb_addstr(&cb, ":textures/skyboxes/");
        {
            static char num[12];
            sprintf(num, "%u", sbn);
            cb_addstr(&cb, num);
        }
        size_t l = cb.len;
        for (unsigned i = 0; i < 6; ++i) {
            cb_add(&cb, '/');
            cb_addstr(&cb, (char*[]){"east", "west", "north", "south", "sky", "ground"}[i]);
            testskybox[i] = getRc(RC_TEXTURE, cb_peek(&cb), NULL, 0, NULL);
            cb.len = l;
        }
        cb_dump(&cb);
    }
    testclouds[0] = getRc(RC_TEXTURE, ":textures/clouds/1/2", NULL, 0, NULL);
    testclouds[1] = getRc(RC_TEXTURE, ":textures/clouds/3/3", NULL, 0, NULL);

    return true;
}

void r_gl_beforeDestroyWindow(void) {
    if (testmodel) rlsRc(testmodel, false);
    if (testskybox[0]) rlsRc(testskybox[0], false);
    if (testskybox[1]) rlsRc(testskybox[1], false);
    if (testskybox[2]) rlsRc(testskybox[2], false);
    if (testskybox[3]) rlsRc(testskybox[3], false);
    if (testskybox[4]) rlsRc(testskybox[4], false);
    if (testskybox[5]) rlsRc(testskybox[5], false);
    if (testclouds[0]) rlsRc(testclouds[0], false);
    if (testclouds[1]) rlsRc(testclouds[1], false);

    #ifndef PSRC_USESDL1
        if (r_gl_data.ctx) SDL_GL_DeleteContext(r_gl_data.ctx);
    #endif
}
