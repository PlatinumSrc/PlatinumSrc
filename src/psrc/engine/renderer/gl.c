#if PLATFORM == PLAT_EMSCR || PLATFORM == PLAT_NXDK || PLATFORM == PLAT_DREAMCAST
    #include <GL/gl.h>
    #if PLATFORM == PLAT_NXDK
        #include <pbgl.h>
        #include <pbkit/pbkit.h>
        #include <pbkit/nv_regs.h>
    #elif PLATFORM == PLAT_DREAMCAST
        #include <GL/glkos.h>
    #endif
    #ifdef GL_KHR_debug
        #undef GL_KHR_debug
    #endif
    #define GL_KHR_debug 0
#else
    #ifndef PSRC_USEGLAD
        #define GL_GLEXT_PROTOTYPES
        #include <GL/gl.h>
        #include <GL/glext.h>
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
    #ifndef PSRC_USEGLAD
        #pragma weak glDebugMessageCallback
    #endif
    static void GLDBGCB gl_dbgcb(GLenum src, GLenum type, GLuint id, GLenum sev, GLsizei l, const GLchar *m, const void *u) {
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
    #if PLATFORM != PLAT_NXDK
    SDL_GLContext ctx;
    #endif
    #endif
    uint8_t fastclear : 1;
    float nearplane;
    float farplane;
    float projmat[4][4];
    float viewmat[4][4];
    #if PLATFORM == PLAT_NXDK
    float scale;
    #endif
    union {
        #ifdef PSRC_USEGL11
        struct {
            char _placeholder;
        } gl11;
        #endif
        #ifdef PSRC_USEGL33
        struct {
            char _placeholder;
        } gl33;
        #endif
        #ifdef PSRC_USEGLES30
        struct {
            char _placeholder;
        } gles30;
        #endif
    };
} gldata = {
    .viewmat = {
        [3][3] = 1.0f
    },
    .projmat = {
        [2][3] = -1.0f
    }
};

static void gl_display(void) {
    #if PLATFORM == PLAT_NXDK
    pb_wait_for_vbl();
    pbgl_swap_buffers();
    #elif PLATFORM == PLAT_DREAMCAST
    glKosSwapBuffers();
    #else
    #ifndef PSRC_USESDL1
    SDL_GL_SwapWindow(rendstate.window);
    #else
    SDL_GL_SwapBuffers();
    #endif
    #endif
}

static void gl_calcProjMat(void) {
    #if PLATFORM != PLAT_NXDK
        #define gl_cpm_near gldata.nearplane
        #define gl_cpm_far gldata.farplane
    #else
        float gl_cpm_near = gldata.nearplane * gldata.scale * gldata.scale;
        float gl_cpm_far = gldata.farplane * gldata.scale * gldata.scale;
    #endif
    float tmp1 = 1.0f / tanf(rendstate.fov * (float)M_PI / 180.0f * 0.5f);
    float tmp2 = 1.0f / (gl_cpm_near - gl_cpm_far);
    gldata.projmat[0][0] = -(tmp1 / rendstate.aspect);
    gldata.projmat[1][1] = tmp1;
    gldata.projmat[2][2] = (gl_cpm_near + gl_cpm_far) * tmp2;
    gldata.projmat[3][2] = 2.0f * gl_cpm_near * gl_cpm_far * tmp2;
    #if PLATFORM != PLAT_NXDK
        #undef gl_cpm_near
        #undef gl_cpm_far
    #endif
}

