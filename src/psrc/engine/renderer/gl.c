#if PLATFORM == PLAT_EMSCR || PLATFORM == PLAT_NXDK || PLATFORM == PLAT_DREAMCAST
    #include <GL/gl.h>
    #if PLATFORM == PLAT_NXDK
        #include <pbgl.h>
        #include <pbkit/pbkit.h>
        #include <pbkit/nv_regs.h>
    #endif
    #ifdef GL_KHR_debug
        #undef GL_KHR_debug
    #endif
    #define GL_KHR_debug 0
#else
    #include "../../../glad/gl.h"
    #ifndef GL_KHR_debug
        #define GL_KHR_debug 0
    #endif
#endif
#if GL_KHR_debug
    static void GLAD_API_PTR gl_dbgcb(GLenum src, GLenum type, GLuint id, GLenum sev, GLsizei l, const GLchar *m, const void *u) {
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
    #if PLATFORM != PLAT_NXDK
    SDL_GLContext ctx;
    #endif
    uint8_t fastclear : 1;
    float nearplane;
    float farplane;
    mat4 projmat;
    mat4 viewmat;
    #if PLATFORM == PLAT_NXDK
    float scale;
    #endif
    union {
        #ifdef PSRC_USEGL11
        struct {
        } gl11;
        #endif
        #ifdef PSRC_USEGL33
        struct {
        } gl33;
        #endif
        #ifdef PSRC_USEGLES30
        struct {
        } gles30;
        #endif
    };
} gldata;

static void gl_display(void) {
    #if PLATFORM != PLAT_NXDK
    SDL_GL_SwapWindow(rendstate.window);
    #else
    pb_wait_for_vbl();
    pbgl_swap_buffers();
    #endif
}

static void gl_calcProjMat(void) {
    glm_perspective(
        rendstate.fov * M_PI / 180.0, -rendstate.aspect,
        #if PLATFORM != PLAT_NXDK
        gldata.nearplane, gldata.farplane,
        #else
        gldata.nearplane * gldata.scale * gldata.scale,
        gldata.farplane * gldata.scale * gldata.scale,
        #endif
        gldata.projmat
    );
}

static inline void gl_calcViewMat(void) {
    static float campos[3];
    static float front[3];
    static float up[3];
    #if PLATFORM != PLAT_NXDK
    campos[0] = rendstate.campos[0];
    campos[1] = rendstate.campos[1];
    campos[2] = rendstate.campos[2];
    #else
    campos[0] = rendstate.campos[0] * gldata.scale;
    campos[1] = rendstate.campos[1] * gldata.scale;
    campos[2] = rendstate.campos[2] * gldata.scale;
    #endif
    static float rotradx, sinx, cosx;
    static float rotrady, siny, cosy;
    static float rotradz, sinz, cosz;
    rotradx = rendstate.camrot[0] * M_PI / 180.0;
    rotrady = rendstate.camrot[1] * -M_PI / 180.0;
    rotradz = rendstate.camrot[2] * M_PI / 180.0;
    sinx = sin(rotradx);
    cosx = cos(rotradx);
    siny = sin(rotrady);
    cosy = cos(rotrady);
    sinz = sin(rotradz);
    cosz = cos(rotradz);
    front[0] = (-siny) * cosx;
    front[1] = sinx;
    front[2] = cosy * cosx;
    up[0] = siny * sinx * cosz + sinz * cosy;
    up[1] = cosx * cosz;
    up[2] = (-cosy) * sinx * cosz + sinz * siny;
    glm_vec3_add(campos, front, front);
    glm_lookat(campos, front, up, gldata.viewmat);
}

static void gl_updateFrame(void) {
    glViewport(0, 0, rendstate.res.current.width, rendstate.res.current.height);
    gl_calcProjMat();
}

