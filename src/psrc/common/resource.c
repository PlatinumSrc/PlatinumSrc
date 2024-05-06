// TODO: rewrite? (too much crust)

#include "resource.h"

#include "../common.h"
#include "../debug.h"

#include "logging.h"
#include "string.h"
#include "filesystem.h"
#include "threading.h"
#include "crc.h"

#ifndef PSRC_MODULE_SERVER
    #include "../../stb/stb_image.h"
    #include "../../stb/stb_image_resize.h"
    #include "../../stb/stb_vorbis.h"
    #ifdef PSRC_USEMINIMP3
        #include "../../minimp3/minimp3_ex.h"
    #endif
#endif

#if PLATFORM == PLAT_NXDK
    #include <SDL.h>
#elif defined(PSRC_USESDL1)
    #include <SDL/SDL.h>
#else
    #include <SDL2/SDL.h>
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../glue.h"

#undef loadResource
#undef freeResource
#undef grabResource
#undef releaseResource

#ifndef PSRC_NOMT
static mutex_t rclock;
#endif

union __attribute__((packed)) resource {
    void* ptr;
    struct rc_config* config;
    struct rc_font* font;
    struct rc_map* map;
    struct rc_material* material;
    struct rc_model* model;
    //struct rc_playermodel* playermodel;
    struct rc_script* script;
    struct rc_sound* sound;
    struct rc_texture* texture;
    struct rc_values* values;
};

union __attribute__((packed)) rcopt {
    void* ptr;
    //struct rcopt_config* config;
    //struct rcopt_font* font;
    struct rcopt_map* map;
    struct rcopt_material* material;
    struct rcopt_model* model;
    //struct rcopt_playermodel* playermodel;
    struct rcopt_script* script;
    struct rcopt_sound* sound;
    struct rcopt_texture* texture;
    //struct rcopt_values* values;
};

struct rcdata {
    struct rcheader header;
    union {
        struct {
            struct rc_config data;
            //struct rcopt_config opt;
        } config;
        struct {
            struct rc_font data;
            //struct rcopt_font opt;
        } font;
        struct {
            struct rc_map data;
            struct rcopt_map opt;
        } map;
        struct {
            struct rc_material data;
            struct rcopt_material opt;
        } material;
        struct {
            struct rc_model data;
            struct rcopt_model opt;
        } model;
        struct {
            //struct rc_playermodel data;
            //struct rcopt_playermodel opt;
        } playermodel;
        struct {
            struct rc_script data;
            struct rcopt_script opt;
        } script;
        struct {
            struct rc_sound data;
            struct rcopt_sound opt;
        } sound;
        struct {
            struct rc_texture data;
            struct rcopt_texture opt;
        } texture;
        struct {
            struct rc_values data;
            //struct rcopt_values opt;
        } values;
    };
};

struct __attribute__((packed)) rcgroup {
    int len;
    int size;
    struct rcdata** data;
};

static struct rcgroup groups[RC__COUNT];
static int groupsizes[RC__COUNT] = {2, 1, 1, 16, 8, 1, 4, 16, 16, 16};

struct rcopt_material materialopt_default = {
    RCOPT_TEXTURE_QLT_HIGH
};
struct rcopt_model modelopt_default = {
    0, RCOPT_TEXTURE_QLT_HIGH
};
struct rcopt_script scriptopt_default = {
    (struct pbc_opt){NULL, false}
};
struct rcopt_sound soundopt_default = {
    true
};
struct rcopt_texture textureopt_default = {
    false, RCOPT_TEXTURE_QLT_HIGH
};

static void* defaultopt[RC__COUNT] = {
    NULL,
    NULL,
    NULL,
    &materialopt_default,
    &modelopt_default,
    NULL,
    &scriptopt_default,
    &soundopt_default,
    &textureopt_default,
    NULL
};

static void* typenames[RC__COUNT] = {
    "config",
    "font",
    "map",
    "material",
    "model",
    "playermodel",
    "script",
    "sound",
    "texture",
    "values"
};

static struct {
    int len;
    int size;
    char** paths;
    #ifndef PSRC_NOMT
    struct accesslock lock;
    #endif
} modinfo;

static char** extlist[RC__COUNT] = {
    (char*[]){".cfg", NULL},
    (char*[]){".ttf", ".otf", NULL},
    (char*[]){".pmf", NULL},
    (char*[]){".txt", NULL},
    (char*[]){".p3m", NULL},
    (char*[]){".txt", NULL},
    (char*[]){".bas", NULL},
    (char*[]){".ogg", ".mp3", ".wav", NULL},
    (char*[]){".png", ".jpg", ".tga", ".bmp", NULL},
    (char*[]){".txt", NULL}
};

