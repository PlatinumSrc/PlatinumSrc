#include "resource.h"
#include "common.h"

#include "../debug.h"

#include "logging.h"
#include "string.h"
#include "filesystem.h"
#include "threading.h"
#include "crc.h"

#ifndef MODULE_SERVER
    #include "../../stb/stb_image.h"
    #include "../../stb/stb_image_resize.h"
    #include "../../stb/stb_vorbis.h"
    #include "../../minimp3/minimp3_ex.h"
#endif

#if PLATFORM != PLAT_NXDK
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../glue.h"

#undef loadResource
#undef freeResource
#undef grabResource
#undef releaseResource
#undef setRcAtt
#undef setRcAttData
#undef setRcAttCallback
#undef getRcAtt
#undef delRcAtt

#ifndef PSRC_NOMT
static mutex_t rclock; // TODO: turn into an access lock
static mutex_t rcattidlock;
#endif

union __attribute__((packed)) resource {
    void* ptr;
    struct rc_config* config;
    struct rc_font* font;
    struct rc_map* map;
    struct rc_material* material;
    struct rc_model* model;
    //struct rc_playermodel* playermodel;
    //struct rc_script* script;
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
    //struct rcopt_script* script;
    struct rcopt_sound* sound;
    struct rcopt_texture* texture;
    //struct rcopt_values* values;
};

struct __attribute__((packed)) rcdata {
    struct rcheader header;
    union __attribute__((packed)) {
        struct __attribute__((packed)) {
            struct rc_config config;
            //struct rcopt_config configopt;
        };
        struct __attribute__((packed)) {
            struct rc_font font;
            //struct rcopt_font fontopt;
        };
        struct __attribute__((packed)) {
            struct rc_map map;
            struct rcopt_map mapopt;
        };
        struct __attribute__((packed)) {
            struct rc_material material;
            struct rcopt_material materialopt;
        };
        struct __attribute__((packed)) {
            struct rc_model model;
            struct rcopt_model modelopt;
        };
        struct __attribute__((packed)) {
            //struct rc_playermodel playermodel;
            //struct rcopt_playermodel playermodelopt;
        };
        struct __attribute__((packed)) {
            //struct rc_script script;
            //struct rcopt_script scriptopt;
        };
        struct __attribute__((packed)) {
            struct rc_sound sound;
            //struct rcopt_sound soundopt;
        };
        struct __attribute__((packed)) {
            struct rc_texture texture;
            struct rcopt_texture textureopt;
        };
        struct __attribute__((packed)) {
            struct rc_values values;
            //struct rcopt_values valuesopt;
        };
    };
};

struct __attribute__((packed)) rcgroup {
    int len;
    int size;
    struct rcdata** data;
};

static struct rcgroup groups[RC__COUNT];
int groupsizes[RC__COUNT] = {2, 1, 1, 16, 8, 1, 4, 16, 16, 16};

struct rcopt_material materialopt_default = {
    RCOPT_TEXTURE_QLT_HIGH
};
struct rcopt_model modelopt_default = {
    0, RCOPT_TEXTURE_QLT_HIGH
};
struct rcopt_sound soundopt_default = {
    true
};
struct rcopt_texture textureopt_default = {
    false, RCOPT_TEXTURE_QLT_HIGH
};

void* defaultopt[RC__COUNT] = {
    NULL,
    /*&mapopt_default*/ NULL,
    &materialopt_default,
    /*&modelopt_default*/ NULL,
    NULL,
    NULL,
    &soundopt_default,
    &textureopt_default,
};

static struct {
    int len;
    int size;
    char** paths;
    #ifndef PSRC_NOMT
    mutex_t lock;
    #endif
} modinfo;