static inline void gl_calcViewMat(void) {
    static float up[3];
    static float front[3];
    #if PLATFORM != PLAT_NXDK
        #define gl_cvm_campos rendstate.campos
    #else
        static float gl_cvm_campos[3];
        gl_cvm_campos[0] = rendstate.campos[0] * gldata.scale;
        gl_cvm_campos[1] = rendstate.campos[1] * gldata.scale;
        gl_cvm_campos[2] = rendstate.campos[2] * gldata.scale;
    #endif
    static float rotradx, rotrady, rotradz;
    rotradx = rendstate.camrot[0] * (float)M_PI / 180.0f;
    rotrady = rendstate.camrot[1] * (float)-M_PI / 180.0f;
    rotradz = rendstate.camrot[2] * (float)M_PI / 180.0f;
    static float sinx, cosx;
    static float siny, cosy;
    static float sinz, cosz;
    sinx = sin(rotradx);
    cosx = cos(rotradx);
    siny = sin(rotrady);
    cosy = cos(rotrady);
    sinz = sin(rotradz);
    cosz = cos(rotradz);
    up[0] = sinx * siny * cosz + cosy * sinz;
    up[1] = cosx * cosz;
    up[2] = -sinx * cosy * cosz + siny * sinz;
    front[0] = cosx * -siny;
    front[1] = sinx;
    front[2] = cosx * cosy;
    gldata.viewmat[0][0] = front[1] * up[2] - front[2] * up[1];
    gldata.viewmat[1][0] = front[2] * up[0] - front[0] * up[2];
    gldata.viewmat[2][0] = front[0] * up[1] - front[1] * up[0];
    gldata.viewmat[3][0] = -(gldata.viewmat[0][0] * gl_cvm_campos[0] + gldata.viewmat[1][0] * gl_cvm_campos[1] + gldata.viewmat[2][0] * gl_cvm_campos[2]);
    gldata.viewmat[0][1] = up[0];
    gldata.viewmat[1][1] = up[1];
    gldata.viewmat[2][1] = up[2];
    gldata.viewmat[3][1] = -(up[0] * gl_cvm_campos[0] + up[1] * gl_cvm_campos[1] + up[2] * gl_cvm_campos[2]);
    gldata.viewmat[0][2] = -front[0];
    gldata.viewmat[1][2] = -front[1];
    gldata.viewmat[2][2] = -front[2];
    gldata.viewmat[3][2] = front[0] * gl_cvm_campos[0] + front[1] * gl_cvm_campos[1] + front[2] * gl_cvm_campos[2];
    #if 0
    puts("VIEWMAT: {");
    for (int i = 0; i < 4; ++i) {
        printf("    {%+.03f, %+.03f, %+.03f, %+.03f}%s\n", gldata.viewmat[i][0], gldata.viewmat[i][1], gldata.viewmat[i][2], gldata.viewmat[i][3], (i == 3) ? "" : ",");
    }
    puts("}");
    #endif
    #if PLATFORM != PLAT_NXDK
        #undef gl_cvm_campos
    #endif
}

static void gl_updateFrame(void) {
    glViewport(0, 0, rendstate.res.current.width, rendstate.res.current.height);
    gl_calcProjMat();
}

static void gl_updateVSync(void) {
    #if PLATFORM != PLAT_NXDK
    #ifndef PSRC_USESDL1
    if (rendstate.vsync) {
        if (SDL_GL_SetSwapInterval(-1) == -1) SDL_GL_SetSwapInterval(1);
    } else {
        SDL_GL_SetSwapInterval(0);
    }
    #else
    SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, rendstate.vsync);
    #endif
    #else
    //pbgl_set_swap_interval(rendstate.vsync);
    #endif
}

static inline void gl_clearScreen(void) {
    if (gldata.fastclear) {
        glClear(GL_DEPTH_BUFFER_BIT);
    } else {
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }
}