static inline int getRcPath_try(struct charbuf* tmpcb, enum rctype type, char** ext, const char* s, ...) {
    cb_addstr(tmpcb, s);
    va_list v;
    va_start(v, s);
    while ((s = va_arg(v, char*))) {
        cb_add(tmpcb, PATHSEP);
        cb_addstr(tmpcb, s);
    }
    va_end(v);
    char** exts = extlist[type];
    char* tmp;
    while ((tmp = *exts)) {
        int len = 0;
        char c;
        while ((c = *tmp)) {
            ++tmp;
            ++len;
            cb_add(tmpcb, c);
        }
        int status = isFile(cb_peek(tmpcb));
        if (status >= 1) {
            if (ext) *ext = *exts;
            return status;
        }
        cb_undo(tmpcb, len);
        ++exts;
    }
    return -1;
}
char* getRcPath(const char* uri, enum rctype type, char** ext) {
    struct charbuf tmpcb;
    cb_init(&tmpcb, 256);
    const char* tmp = uri;
    enum rcprefix prefix;
    while (1) {
        char c = *tmp;
        if (c) {
            if (c == ':') {
                uri = ++tmp;
                char* srcstr = cb_peek(&tmpcb);
                if (!*srcstr || !strcmp(srcstr, "self")) {
                    prefix = RCPREFIX_SELF;
                } else if (!strcmp(srcstr, "game")) {
                    prefix = RCPREFIX_GAME;
                } else if (!strcmp(srcstr, "mod")) {
                    prefix = RCPREFIX_MOD;
                } else if (!strcmp(srcstr, "user")) {
                    prefix = RCPREFIX_USER;
                } else if (!strcmp(srcstr, "engine")) {
                    prefix = RCPREFIX_ENGINE;
                } else {
                    cb_dump(&tmpcb);
                    free(srcstr);
                    return NULL;
                }
                cb_clear(&tmpcb);
                break;
            } else {
                cb_add(&tmpcb, c);
            }
        } else {
            cb_clear(&tmpcb);
            tmp = uri;
            prefix = RCPREFIX_SELF;
            break;
        }
        ++tmp;
    }
    int level = 0;
    char* path = strrelpath(tmp);
    char* tmp2 = path;
    int lastlen = 0;
    while (1) {
        char c = *tmp2;
        if (c == PATHSEP || !c) {
            char* tmp3 = &(cb_peek(&tmpcb))[lastlen];
            if (!strcmp(tmp3, "..")) {
                --level;
                if (level < 0) {
                    plog(LL_ERROR, "%s reaches out of bounds", path);
                    cb_dump(&tmpcb);
                    free(path);
                    return NULL;
                }
                tmpcb.len -= 2;
                if (tmpcb.len > 0) {
                    --tmpcb.len;
                    while (tmpcb.len > 0 && tmpcb.data[tmpcb.len - 1] != PATHSEP) {
                        --tmpcb.len;
                    }
                }
            } else if (!strcmp(tmp3, ".")) {
                tmpcb.len -= 1;
            } else {
                ++level;
                if (c) cb_add(&tmpcb, PATHSEP);
            }
            if (!c) break;
            lastlen = tmpcb.len;
        } else {
            cb_add(&tmpcb, c);
        }
        ++tmp2;
    }
    free(path);
    path = cb_reinit(&tmpcb, 256);
    int filestatus = -1;
    switch (prefix) {
        case RCPREFIX_SELF: {
            for (int i = 0; i < modinfo.len; ++i) {
                if ((filestatus = getRcPath_try(&tmpcb, type, ext, modinfo.paths[i], "games", gamedir, path, NULL)) >= 1) goto found;
                cb_clear(&tmpcb);
            }
            if ((filestatus = getRcPath_try(&tmpcb, type, ext, maindir, "games", gamedir, path, NULL)) >= 1) goto found;
        } break;
        case RCPREFIX_ENGINE: {
            for (int i = 0; i < modinfo.len; ++i) {
                if ((filestatus = getRcPath_try(&tmpcb, type, ext, modinfo.paths[i], "engine", path, NULL)) >= 1) goto found;
                cb_clear(&tmpcb);
            }
            if ((filestatus = getRcPath_try(&tmpcb, type, ext, maindir, "engine", path, NULL)) >= 1) goto found;
        } break;
        case RCPREFIX_GAME: {
            for (int i = 0; i < modinfo.len; ++i) {
                if ((filestatus = getRcPath_try(&tmpcb, type, ext, modinfo.paths[i], "games", path, NULL)) >= 1) goto found;
                cb_clear(&tmpcb);
            }
            if ((filestatus = getRcPath_try(&tmpcb, type, ext, maindir, "games", path, NULL)) >= 1) goto found;
        } break;
        case RCPREFIX_MOD: {
            if ((filestatus = getRcPath_try(&tmpcb, type, ext, userdir, "mods", path, NULL)) >= 1) goto found;
            cb_clear(&tmpcb);
            if ((filestatus = getRcPath_try(&tmpcb, type, ext, maindir, "mods", path, NULL)) >= 1) goto found;
        } break;
        case RCPREFIX_USER: {
            if ((filestatus = getRcPath_try(&tmpcb, type, ext, userdir, path, NULL)) >= 1) goto found;
        } break;
        default: break;
    }
    free(path);
    cb_dump(&tmpcb);
    return NULL;
    found:;
    free(path);
    path = cb_finalize(&tmpcb);
    if (filestatus > 1) {
        plog(LL_WARN, LW_SPECIALFILE(path));
    }
    return path;
}

