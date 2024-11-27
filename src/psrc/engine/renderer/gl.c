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
        #include "../../../glad/gl.h"
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
        int ll = -1;
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

static struct {
    #ifndef PSRC_USESDL1
    SDL_GLContext ctx;
    #endif
    uint8_t fastclear : 1;
    float nearplane;
    float farplane;
    float projmat[4][4];
    float viewmat[4][4];
    union {
        #ifdef PSRC_ENGINE_RENDERER_GL_USEGL11
        struct {
            uint_fast8_t oddframe : 1;
            uint_fast8_t has_ARB_multitexture : 1;
            uint_fast8_t has_ARB_texture_border_clamp : 1;
            int maxlights;
        } gl11;
        #endif
        #ifdef PSRC_ENGINE_RENDERER_GL_USEGL33
        struct {
            char _placeholder;
        } gl33;
        #endif
        #ifdef PSRC_ENGINE_RENDERER_GL_USEGLES30
        struct {
            char _placeholder;
        } gles30;
        #endif
    };
} r_gl_data = {
    .viewmat = {
        [3][3] = 1.0f
    },
    .projmat = {
        [2][3] = -1.0f
    }
};

static void r_gl_display(void) {
    #ifndef PSRC_USESDL1
    SDL_GL_SwapWindow(rendstate.window);
    #else
    SDL_GL_SwapBuffers();
    #endif
}

static void r_gl_calcProjMat(void) {
    float tmp1 = 1.0f / tanf(rendstate.fov * (float)M_PI / 180.0f * 0.5f);
    float tmp2 = 1.0f / (r_gl_data.nearplane - r_gl_data.farplane);
    r_gl_data.projmat[0][0] = -(tmp1 / rendstate.aspect);
    r_gl_data.projmat[1][1] = tmp1;
    r_gl_data.projmat[2][2] = (r_gl_data.nearplane + r_gl_data.farplane) * tmp2;
    r_gl_data.projmat[3][2] = 2.0f * r_gl_data.nearplane * r_gl_data.farplane * tmp2;
}

static inline void r_gl_calcViewMat(void) {
    static float up[3];
    static float front[3];
    static float rotradx, rotrady, rotradz;
    rotradx = rendstate.camrot[0] * (float)M_PI / 180.0f;
    rotrady = rendstate.camrot[1] * -(float)M_PI / 180.0f;
    rotradz = rendstate.camrot[2] * (float)M_PI / 180.0f;
    static float sinx, cosx;
    static float siny, cosy;
    static float sinz, cosz;
    sinx = sinf(rotradx);
    cosx = cosf(rotradx);
    siny = sinf(rotrady);
    cosy = cosf(rotrady);
    sinz = sinf(rotradz);
    cosz = cosf(rotradz);
    up[0] = sinx * siny * cosz + cosy * sinz;
    up[1] = cosx * cosz;
    up[2] = -sinx * cosy * cosz + siny * sinz;
    front[0] = cosx * -siny;
    front[1] = sinx;
    front[2] = cosx * cosy;
    r_gl_data.viewmat[0][0] = front[1] * up[2] - front[2] * up[1];
    r_gl_data.viewmat[1][0] = front[2] * up[0] - front[0] * up[2];
    r_gl_data.viewmat[2][0] = front[0] * up[1] - front[1] * up[0];
    r_gl_data.viewmat[3][0] = -(r_gl_data.viewmat[0][0] * rendstate.campos[0] + r_gl_data.viewmat[1][0] * rendstate.campos[1] + r_gl_data.viewmat[2][0] * rendstate.campos[2]);
    r_gl_data.viewmat[0][1] = up[0];
    r_gl_data.viewmat[1][1] = up[1];
    r_gl_data.viewmat[2][1] = up[2];
    r_gl_data.viewmat[3][1] = -(up[0] * rendstate.campos[0] + up[1] * rendstate.campos[1] + up[2] * rendstate.campos[2]);
    r_gl_data.viewmat[0][2] = -front[0];
    r_gl_data.viewmat[1][2] = -front[1];
    r_gl_data.viewmat[2][2] = -front[2];
    r_gl_data.viewmat[3][2] = front[0] * rendstate.campos[0] + front[1] * rendstate.campos[1] + front[2] * rendstate.campos[2];
}