static void gl_updateVSync(void) {
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
    double dt = (double)(lt % 1000) / 1000.0;
    double t = (double)(lt / 1000) + dt;
    float tsin = sin(t * 0.179254 * M_PI) * 2.0;
    float tsin2 = fabs(sin(t * 0.374124 * M_PI));
    float tcos = cos(t * 0.214682 * M_PI) * 0.5;
    for (int ig = 0; ig < m->indexgroupcount; ++ig) {
        uint16_t indcount = m->indexgroups[ig].indexcount;
        uint16_t* inds = m->indexgroups[ig].indices;
        glBegin(GL_TRIANGLES);
        //glColor3f(1.0, 1.0, 1.0);
        for (uint16_t i = 0; i < indcount; ++i) {
            float tmp1 = verts[*inds].z * 7.5;
            #if 1
            int ci = (*inds * 0x10492851) ^ *inds;
            uint8_t c[3] = {ci >> 16, ci >> 8, ci};
            //glColor3f(c[0] / 255.0, c[1] / 255.0, c[2] / 255.0);
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
        glColor3f(0.0, 0.0, 0.0);
        glVertex3f(-1.0, -0.025 + tsin2, z);
        glColor3f(0.0, 0.0, 0.0);
        glVertex3f(1.0, -0.025 + tsin2, z);
        glColor3f(0.0, 0.0, 0.0);
        glVertex3f(1.0, 0.025 + tsin2, z);
        glColor3f(1.0, 1.0, 1.0);
        glVertex3f(-0.025 + tsin, 1.0, z);
        glColor3f(1.0, 1.0, 1.0);
        glVertex3f(-0.025 + tsin, -1.0, z);
        glColor3f(1.0, 1.0, 1.0);
        glVertex3f(0.025 + tsin, -1.0, z);
        glColor3f(1.0, 1.0, 1.0);
        glVertex3f(0.025 + tsin, 1.0, z);
        glColor3f(0.0, 0.5, 0.0);
        glVertex3f(-0.5, -1.0, z);
        glColor3f(0.0, 0.5, 0.0);
        glVertex3f(-0.5, -1.0, z - 1.0);
        glColor3f(0.0, 0.5, 0.0);
        glVertex3f(0.5, -1.0, z - 1.0);
        glColor3f(0.0, 0.5, 0.0);
        glVertex3f(0.5, -1.0, z);
    glEnd();

    glEnable(GL_CULL_FACE);

    if (testmodel) rendermodel_gl_legacy(testmodel->model, NULL);

    glDisable(GL_CULL_FACE);
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
#define SDL_SetHint(n, v) if (!SDL_SetHint((n), (v))) plog(LL_WARN, "Failed to set " #n " to %s: %s", (char*)(v), SDL_GetError())
#define SDL_SetHintWithPriority(n, v, p) if (!SDL_SetHintWithPriority((n), (v), (p))) plog(LL_WARN, "Failed to set " #n " to %s using " #p ": %s", (char*)(v), SDL_GetError())

static bool gl_beforeCreateWindow(unsigned* f) {
    switch (rendstate.api) {
        #if PLATFORM != PLAT_NXDK && PLATFORM != PLAT_EMSCR
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
        default:
            break;
    }
    #if PLATFORM != PLAT_NXDK
    *f |= SDL_WINDOW_OPENGL;
    char* tmp;
    tmp = cfg_getvar(config, "Renderer", "gl.doublebuffer");
    if (tmp) {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, strbool(tmp, 1));
        free(tmp);
    } else {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    }
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
    #else
    //pbgl_set_swap_interval(rendstate.vsync);
    #endif
    return true;
}

static bool gl_afterCreateWindow(void) {
    #if PLATFORM != PLAT_NXDK
    gldata.ctx = SDL_GL_CreateContext(rendstate.window);
    if (!gldata.ctx) {
        plog(LL_CRIT | LF_FUNC, "Failed to create OpenGL context: %s", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(rendstate.window, gldata.ctx);
    #if PLATFORM != PLAT_EMSCR
    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        plog(LL_CRIT | LF_FUNC, "Failed to load OpenGL");
        return false;
    }
    #endif
    gl_updateVSync();
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
    #if GL_KHR_debug
    if (glDebugMessageCallback) plog(LL_INFO, "  glDebugMessageCallback is supported");
    #endif
    tmpstr[0] = cfg_getvar(config, "Renderer", "gl.nearplane");
    if (tmpstr[0]) {
        gldata.nearplane = atof(tmpstr[0]);
        free(tmpstr[0]);
    } else {
        gldata.nearplane = 0.1;
    }
    tmpstr[0] = cfg_getvar(config, "Renderer", "gl.farplane");
    if (tmpstr[0]) {
        gldata.farplane = atof(tmpstr[0]);
        free(tmpstr[0]);
    } else {
        gldata.farplane = 1000.0;
    }
    #if PLATFORM == PLAT_NXDK
    tmpstr[0] = cfg_getvar(config, "Renderer", "nxdk.gl.scale");
    if (tmpstr[0]) {
        gldata.scale = atof(tmpstr[0]);
        free(tmpstr[0]);
    } else {
        gldata.scale = 25.0;
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
    #if GL_KHR_debug
    if (glDebugMessageCallback) glDebugMessageCallback(gl_dbgcb, NULL);
    #endif
    glClearColor(0.0, 0.0, 0.1, 1.0);
    gl_updateFrame();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    return true;
}

static bool gl_prepRenderer(void) {
    return true;
}

static void gl_beforeDestroyWindow(void) {
    #if PLATFORM != PLAT_NXDK
    if (gldata.ctx) SDL_GL_DeleteContext(gldata.ctx);
    #endif
}