#ifdef PSRC_USEGL11
static void rendermodel_gl_legacy(struct p3m* m, struct p3m_vertex* verts) {
    if (!verts) verts = m->vertices;
    long lt = SDL_GetTicks();
    #if 0
    int vertct = 0;
    for (int mc = 0; mc < 5; ++mc) {
    #endif
        double dt = (double)(lt % 1000) / 1000.0f;
        double t = (double)(lt / 1000) + dt;
        float tsin = sin(t * 0.179254f * M_PI) * 2.0f;
        float tsin2 = fabs(sin(t * 0.374124f * M_PI));
        float tcos = cos(t * 0.214682f * M_PI) * 0.5f;
        for (int ig = 0; ig < m->indexgroupcount; ++ig) {
            uint16_t indcount = m->indexgroups[ig].indexcount;
            uint16_t* inds = m->indexgroups[ig].indices;
            glBegin(GL_TRIANGLES);
            //glColor3f(1.0f, 1.0f, 1.0f);
            for (uint16_t i = 0; i < indcount; ++i) {
                float tmp1 = verts[*inds].z * 5.0f + 0.5f;
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
static void render_gl_legacy(void) {
    gl_clearScreen();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((float*)gldata.projmat);
    #if PLATFORM == PLAT_NXDK
    // workaround for some depth buffer issues
    glScalef(gldata.scale, gldata.scale, gldata.scale);
    #endif
    glMatrixMode(GL_MODELVIEW);
    gl_calcViewMat();
    glLoadMatrixf((float*)gldata.viewmat);
    #if PLATFORM == PLAT_NXDK
    glScalef(gldata.scale, gldata.scale, gldata.scale);
    #endif

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
static void render_gl_legacy(void) {
    long lt = SDL_GetTicks();
    double dt = (double)(lt % 1000) / 1000.0f;
    double t = (double)(lt / 1000) + dt;
    float tsin = sin(t * 0.827535f * M_PI);
    float tsin2 = sin(t * 0.628591f * M_PI);
    float tsinn = sin(t * M_PI) * 0.5f + 0.5f;
    float tcosn = cos(t * M_PI) * 0.5f + 0.5f;
    float tsini = 1.0f - tsinn;
    float tcosi = 1.0f - tcosn;

    glDepthMask(GL_TRUE);
    gl_clearScreen();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((float*)gldata.projmat);
    #if PLATFORM == PLAT_NXDK
    // workaround for some depth buffer issues
    glScalef(gldata.scale, gldata.scale, gldata.scale);
    #endif
    glMatrixMode(GL_MODELVIEW);
    gl_calcViewMat();
    glLoadMatrixf((float*)gldata.viewmat);
    #if PLATFORM == PLAT_NXDK
    glScalef(gldata.scale, gldata.scale, gldata.scale);
    #endif

    float z = 2.0f;

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

    if (testmodel) rendermodel_gl_legacy(testmodel->model, NULL);

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);

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

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}
#endif
#endif

#if defined(PSRC_USEGL33) || defined(PSRC_USEGLES30)
static void render_gl_advanced(void) {
    gl_clearScreen();

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

static void gl_render(void) {
    switch (rendstate.api) {
        #ifdef PSRC_USEGL11
        case RENDAPI_GL11:
            render_gl_legacy();
            break;
        #endif
        #if defined(PSRC_USEGL33) || defined(PSRC_USEGLES30)
        #ifdef PSRC_USEGL33
        case RENDAPI_GL33:
        #endif
        #ifdef PSRC_USEGLES30
        case RENDAPI_GLES30:
        #endif
            render_gl_advanced();
            break;
        #endif
        default:
            break;
    }
    glFinish();
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

static void* gl_takeScreenshot(int* w, int* h, int* s) {
    if (w) *w = rendstate.res.current.width;
    if (h) *h = rendstate.res.current.height;
    int linesz = rendstate.res.current.width * 3;
    int framesz = linesz * rendstate.res.current.height;
    if (s) *s = framesz;
    uint8_t* line = malloc(linesz);
    uint8_t* frame = malloc(framesz);
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
static bool gl_beforeCreateWindow(unsigned* f) {
    switch (rendstate.api) {
        #if PLATFORM != PLAT_NXDK && PLATFORM != PLAT_EMSCR
        #ifndef PSRC_USESDL1
        #ifdef PSRC_USEGL11
        case RENDAPI_GL11:
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
            break;
        #endif
        #ifdef PSRC_USEGL33
        case RENDAPI_GL33:
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
            break;
        #endif
        #ifdef PSRC_USEGLES30
        case RENDAPI_GLES30:
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            break;
        #endif
        #endif
        #endif
        default:
            break;
    }
    #if PLATFORM != PLAT_NXDK
    #if PLATFORM != PLAT_DREAMCAST
    #ifndef PSRC_USESDL1
    *f |= SDL_WINDOW_OPENGL;
    #else
    *f |= SDL_OPENGL;
    #endif
    #else
    (void)f;
    glKosInit();
    #endif
    char* tmp;
    tmp = cfg_getvar(config, "Renderer", "gl.doublebuffer");
    if (tmp) {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, strbool(tmp, 1));
        free(tmp);
    } else {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    }
    #ifndef PSRC_USESDL1
    #if PLATFORM != PLAT_EMSCR
    unsigned flags;
    tmp = cfg_getvar(config, "Renderer", "gl.forwardcompat");
    if (strbool(tmp, 0)) flags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
    else flags = 0;
    free(tmp);
    tmp = cfg_getvar(config, "Renderer", "gl.debug");
    if (strbool(tmp, 1)) flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
    free(tmp);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, flags);
    #endif
    #endif
    #else
    /*
    char* tmp;
    tmp = cfg_getvar(config, "Renderer", "pbgl.triplebuffer");
    if (tmp) {
        if (strbool(tmp, 0)) {
            pb_back_buffers(2);
        }
        free(tmp);
    }
    */
    //pbgl_set_swap_interval(rendstate.vsync);
    #endif
    gl_updateVSync();
    return true;
}

static bool gl_afterCreateWindow(void) {
    #if PLATFORM != PLAT_NXDK
    #ifndef PSRC_USESDL1
    gldata.ctx = SDL_GL_CreateContext(rendstate.window);
    if (!gldata.ctx) {
        plog(LL_CRIT | LF_FUNC, "Failed to create OpenGL context: %s", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(rendstate.window, gldata.ctx);
    #endif
    #if PLATFORM != PLAT_EMSCR && PLATFORM != PLAT_DREAMCAST && defined(PSRC_USEGLAD)
    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        plog(LL_CRIT | LF_FUNC, "Failed to load OpenGL");
        return false;
    }
    #endif
    #endif
    gl_updateVSync();
    plog(LL_INFO, "OpenGL info:");
    bool cond[4];
    int tmpint[4];
    char* tmpstr[1];
    #ifndef PSRC_USESDL1
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
    #endif
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
    #if GL_KHR_debug
    if (glDebugMessageCallback) plog(LL_INFO, "  glDebugMessageCallback is supported");
    #endif
    tmpstr[0] = cfg_getvar(config, "Renderer", "gl.near");
    if (tmpstr[0]) {
        gldata.nearplane = atof(tmpstr[0]);
        free(tmpstr[0]);
    } else {
        gldata.nearplane = 0.1f;
    }
    tmpstr[0] = cfg_getvar(config, "Renderer", "gl.far");
    if (tmpstr[0]) {
        gldata.farplane = atof(tmpstr[0]);
        free(tmpstr[0]);
    } else {
        gldata.farplane = 100.0f;
    }
    #if PLATFORM == PLAT_NXDK
    tmpstr[0] = cfg_getvar(config, "Renderer", "nxdk.gl.scale");
    if (tmpstr[0]) {
        gldata.scale = atof(tmpstr[0]);
        free(tmpstr[0]);
    } else {
        gldata.scale = 25.0f;
    }
    #endif
    tmpstr[0] = cfg_getvar(config, "Renderer", "gl.fastclear");
    #if DEBUG(1)
    // makes debugging easier
    gldata.fastclear = strbool(tmpstr[0], false);
    #else
    gldata.fastclear = strbool(tmpstr[0], true);
    #endif
    free(tmpstr[0]);
    return true;
}

static bool gl_prepRenderer(void) {
    #if GL_KHR_debug
    if (glDebugMessageCallback) glDebugMessageCallback(gl_dbgcb, NULL);
    #endif
    glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
    gl_updateFrame();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    return true;
}

static void gl_beforeDestroyWindow(void) {
    #ifndef PSRC_USESDL1
    #if PLATFORM != PLAT_NXDK
    #if PLATFORM != PLAT_DREAMCAST
    if (gldata.ctx) SDL_GL_DeleteContext(gldata.ctx);
    #else
    glKosShutdown();
    #endif
    #endif
    #endif
}