static void r_gl_updateFrame(void) {
    glViewport(0, 0, rendstate.res.current.width, rendstate.res.current.height);
    r_gl_calcProjMat();
}

static void r_gl_updateVSync(void) {
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
    r_gl_clearScreen();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((float*)r_gl_data.projmat);
    glMatrixMode(GL_MODELVIEW);
    r_gl_calcViewMat();
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
}
#else
static void r_gl_render_legacy(void) {
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
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        glDepthFunc(GL_LEQUAL);
        glDepthRange(0.1, 1.0);
    } else if (r_gl_data.gl11.oddframe) {
        glDepthFunc(GL_LEQUAL);
        glDepthRange(0.1, 0.5);
    } else {
        glDepthFunc(GL_GEQUAL);
        glDepthRange(0.9, 0.5);
    }

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((float*)r_gl_data.projmat);
    glMatrixMode(GL_MODELVIEW);
    r_gl_calcViewMat();
    glLoadMatrixf((float*)r_gl_data.viewmat);

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

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
    glEnable(GL_DEPTH_TEST);

    if (testmodel) r_gl_rendermodel_legacy(&testmodel->model, NULL);

    glDepthMask(GL_FALSE);
    glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);

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
    else if (r_gl_data.gl11.oddframe) glDepthRange(0.5, 0.5);
    else glDepthRange(0.5, 0.5);

    r_gl_data.viewmat[3][0] = 0.0f;
    r_gl_data.viewmat[3][1] = 0.0f;
    r_gl_data.viewmat[3][2] = 0.0f;
    glLoadMatrixf((float*)r_gl_data.viewmat);

    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    // skybox
    glBegin(GL_QUADS);
        // top
        glColor3f(0.0f, 0.0f, tsinn);
        glVertex3f(-1.0f, 1.0f, -1.0f);
        glColor3f(0.0f, 0.0f, tcosn);
        glVertex3f(-1.0f, 1.0f, 1.0f);
        glColor3f(0.0f, 0.0f, tsini);
        glVertex3f(1.0f, 1.0f, 1.0f);
        glColor3f(0.0f, 0.0f, tcosi);
        glVertex3f(1.0f, 1.0f, -1.0f);
        // bottom
        glColor3f(0.0f, 0.0f, tsinn);
        glVertex3f(-1.0f, -1.0f, 1.0f);
        glColor3f(0.0f, 0.0f, tcosn);
        glVertex3f(-1.0f, -1.0f, -1.0f);
        glColor3f(0.0f, 0.0f, tsini);
        glVertex3f(1.0f, -1.0f, -1.0f);
        glColor3f(0.0f, 0.0f, tcosi);
        glVertex3f(1.0f, -1.0f, 1.0f);
        // front
        glColor3f(0.0f, 0.0f, tcosn);
        glVertex3f(-1.0f, 1.0f, 1.0f);
        glColor3f(0.0f, 0.0f, tsinn);
        glVertex3f(-1.0f, -1.0f, 1.0f);
        glColor3f(0.0f, 0.0f, tcosi);
        glVertex3f(1.0f, -1.0f, 1.0f);
        glColor3f(0.0f, 0.0f, tsini);
        glVertex3f(1.0f, 1.0f, 1.0f);
        // back
        glColor3f(0.0f, 0.0f, tsini);
        glVertex3f(1.0f, -1.0f, -1.0f);
        glColor3f(0.0f, 0.0f, tcosn);
        glVertex3f(-1.0f, -1.0f, -1.0f);
        glColor3f(0.0f, 0.0f, tsinn);
        glVertex3f(-1.0f, 1.0f, -1.0f);
        glColor3f(0.0f, 0.0f, tcosi);
        glVertex3f(1.0f, 1.0f, -1.0f);
        // left
        glColor3f(0.0f, 0.0f, tsinn);
        glVertex3f(-1.0f, 1.0f, -1.0f);
        glColor3f(0.0f, 0.0f, tcosn);
        glVertex3f(-1.0f, -1.0f, -1.0f);
        glColor3f(0.0f, 0.0f, tsinn);
        glVertex3f(-1.0f, -1.0f, 1.0f);
        glColor3f(0.0f, 0.0f, tcosn);
        glVertex3f(-1.0f, 1.0f, 1.0f);
        // right
        glColor3f(0.0f, 0.0f, tsini);
        glVertex3f(1.0f, 1.0f, 1.0f);
        glColor3f(0.0f, 0.0f, tcosi);
        glVertex3f(1.0f, -1.0f, 1.0f);
        glColor3f(0.0f, 0.0f, tsini);
        glVertex3f(1.0f, -1.0f, -1.0f);
        glColor3f(0.0f, 0.0f, tcosi);
        glVertex3f(1.0f, 1.0f, -1.0f);
    glEnd();

    {
        float s = r_gl_data.nearplane * 100.0f;
        glScalef(s, s, s);
    }

    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_BLEND);

    // clouds
    static const float cloudheight[2] = {0.1, 0.1};
    glBegin(GL_QUADS);
        // top
        glColor4f(0.0f, tcosn, 0.0f, 0.25f);
        glVertex3f(-1.0f, cloudheight[0], -1.0f);
        glColor4f(0.0f, tsinn, 0.0f, 0.25f);
        glVertex3f(-1.0f, cloudheight[0], 1.0f);
        glColor4f(0.0f, tcosi, 0.0f, 0.25f);
        glVertex3f(1.0f, cloudheight[0], 1.0f);
        glColor4f(0.0f, tsini, 0.0f, 0.25f);
        glVertex3f(1.0f, cloudheight[0], -1.0f);
        // bottom
        glColor4f(tcosn, 0.0f, 0.0f, 0.25f);
        glVertex3f(-1.0f, cloudheight[1], -1.0f);
        glColor4f(tcosi, 0.0f, 0.0f, 0.25f);
        glVertex3f(-1.0f, cloudheight[1], 1.0f);
        glColor4f(tsinn, 0.0f, 0.0f, 0.25f);
        glVertex3f(1.0f, cloudheight[1], 1.0f);
        glColor4f(tsini, 0.0f, 0.0f, 0.25f);
        glVertex3f(1.0f, cloudheight[1], -1.0f);
    glEnd();

    if (!r_gl_data.fastclear) glDepthRange(0.0, 0.1);
    else if (r_gl_data.gl11.oddframe) glDepthRange(0.0, 0.1);
    else glDepthRange(1.0, 0.9);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

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
}
#endif
#endif