static char** extlist[RC__COUNT] = {
    (char*[2]){".cfg", NULL},
    (char*[3]){".ttf", ".otf", NULL},
    (char*[2]){".pmf", NULL},
    (char*[2]){".txt", NULL},
    (char*[2]){".p3m", NULL},
    (char*[2]){".txt", NULL},
    (char*[3]){".psc", NULL},
    (char*[4]){".ogg", ".mp3", ".wav", NULL},
    (char*[5]){".png", ".jpg", ".tga", ".bmp", NULL},
    (char*[2]){".txt", NULL},
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
                char* srcstr = cb_reinit(&tmpcb, 256);
                if (!*srcstr || !strcmp(srcstr, "self")) {
                    prefix = RCPREFIX_SELF;
                } else if (!strcmp(srcstr, "common")) {
                    prefix = RCPREFIX_COMMON;
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
                free(srcstr);
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
        case RCPREFIX_COMMON: {
            for (int i = 0; i < modinfo.len; ++i) {
                if ((filestatus = getRcPath_try(&tmpcb, type, ext, modinfo.paths[i], "common", path, NULL)) >= 1) goto found;
                cb_clear(&tmpcb);
            }
            if ((filestatus = getRcPath_try(&tmpcb, type, ext, maindir, "common", path, NULL)) >= 1) goto found;
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
    size_t size = sizeof(struct rcheader);
    switch (t) {
        case RC_CONFIG:
            size += sizeof(struct rc_config);
            break;
        case RC_FONT:
            size += sizeof(struct rc_font);
            break;
        case RC_MAP:
            size += sizeof(struct rc_map) + sizeof(struct rcopt_map);
            break;
        case RC_MATERIAL:
            size += sizeof(struct rc_material) + sizeof(struct rcopt_material);
            break;
        case RC_MODEL:
            size += sizeof(struct rc_model) + sizeof(struct rcopt_model);
            break;
        case RC_SOUND:
            size += sizeof(struct rc_sound) + sizeof(struct rcopt_sound);
            break;
        case RC_TEXTURE:
            size += sizeof(struct rc_texture) + sizeof(struct rcopt_texture);
            break;
        case RC_VALUES:
            size += sizeof(struct rc_values);
            break;
        default:
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
            g->size *= 2;
            g->data = realloc(g->data, g->size * sizeof(*g->data));
        }
        ptr = malloc(size);
        ptr->header.index = g->len;
        g->data[g->len++] = ptr;
    }
    ptr->header.type = t;
    ptr->header.path = strdup(p);
    ptr->header.pathcrc = pcrc;
    ptr->header.refs = 1;
    ptr->header.att.data = NULL;
    return ptr;
}

static struct rcdata* loadResource_internal(enum rctype, const char*, union rcopt);

static void* loadResource_wrapper(enum rctype t, const char* uri, union rcopt o) {
    struct rcdata* r = loadResource_internal(t, uri, o);
    if (!r) return NULL;
    return (void*)r + sizeof(struct rcheader);
}
#define loadResource_wrapper(t, p, o) loadResource_wrapper((t), (p), (union rcopt){.ptr = (void*)(o)})

static struct rcdata* loadResource_internal(enum rctype t, const char* uri, union rcopt o) {
    char* ext;
    char* p = getRcPath(uri, t, &ext);
    if (!p) {
        plog(LL_ERROR, "Could not find resource %s", uri);
        return NULL;
    }
    uint32_t pcrc = strcrc32(p);
    struct rcdata* d;
    struct rcgroup* g = &groups[t];
    for (int i = 0; i < g->len; ++i) {
        d = g->data[i];
        if (d && d->header.pathcrc == pcrc && !strcmp(p, d->header.path)) {
            switch (t) {
                case RC_MATERIAL: {
                    if (o.material->quality != d->materialopt.quality) goto nomatch;
                } break;
                case RC_MODEL: {
                    if ((o.model->flags & P3M_LOADFLAG_IGNOREVERTS) < (d->modelopt.flags & P3M_LOADFLAG_IGNOREVERTS) ||
                        (o.model->flags & P3M_LOADFLAG_IGNOREBONES) < (d->modelopt.flags & P3M_LOADFLAG_IGNOREBONES) ||
                        (o.model->flags & P3M_LOADFLAG_IGNOREANIMS) < (d->modelopt.flags & P3M_LOADFLAG_IGNOREANIMS)) goto nomatch;
                    if (o.model->texture_quality != d->modelopt.texture_quality) goto nomatch;
                } break;
                case RC_TEXTURE: {
                    if (o.texture->needsalpha && d->texture.channels != RC_TEXTURE_FRMT_RGBA) goto nomatch;
                    if (o.texture->quality != d->textureopt.quality) goto nomatch;
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
                d->config.config = config;
            }
        } break;
        #ifndef MODULE_SERVER
        case RC_FONT: {
            SFT_Font* font = sft_loadfile(p);
            if (font) {
                d = loadResource_newptr(t, g, p, pcrc);
                d->font.font = font;
            }
        } break;
        #endif
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
                    if (!o.material) o.material = &materialopt_default;
                    struct rcopt_texture texopt = {false, o.material->quality};
                    d->material.texture = loadResource_wrapper(RC_TEXTURE, tmp, &texopt);
                    free(tmp);
                } else {
                    d->material.texture = NULL;
                }
                d->material.color[0] = 1.0;
                d->material.color[1] = 1.0;
                d->material.color[2] = 1.0;
                d->material.color[3] = 1.0;
                tmp = cfg_getvar(mat, NULL, "color");
                if (tmp) {
                    sscanf(tmp, "%f,%f,%f", &d->material.color[0], &d->material.color[1], &d->material.color[2]);
                }
                tmp = cfg_getvar(mat, NULL, "alpha");
                if (tmp) {
                    sscanf(tmp, "%f", &d->material.color[3]);
                }
            }
        } break;
        case RC_MODEL: {
            if (!o.model) o.model = &modelopt_default;
            struct p3m* m = p3m_loadfile(p, o.model->flags);
            if (m) {
                d = loadResource_newptr(t, g, p, pcrc);
                d->model.model = m;
                d->model.textures = malloc(m->indexgroupcount * sizeof(*d->model.textures));
                //struct rcopt_texture texopt = {false, o.model->texture_quality};
                for (int i = 0; i < m->indexgroupcount; ++i) {
                    //d->model.textures[i] = loadResource_wrapper(RC_TEXTURE, m->indexgroups[i].texture, &texopt);
                    d->model.textures[i] = NULL;
                }
            }
        } break;
        #ifndef MODULE_SERVER
        case RC_SOUND: {
            FILE* f = fopen(p, "rb");
            if (f) {
                if (!o.sound) o.sound = &soundopt_default;
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
                                d->sound.format = RC_SOUND_FRMT_WAV;
                                stb_vorbis_info info = stb_vorbis_get_info(v);
                                int len = stb_vorbis_stream_length_in_samples(v);
                                int ch = (info.channels > 1) + 1;
                                int size = len * ch * sizeof(int16_t);
                                d->sound.len = len;
                                d->sound.size = size;
                                d->sound.data = malloc(size);
                                d->sound.freq = info.sample_rate;
                                d->sound.channels = info.channels;
                                d->sound.is8bit = false;
                                d->sound.stereo = (info.channels > 1);
                                stb_vorbis_get_samples_short_interleaved(v, ch, (int16_t*)d->sound.data, len * ch);
                                stb_vorbis_close(v);
                                free(data);
                            } else {
                                d->sound.format = RC_SOUND_FRMT_VORBIS;
                                d->sound.size = sz;
                                d->sound.data = data;
                                d->sound.len = stb_vorbis_stream_length_in_samples(v);
                                stb_vorbis_info info = stb_vorbis_get_info(v);
                                d->sound.freq = info.sample_rate;
                                d->sound.channels = info.channels;
                                d->sound.stereo = (info.channels > 1);
                                stb_vorbis_close(v);
                            }
                        } else {
                            free(data);
                        }
                    }
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
                                d->sound.format = RC_SOUND_FRMT_WAV;
                                int len = m->samples / m->info.channels;
                                int size = m->samples * sizeof(mp3d_sample_t);
                                d->sound.len = len;
                                d->sound.size = size;
                                d->sound.data = malloc(size);
                                d->sound.freq = m->info.hz;
                                d->sound.channels = m->info.channels;
                                d->sound.is8bit = false;
                                d->sound.stereo = (m->info.channels > 1);
                                mp3dec_ex_read(m, (mp3d_sample_t*)d->sound.data, m->samples);
                                mp3dec_ex_close(m);
                                free(data);
                            } else {
                                d->sound.format = RC_SOUND_FRMT_MP3;
                                d->sound.size = sz;
                                d->sound.data = data;
                                d->sound.len = m->samples / m->info.channels;
                                d->sound.freq = m->info.hz;
                                d->sound.channels = m->info.channels;
                                d->sound.stereo = (m->info.channels > 1);
                                mp3dec_ex_close(m);
                            }
                        }
                        free(m);
                    }
                } else if (!strcmp(ext, ".wav")) {
                    SDL_RWops* rwops = SDL_RWFromFP(f, false);
                    if (rwops) {
                        SDL_AudioSpec spec;
                        uint8_t* data;
                        uint32_t sz;
                        if (SDL_LoadWAV_RW(rwops, false, &spec, &data, &sz)) {
                            SDL_AudioCVT cvt;
                            SDL_AudioFormat destfrmt;
                            if (SDL_AUDIO_BITSIZE(spec.format) == 8) {
                                destfrmt = AUDIO_U8;
                            } else {
                                destfrmt = AUDIO_S16SYS;
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
                                        d->sound.format = RC_SOUND_FRMT_WAV;
                                        d->sound.size = sz;
                                        d->sound.data = data;
                                        d->sound.len = sz / ((spec.channels > 1) + 1) / ((destfrmt == AUDIO_S16SYS) + 1);
                                        d->sound.freq = spec.freq;
                                        d->sound.channels = spec.channels;
                                        d->sound.is8bit = (destfrmt == AUDIO_U8);
                                        d->sound.stereo = (spec.channels > 1);
                                    }
                                } else {
                                    d = loadResource_newptr(t, g, p, pcrc);
                                    d->sound.format = RC_SOUND_FRMT_WAV;
                                    d->sound.size = sz;
                                    d->sound.data = data;
                                    d->sound.len = sz / ((spec.channels > 1) + 1) / ((destfrmt == AUDIO_S16SYS) + 1);
                                    d->sound.freq = spec.freq;
                                    d->sound.channels = spec.channels;
                                    d->sound.is8bit = (destfrmt == AUDIO_U8);
                                    d->sound.stereo = (spec.channels > 1);
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
                if (!o.texture) o.texture = &textureopt_default;
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
                    d->texture.width = w;
                    d->texture.height = h;
                    d->texture.channels = c;
                    d->texture.data = data;
                    d->textureopt = *o.texture;
                }
            }
        } break;
        #endif
        case RC_VALUES: {
            struct cfg* values = cfg_open(p);
            if (values) {
                d = loadResource_newptr(t, g, p, pcrc);
                d->values.values = values;
            }
        } break;
        default: break;
    }
    free(p);
    if (!d) plog(LL_WARN, "Failed to load resource %s", uri);
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
            freeResource_wrapper(r->material.texture);
        } break;
        case RC_MODEL: {
            for (unsigned i = 0; i < r->model.model->indexgroupcount; ++i) {
                freeResource_wrapper(r->model.textures[i]);
            }
            p3m_free(r->model.model);
            free(r->model.textures);
        } break;
        case RC_SOUND: {
            free(r->sound.data);
        } break;
        case RC_TEXTURE: {
            free(r->texture.data);
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

int8_t genRcAttKey(void) {
    #ifndef PSRC_NOMT
    lockMutex(&rcattidlock);
    #endif
    static int8_t key = 0;
    return key++;
    #ifndef PSRC_NOMT
    unlockMutex(&rcattidlock);
    #endif
}

static inline struct rcatt* setRcAtt_getptr(struct rcheader* rh, int8_t key) {
    struct rcatt* d;
    if (!rh->att.data) {
        rh->att.len = 1;
        rh->att.size = 1;
        d = malloc(rh->att.size * sizeof(*rh->att.data));
        rh->att.data = d;
        d->key = key;
        return d;
    }
    int index = -1;
    for (int i = 0; i < rh->att.len; ++i) {
        d = &rh->att.data[i];
        if (d->key < 0 || d->key == key) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        if (rh->att.len == rh->att.size) {
            rh->att.size *= 2;
            rh->att.data = realloc(rh->att.data, rh->att.size * sizeof(*rh->att.data));
        }
        d = &rh->att.data[rh->att.len++];
        d->key = key;
    }
    return d;
}

void setRcAtt(void* r, int8_t key, void* data, void (*cb)(void*)) {
    struct rcheader* rh = r - sizeof(struct rcheader);
    struct rcatt* d = setRcAtt_getptr(rh, key);
    d->data = data;
    d->cb = cb;
}

void setRcAttData(void* r, int8_t key, void* data) {
    struct rcheader* rh = r - sizeof(struct rcheader);
    struct rcatt* d = setRcAtt_getptr(rh, key);
    d->data = data;
}

void setRcAttCallback(void* r, int8_t key, void (*cb)(void*)) {
    struct rcheader* rh = r - sizeof(struct rcheader);
    struct rcatt* d = setRcAtt_getptr(rh, key);
    d->cb = cb;
}

bool getRcAtt(void* r, int8_t key, void** out) {
    struct rcheader* rh = r - sizeof(struct rcheader);
    if (rh->att.data) {
        for (int i = 0; i < rh->att.len; ++i) {
            struct rcatt* d = &rh->att.data[i];
            if (d->key == key) {
                *out = d->data;
                return true;
            }
        }
    }
    return false;
}

void delRcAtt(void* r, int8_t key) {
    struct rcheader* rh = r - sizeof(struct rcheader);
    if (rh->att.data) {
        for (int i = 0; i < rh->att.len; ++i) {
            struct rcatt* d = &rh->att.data[i];
            if (d->key == key) {
                if (d->cb) d->cb(d->data);
                d->key = -1;
            }
        }
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
    lockMutex(&modinfo.lock);
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
                plog(LL_WARN, "Unable to locate mod: %s", modnames[i]);
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
    unlockMutex(&modinfo.lock);
    #endif
}

char** queryModInfo(int* len) {
    #ifndef PSRC_NOMT
    lockMutex(&modinfo.lock);
    #endif
    if (modinfo.len > 0) {
        if (len) *len = modinfo.len;
        char** data = malloc((modinfo.len + 1) * sizeof(*data));
        for (int i = 0; i < modinfo.len; ++i) {
            data[i] = strdup(modinfo.paths[i]);
        }
        data[modinfo.len] = NULL;
        #ifndef PSRC_NOMT
        unlockMutex(&modinfo.lock);
        #endif
        return data;
    }
    #ifndef PSRC_NOMT
    unlockMutex(&modinfo.lock);
    #endif
    return NULL;
}

bool initResource(void) {
    #ifndef PSRC_NOMT
    if (!createMutex(&rclock)) return false;
    if (!createMutex(&rcattidlock)) return false;
    if (!createMutex(&modinfo.lock)) return false;
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
    destroyMutex(&rcattidlock);
    destroyMutex(&modinfo.lock);
    #endif
}