static struct rcdata* loadResource_newptr(enum rctype t, struct rcgroup* g, const char* p, uint32_t pcrc) {
    struct rcdata* ptr = NULL;
    size_t size;
    switch (t) {
        case RC_CONFIG:
            size = offsetof(struct rcdata, config) + sizeof(ptr->config);
            break;
        case RC_FONT:
            size = offsetof(struct rcdata, font) + sizeof(ptr->font);
            break;
        case RC_MAP:
            size = offsetof(struct rcdata, map) + sizeof(ptr->map);
            break;
        case RC_MATERIAL:
            size = offsetof(struct rcdata, material) + sizeof(ptr->material);
            break;
        case RC_MODEL:
            size = offsetof(struct rcdata, model) + sizeof(ptr->model);
            break;
        case RC_PLAYERMODEL:
            size = offsetof(struct rcdata, playermodel) + sizeof(ptr->playermodel);
            break;
        case RC_SCRIPT:
            size = offsetof(struct rcdata, script) + sizeof(ptr->script);
            break;
        case RC_SOUND:
            size = offsetof(struct rcdata, sound) + sizeof(ptr->sound);
            break;
        case RC_TEXTURE:
            size = offsetof(struct rcdata, texture) + sizeof(ptr->texture);
            break;
        case RC_VALUES:
            size = offsetof(struct rcdata, values) + sizeof(ptr->values);
            break;
        default: // never happens but this is to silence some warnings
            size = 0;
            break;
    }
    for (int i = 0; i < g->len; ++i) {
        struct rcdata* d = g->data[i];
        if (!d) {
            ptr = malloc(size);
            ptr->header.index = i;
            g->data[i] = ptr;
        }
    }
    if (!ptr) {
        if (g->len == g->size) {
            if (g->size == 1) {
                ++g->size;
            } else {
                g->size *= 3;
                g->size /= 2;
            }
            g->data = realloc(g->data, g->size * sizeof(*g->data));
        }
        ptr = malloc(size);
        ptr->header.index = g->len;
        g->data[g->len++] = ptr;
    }
    ptr->header.type = t;
    ptr->header.path = strdup(p);
    ptr->header.pathcrc = pcrc;
    ptr->header.hasdatacrc = false;
    ptr->header.refs = 1;
    return ptr;
}

static struct rcdata* loadResource_internal(enum rctype, const char*, union rcopt);

static inline void* loadResource_wrapper(enum rctype t, const char* uri, union rcopt o) {
    struct rcdata* r = loadResource_internal(t, uri, o);
    if (!r) return NULL;
    return (void*)r + sizeof(struct rcheader);
}
#define loadResource_wrapper(t, p, o) loadResource_wrapper((t), (p), (union rcopt){.ptr = (void*)(o)})