#if defined(PSRC_ENGINE_RENDERER_GL_USEGL33) || defined(PSRC_ENGINE_RENDERER_GL_USEGLES30)
static void r_gl_render_advanced(void) {
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
}
#endif

static void r_gl_render(void) {
    switch (rendstate.api) {
        #ifdef PSRC_ENGINE_RENDERER_GL_USEGL11
        case RENDAPI_GL11:
            r_gl_render_legacy();
            break;
        #endif
        #if defined(PSRC_ENGINE_RENDERER_GL_USEGL33) || defined(PSRC_ENGINE_RENDERER_GL_USEGLES30)
        #ifdef PSRC_ENGINE_RENDERER_GL_USEGL33
        case RENDAPI_GL33:
        #endif
        #ifdef PSRC_ENGINE_RENDERER_GL_USEGLES30
        case RENDAPI_GLES30:
        #endif
            r_gl_render_advanced();
            break;
        #endif
        default:
            break;
    }
    glFinish();
}

static void* r_gl_takeScreenshot(int* w, int* h, int* s) {
    if (w) *w = rendstate.res.current.width;
    if (h) *h = rendstate.res.current.height;
    int linesz = rendstate.res.current.width * 3;
    int framesz = linesz * rendstate.res.current.height;
    if (s) *s = framesz;
    uint8_t* line = rcmgr_malloc(linesz);
    uint8_t* frame = rcmgr_malloc(framesz);
    uint8_t* top = frame;
    uint8_t* bottom = &frame[linesz * (rendstate.res.current.height - 1)];
    glReadPixels(0, 0, rendstate.res.current.width, rendstate.res.current.height, GL_RGB, GL_UNSIGNED_BYTE, frame);
    for (int i = 0; i < rendstate.res.current.height / 2; ++i) {
        memcpy(line, top, linesz);
        memcpy(top, bottom, linesz);
        memcpy(bottom, line, linesz);
        top += linesz;
        bottom -= linesz;
    }
    return frame;
}