static struct rcdata* loadResource_internal(enum rctype t, const char* uri, union rcopt o) {
    char* ext;
    char* p = getRcPath(uri, t, &ext);
    if (!p) {
        plog(LL_ERROR, "Could not find %s at resource path %s", typenames[t], uri);
        return NULL;
    }
    uint32_t pcrc = strcrc32(p);
    struct rcdata* d;
    struct rcgroup* g = &groups[t];
    if (!o.ptr) o.ptr = defaultopt[t];
    for (int i = 0; i < g->len; ++i) {
        d = g->data[i];
        if (d && d->header.pathcrc == pcrc && !strcmp(p, d->header.path)) {
            switch (t) {
                case RC_MATERIAL: {
                    if (o.material->quality != d->material.opt.quality) goto nomatch;
                } break;
                case RC_MODEL: {
                    if ((o.model->flags & P3M_LOADFLAG_IGNOREVERTS) < (d->model.opt.flags & P3M_LOADFLAG_IGNOREVERTS)) goto nomatch;
                    if ((o.model->flags & P3M_LOADFLAG_IGNOREBONES) < (d->model.opt.flags & P3M_LOADFLAG_IGNOREBONES)) goto nomatch;
                    if ((o.model->flags & P3M_LOADFLAG_IGNOREANIMS) < (d->model.opt.flags & P3M_LOADFLAG_IGNOREANIMS)) goto nomatch;
                    if (o.model->texture_quality != d->model.opt.texture_quality) goto nomatch;
                } break;
                case RC_SCRIPT: {
                    if (o.script->compileropt.findext != d->script.opt.compileropt.findext) goto nomatch;
                    if (o.script->compileropt.fakedata != d->script.opt.compileropt.fakedata) goto nomatch;
                } break;
                case RC_TEXTURE: {
                    if (o.texture->needsalpha && d->texture.data.channels != RC_TEXTURE_FRMT_RGBA) goto nomatch;
                    if (o.texture->quality != d->texture.opt.quality) goto nomatch;
                } break;
                default: break;
            }
            ++d->header.refs;
            free(p);
            return d;
            nomatch:;
        }
    }
    d = NULL;
    switch (t) {
        case RC_CONFIG: {
            struct cfg* config = cfg_open(p);
            if (config) {
                d = loadResource_newptr(t, g, p, pcrc);
                d->config.data.config = config;
            }
        } break;
        #ifndef PSRC_MODULE_SERVER
        case RC_FONT: {
            SFT_Font* font = sft_loadfile(p);
            if (font) {
                d = loadResource_newptr(t, g, p, pcrc);
                d->font.data.font = font;
            }
        } break;
        case RC_MATERIAL: {
            struct cfg* mat = cfg_open(p);
            if (mat) {
                d = loadResource_newptr(t, g, p, pcrc);
                char* tmp = cfg_getvar(mat, NULL, "base");
                if (tmp) {
                    char* tmp2 = getRcPath(tmp, RC_MATERIAL, NULL);
                    free(tmp);
                    if (tmp2) {
                        cfg_merge(mat, tmp2, false);
                        free(tmp2);
                    }
                }
                tmp = cfg_getvar(mat, NULL, "texture");
                if (tmp) {
                    struct rcopt_texture texopt = {false, o.material->quality};
                    d->material.data.texture = loadResource_wrapper(RC_TEXTURE, tmp, &texopt);
                    free(tmp);
                } else {
                    d->material.data.texture = NULL;
                }
                d->material.data.color[0] = 1.0f;
                d->material.data.color[1] = 1.0f;
                d->material.data.color[2] = 1.0f;
                d->material.data.color[3] = 1.0f;
                tmp = cfg_getvar(mat, NULL, "color");
                if (tmp) {
                    sscanf(tmp, "%f,%f,%f", &d->material.data.color[0], &d->material.data.color[1], &d->material.data.color[2]);
                }
                tmp = cfg_getvar(mat, NULL, "alpha");
                if (tmp) {
                    sscanf(tmp, "%f", &d->material.data.color[3]);
                }
                d->material.opt = *o.material;
            }
        } break;
        #endif
        case RC_MODEL: {
            struct p3m* m = p3m_loadfile(p, o.model->flags);
            if (m) {
                d = loadResource_newptr(t, g, p, pcrc);
                d->model.data.model = m;
                d->model.data.textures = malloc(m->indexgroupcount * sizeof(*d->model.data.textures));
                //struct rcopt_texture texopt = {false, o.model->texture_quality};
                for (int i = 0; i < m->indexgroupcount; ++i) {
                    //d->model.data.textures[i] = loadResource_wrapper(RC_TEXTURE, m->indexgroups[i].texture, &texopt);
                    d->model.data.textures[i] = NULL;
                }
                d->model.opt = *o.model;
            }
        } break;
        case RC_SCRIPT: {
            struct pb_script s;
            struct charbuf e;
            cb_init(&e, 128);
            if (pb_compilefile(p, &o.script->compileropt, &s, &e)) {
                cb_dump(&e);
                d = loadResource_newptr(t, g, p, pcrc);
                d->script.data.script = s;
                d->script.opt = *o.script;
            } else {
                plog(LL_ERROR, "Failed to compile script %s: %s", uri, cb_peek(&e));
                cb_dump(&e);
            }
        } break;
        #ifndef PSRC_MODULE_SERVER
        case RC_SOUND: {
            FILE* f = fopen(p, "rb");
            if (f) {
                if (!strcmp(ext, ".ogg")) {
                    fseek(f, 0, SEEK_END);
                    long sz = ftell(f);
                    if (sz > 0) {
                        uint8_t* data = malloc(sz);
                        fseek(f, 0, SEEK_SET);
                        fread(data, 1, sz, f);
                        stb_vorbis* v = stb_vorbis_open_memory(data, sz, NULL, NULL);
                        if (v) {
                            d = loadResource_newptr(t, g, p, pcrc);
                            if (o.sound->decodewhole) {
                                d->sound.data.format = RC_SOUND_FRMT_WAV;
                                stb_vorbis_info info = stb_vorbis_get_info(v);
                                int len = stb_vorbis_stream_length_in_samples(v);
                                int ch = (info.channels > 1) + 1;
                                int size = len * ch * sizeof(int16_t);
                                d->sound.data.len = len;
                                d->sound.data.size = size;
                                d->sound.data.data = malloc(size);
                                d->sound.data.freq = info.sample_rate;
                                d->sound.data.channels = info.channels;
                                d->sound.data.is8bit = false;
                                d->sound.data.stereo = (info.channels > 1);
                                d->sound.opt = *o.sound;
                                stb_vorbis_get_samples_short_interleaved(v, ch, (int16_t*)d->sound.data.data, len * ch);
                                stb_vorbis_close(v);
                                free(data);
                            } else {
                                d->sound.data.format = RC_SOUND_FRMT_VORBIS;
                                d->sound.data.size = sz;
                                d->sound.data.data = data;
                                d->sound.data.len = stb_vorbis_stream_length_in_samples(v);
                                stb_vorbis_info info = stb_vorbis_get_info(v);
                                d->sound.data.freq = info.sample_rate;
                                d->sound.data.channels = info.channels;
                                d->sound.data.stereo = (info.channels > 1);
                                d->sound.opt = *o.sound;
                                stb_vorbis_close(v);
                            }
                        } else {
                            free(data);
                        }
                    }
                #ifdef PSRC_USEMINIMP3
                } else if (!strcmp(ext, ".mp3")) {
                    fseek(f, 0, SEEK_END);
                    long sz = ftell(f);
                    if (sz > 0) {
                        uint8_t* data = malloc(sz);
                        fseek(f, 0, SEEK_SET);
                        fread(data, 1, sz, f);
                        mp3dec_ex_t* m = malloc(sizeof(*m));
                        if (mp3dec_ex_open_buf(m, data, sz, MP3D_SEEK_TO_SAMPLE)) {
                            free(data);
                            free(m);
                        } else {
                            d = loadResource_newptr(t, g, p, pcrc);
                            if (o.sound->decodewhole) {
                                d->sound.data.format = RC_SOUND_FRMT_WAV;
                                int len = m->samples / m->info.channels;
                                int size = m->samples * sizeof(mp3d_sample_t);
                                d->sound.data.len = len;
                                d->sound.data.size = size;
                                d->sound.data.data = malloc(size);
                                d->sound.data.freq = m->info.hz;
                                d->sound.data.channels = m->info.channels;
                                d->sound.data.is8bit = false;
                                d->sound.data.stereo = (m->info.channels > 1);
                                d->sound.opt = *o.sound;
                                mp3dec_ex_read(m, (mp3d_sample_t*)d->sound.data.data, m->samples);
                                mp3dec_ex_close(m);
                                free(data);
                            } else {
                                d->sound.data.format = RC_SOUND_FRMT_MP3;
                                d->sound.data.size = sz;
                                d->sound.data.data = data;
                                d->sound.data.len = m->samples / m->info.channels;
                                d->sound.data.freq = m->info.hz;
                                d->sound.data.channels = m->info.channels;
                                d->sound.data.stereo = (m->info.channels > 1);
                                d->sound.opt = *o.sound;
                                mp3dec_ex_close(m);
                            }
                        }
                        free(m);
                    }
                #endif
                } else if (!strcmp(ext, ".wav")) {
                    SDL_RWops* rwops = SDL_RWFromFP(f, false);
                    if (rwops) {
                        SDL_AudioSpec spec;
                        uint8_t* data;
                        uint32_t sz;
                        if (SDL_LoadWAV_RW(rwops, false, &spec, &data, &sz)) {
                            SDL_AudioCVT cvt;
                            int destfrmt;
                            {
                                bool is8bit;
                                #ifndef PSRC_USESDL1
                                is8bit = (SDL_AUDIO_BITSIZE(spec.format) == 8);
                                #else
                                is8bit = (spec.format == AUDIO_U8 || spec.format == AUDIO_S8);
                                #endif
                                if (is8bit) {
                                    destfrmt = AUDIO_U8;
                                } else {
                                    destfrmt = AUDIO_S16SYS;
                                }
                            }
                            int ret = SDL_BuildAudioCVT(
                                &cvt,
                                spec.format, spec.channels, spec.freq,
                                destfrmt, (spec.channels > 1) + 1, spec.freq
                            );
                            if (ret >= 0) {
                                if (ret) {
                                    cvt.len = sz;
                                    data = SDL_realloc(data, cvt.len * cvt.len_mult);
                                    cvt.buf = data;
                                    if (SDL_ConvertAudio(&cvt)) {
                                        free(data);
                                    } else {
                                        data = SDL_realloc(data, cvt.len_cvt);
                                        sz = cvt.len_cvt;
                                        d = loadResource_newptr(t, g, p, pcrc);
                                        d->sound.data.format = RC_SOUND_FRMT_WAV;
                                        d->sound.data.size = sz;
                                        d->sound.data.data = data;
                                        d->sound.data.len = sz / ((spec.channels > 1) + 1) / ((destfrmt == AUDIO_S16SYS) + 1);
                                        d->sound.data.freq = spec.freq;
                                        d->sound.data.channels = spec.channels;
                                        d->sound.data.is8bit = (destfrmt == AUDIO_U8);
                                        d->sound.data.stereo = (spec.channels > 1);
                                        d->sound.opt = *o.sound;
                                    }
                                } else {
                                    d = loadResource_newptr(t, g, p, pcrc);
                                    d->sound.data.format = RC_SOUND_FRMT_WAV;
                                    d->sound.data.size = sz;
                                    d->sound.data.data = data;
                                    d->sound.data.len = sz / ((spec.channels > 1) + 1) / ((destfrmt == AUDIO_S16SYS) + 1);
                                    d->sound.data.freq = spec.freq;
                                    d->sound.data.channels = spec.channels;
                                    d->sound.data.is8bit = (destfrmt == AUDIO_U8);
                                    d->sound.data.stereo = (spec.channels > 1);
                                    d->sound.opt = *o.sound;
                                }
                            } else {
                                free(data);
                            }
                        }
                        SDL_RWclose(rwops);
                    }
                }
                fclose(f);
            }
        } break;
        case RC_TEXTURE: {
            int w, h, c;
            if (stbi_info(p, &w, &h, &c)) {
                if (o.texture->needsalpha) {
                    c = 4;
                } else {
                    if (c < 3) c += 2;
                }
                int c2;
                unsigned char* data = stbi_load(p, &w, &h, &c2, c);
                if (data) {
                    if (o.texture->quality != RCOPT_TEXTURE_QLT_HIGH) {
                        int w2 = w, h2 = h;
                        switch ((uint8_t)o.texture->quality) {
                            case RCOPT_TEXTURE_QLT_MED: {
                                w2 /= 2;
                                h2 /= 2;
                            } break;
                            case RCOPT_TEXTURE_QLT_LOW: {
                                w2 /= 4;
                                h2 /= 4;
                            } break;
                        }
                        if (w2 < 1) w2 = 1;
                        if (h2 < 1) h2 = 1;
                        unsigned char* data2 = malloc(w * h * c);
                        int status = stbir_resize_uint8_generic(
                            data, w, h, 0,
                            data2, w2, h2, 0,
                            c, -1, 0,
                            STBIR_EDGE_WRAP, STBIR_FILTER_BOX, STBIR_COLORSPACE_LINEAR,
                            NULL
                        );
                        if (status) {
                            free(data);
                            w = w2;
                            h = h2;
                            data = data2;
                        }
                    }
                    d = loadResource_newptr(t, g, p, pcrc);
                    d->texture.data.width = w;
                    d->texture.data.height = h;
                    d->texture.data.channels = c;
                    d->texture.data.data = data;
                    d->texture.opt = *o.texture;
                }
            }
        } break;
        #endif
        case RC_VALUES: {
            struct cfg* values = cfg_open(p);
            if (values) {
                d = loadResource_newptr(t, g, p, pcrc);
                d->values.data.values = values;
            }
        } break;
        default: break;
    }
    free(p);
    if (!d) plog(LL_ERROR, "Failed to load %s at resource path %s", typenames[t], uri);
    return d;
}

void* loadResource(enum rctype t, const char* uri, void* o) {
    #ifndef PSRC_NOMT
    lockMutex(&rclock);
    #endif
    void* r = loadResource_wrapper(t, uri, o);
    #ifndef PSRC_NOMT
    unlockMutex(&rclock);
    #endif
    return r;
}

static void freeResource_internal(struct rcdata*);

static inline void freeResource_wrapper(void* r) {
    if (r) freeResource_internal(r - sizeof(struct rcheader));
}

static inline void freeResource_force(enum rctype type, struct rcdata* r) {
    switch (type) {
        case RC_MATERIAL: {
            freeResource_wrapper(r->material.data.texture);
        } break;
        case RC_MODEL: {
            for (unsigned i = 0; i < r->model.data.model->indexgroupcount; ++i) {
                freeResource_wrapper(r->model.data.textures[i]);
            }
            p3m_free(r->model.data.model);
            free(r->model.data.textures);
        } break;
        case RC_SCRIPT: {
            pb_deletescript(&r->script.data.script);
        } break;
        case RC_SOUND: {
            free(r->sound.data.data);
        } break;
        case RC_TEXTURE: {
            free(r->texture.data.data);
        } break;
        default: break;
    }
    free(r->header.path);
}

static void freeResource_internal(struct rcdata* _r) {
    enum rctype type = _r->header.type;
    int index = _r->header.index;
    struct rcdata* r = groups[type].data[index];
    --r->header.refs;
    if (!r->header.refs) {
        freeResource_force(type, r);
        free(r);
        groups[type].data[index] = NULL;
    }
}