#define SDL_GL_SetAttribute(a, v) if (SDL_GL_SetAttribute((a), (v))) plog(LL_WARN, "Failed to set " #a " to " #v ": %s", SDL_GetError())
static bool r_gl_beforeCreateWindow(unsigned* f) {
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

static bool r_gl_afterCreateWindow(void) {
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
        char** tmplist = splitstr(tmpstr, " ", false, &tmpint[0]);
        char* tmp = cfg_getvar(&config, "Debug", "gl.allext");
        int max;
        if (strbool(tmp, false)) {
            max = tmpint[0];
        } else {
            #if DEBUG(1)
            max = 16;
            #else
            max = 8;
            #endif
        }
        free(tmp);
        for (int i = 0, ct = 0; i < tmpint[0]; ++i) {
            if (!*tmplist[i]) continue;
            if (ct == max) {
                plog(LL_INFO, "    ... (enable gl.allext under [Debug] to show all)");
                break;
            }
            plog(LL_INFO, "    %s", tmplist[i]);
            ++ct;
        }
        if (rendstate.api == RENDAPI_GL11) {
            for (int i = 0, foundext = 0; i < tmpint[0]; ++i) {
                if (!*tmplist[i]) continue;
                if (!strcasecmp(tmplist[i], "GL_ARB_multitexture")) {
                    r_gl_data.gl11.has_ARB_multitexture = 1;
                    ++foundext;
                } else if (!strcasecmp(tmplist[i], "GL_ARB_texture_border_clamp")) {
                    r_gl_data.gl11.has_ARB_texture_border_clamp = 1;
                    ++foundext;
                }
                if (foundext == 2) break;
            }
        }
        free(*tmplist);
        free(tmplist);
    }
    if (rendstate.api == RENDAPI_GL11) {
        plog(LL_INFO, "  GL_ARB_multitexture is %ssupported", (r_gl_data.gl11.has_ARB_multitexture) ? "" : "not ");
        plog(LL_INFO, "  GL_ARB_texture_border_clamp is %ssupported", (r_gl_data.gl11.has_ARB_texture_border_clamp) ? "" : "not ");
    }
    #if GL_KHR_debug
        plog(LL_INFO, "  GL_KHR_debug is %ssupported", (glDebugMessageCallback) ? "" : "not ");
    #else
        plog(LL_INFO, "  GL_KHR_debug is not supported");
    #endif
    if (rendstate.api == RENDAPI_GL11) {
        glGetIntegerv(GL_MAX_LIGHTS, &r_gl_data.gl11.maxlights);
        plog(LL_INFO, "  Max lights: %d", r_gl_data.gl11.maxlights);
    }
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
    tmpstr = cfg_getvar(&config, "Renderer", "gl.near");
    if (tmpstr) {
        r_gl_data.nearplane = atof(tmpstr);
        free(tmpstr);
    } else {
        r_gl_data.nearplane = 0.1f;
    }
    tmpstr = cfg_getvar(&config, "Renderer", "gl.far");
    if (tmpstr) {
        r_gl_data.farplane = atof(tmpstr);
        free(tmpstr);
    } else {
        r_gl_data.farplane = 100.0f;
    }
    tmpstr = cfg_getvar(&config, "Renderer", "gl.fastclear");
    #if DEBUG(1) || PLATFORM == PLAT_EMSCR
        // makes debugging easier
        r_gl_data.fastclear = strbool(tmpstr, false);
    #else
        r_gl_data.fastclear = strbool(tmpstr, true);
    #endif
    free(tmpstr);
    return true;
}

static bool r_gl_prepRenderer(void) {
    #if GL_KHR_debug
        if (glDebugMessageCallback) glDebugMessageCallback(r_gl_dbgcb, NULL);
    #endif
    glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
    r_gl_updateFrame();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    return true;
}

static void r_gl_beforeDestroyWindow(void) {
    #ifndef PSRC_USESDL1
        if (r_gl_data.ctx) SDL_GL_DeleteContext(r_gl_data.ctx);
    #endif
}