void freeResource(void* r) {
    if (r) {
        #ifndef PSRC_NOMT
        lockMutex(&rclock);
        #endif
        freeResource_internal(r - sizeof(struct rcheader));
        #ifndef PSRC_NOMT
        unlockMutex(&rclock);
        #endif
    }
}

void grabResource(void* _r) {
    if (_r) {
        #ifndef PSRC_NOMT
        lockMutex(&rclock);
        #endif
        struct rcdata* r = _r - sizeof(struct rcheader);
        ++r->header.refs;
        #ifndef PSRC_NOMT
        unlockMutex(&rclock);
        #endif
    }
}

static inline void loadMods_addpath(char* p) {
    ++modinfo.len;
    if (modinfo.len == modinfo.size) {
        modinfo.size *= 2;
        modinfo.paths = realloc(modinfo.paths, modinfo.size * sizeof(*modinfo.paths));
    }
    modinfo.paths[modinfo.len - 1] = p;
}

void loadMods(const char* const* modnames, int modcount) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&modinfo.lock);
    #endif
    for (int i = 0; i < modinfo.len; ++i) {
        free(modinfo.paths[i]);
    }
    modinfo.len = 0;
    if (modcount > 0 && modnames && *modnames) {
        if (modinfo.size < 4) {
            modinfo.size = 4;
            modinfo.paths = realloc(modinfo.paths, modinfo.size * sizeof(*modinfo.paths));
        }
        #if DEBUG(1)
        {
            struct charbuf cb;
            cb_init(&cb, 256);
            cb_add(&cb, '{');
            if (modcount) {
                const char* tmp = modnames[0];
                char c;
                cb_add(&cb, '"');
                while ((c = *tmp)) {
                    if (c == '"') cb_add(&cb, '\\');
                    cb_add(&cb, c);
                    ++tmp;
                }
                cb_add(&cb, '"');
                for (int i = 1; i < modcount; ++i) {
                    cb_add(&cb, ',');
                    cb_add(&cb, ' ');
                    tmp = modnames[i];
                    cb_add(&cb, '"');
                    while ((c = *tmp)) {
                        if (c == '"' || c == '\\') cb_add(&cb, '\\');
                        cb_add(&cb, c);
                        ++tmp;
                    }
                    cb_add(&cb, '"');
                }
            }
            cb_add(&cb, '}');
            plog(LL_INFO | LF_DEBUG, "Requested mods: %s", cb_peek(&cb));
            cb_dump(&cb);
        }
        #endif
        for (int i = 0; i < modcount; ++i) {
            bool notfound = true;
            char* tmp = mkpath(userdir, "mods", modnames[i], NULL);
            if (isFile(tmp)) {
                free(tmp);
            } else {
                notfound = false;
                loadMods_addpath(tmp);
            }
            tmp = mkpath(maindir, "mods", modnames[i], NULL);
            if (isFile(tmp)) {
                free(tmp);
            } else {
                notfound = false;
                loadMods_addpath(tmp);
            }
            if (notfound) {
                plog(LL_ERROR, "Unable to locate mod: %s", modnames[i]);
            }
        }
        #if DEBUG(1)
        {
            struct charbuf cb;
            cb_init(&cb, 256);
            cb_add(&cb, '{');
            if (modinfo.len) {
                const char* tmp = modinfo.paths[0];
                char c;
                cb_add(&cb, '"');
                while ((c = *tmp)) {
                    if (c == '"') cb_add(&cb, '\\');
                    cb_add(&cb, c);
                    ++tmp;
                }
                cb_add(&cb, '"');
                for (int i = 1; i < modinfo.len; ++i) {
                    cb_add(&cb, ',');
                    cb_add(&cb, ' ');
                    tmp = modinfo.paths[i];
                    cb_add(&cb, '"');
                    while ((c = *tmp)) {
                        if (c == '"' || c == '\\') cb_add(&cb, '\\');
                        cb_add(&cb, c);
                        ++tmp;
                    }
                    cb_add(&cb, '"');
                }
            }
            cb_add(&cb, '}');
            plog(LL_INFO | LF_DEBUG, "Found mods: %s", cb_peek(&cb));
            cb_dump(&cb);
        }
        #endif
    } else {
        modinfo.size = 0;
        free(modinfo.paths);
        modinfo.paths = NULL;
    }
    #ifndef PSRC_NOMT
    releaseWriteAccess(&modinfo.lock);
    #endif
}

char** queryModInfo(int* len) {
    #ifndef PSRC_NOMT
    acquireReadAccess(&modinfo.lock);
    #endif
    if (modinfo.len > 0) {
        if (len) *len = modinfo.len;
        char** data = malloc((modinfo.len + 1) * sizeof(*data));
        for (int i = 0; i < modinfo.len; ++i) {
            data[i] = strdup(modinfo.paths[i]);
        }
        data[modinfo.len] = NULL;
        #ifndef PSRC_NOMT
        releaseReadAccess(&modinfo.lock);
        #endif
        return data;
    }
    #ifndef PSRC_NOMT
    releaseReadAccess(&modinfo.lock);
    #endif
    return NULL;
}

bool initResource(void) {
    #ifndef PSRC_NOMT
    if (!createMutex(&rclock)) return false;
    if (!createAccessLock(&modinfo.lock)) return false;
    #endif

    for (int i = 0; i < RC__COUNT; ++i) {
        groups[i].len = 0;
        groups[i].size = groupsizes[i];
        groups[i].data = malloc(groups[i].size * sizeof(*groups[i].data));
    }

    char* tmp = cfg_getvar(config, NULL, "mods");
    if (tmp) {
        int modcount;
        char** modnames = splitstrlist(tmp, ',', false, &modcount);
        free(tmp);
        loadMods((const char* const*)modnames, modcount);
        for (int i = 0; i < modcount; ++i) {
            free(modnames[i]);
        }
        free(modnames);
    } else {
        loadMods(NULL, 0);
    }

    return true;
}

void quitResource(void) {
    for (int i = 0; i < RC__COUNT; ++i) {
        int grouplen = groups[i].len;
        for (int j = 0; j < grouplen; ++j) {
            struct rcdata* r = groups[i].data[j];
            if (r) {
                freeResource_force(r->header.type, r);
                free(r);
            }
        }
    }

    #ifndef PSRC_NOMT
    destroyMutex(&rclock);
    destroyAccessLock(&modinfo.lock);
    #endif
}
