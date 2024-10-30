#include "../rcmgralloc.h"
#include "string.h"
#include "memory.h"
#include "../undefalloc.h"

#include "resource.h"

#include "../common.h"
#include "../debug.h"

#include "logging.h"
#include "filesystem.h"
#include "threading.h"
#include "crc.h"
#include "time.h"

#ifndef PSRC_MODULE_SERVER
    #include "../../stb/stb_image.h"
    #include "../../stb/stb_image_resize.h"
    #include "../../stb/stb_vorbis.h"
    #ifdef PSRC_USEMINIMP3
        #include "../../minimp3/minimp3_ex.h"
    #endif
    #include "../engine/ptf.h"
#endif

#if PLATFORM == PLAT_NXDK || PLATFORM == PLAT_GDK
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

#ifndef PSRC_NOMT
static struct accesslock rclock;
#endif

static const char* const rctypenames[RC__COUNT] = {
    "config",
    "font",
    "map",
    "model",
    "script",
    "sound",
    "texture",
    "values"
};

static const char* const* const rcextensions[RC__COUNT] = {
    (const char* const[]){"cfg", NULL},
    (const char* const[]){"ttf", "otf", NULL},
    (const char* const[]){"pmf", NULL},
    (const char* const[]){"p3m", NULL},
    (const char* const[]){"bas", NULL},
    (const char* const[]){"ogg", "mp3", "wav", NULL},
    (const char* const[]){"ptf", "png", "jpg", "tga", "bmp", NULL},
    (const char* const[]){"txt", NULL}
};

static uintptr_t rctick; // using uintptr_t to get 64 bits on 64-bit windows

struct rcheader {
    enum rctype type;
    enum rcprefix prefix;
    char* path; // sanitized resource path without prefix (e.g. /textures/icon.png)
    uint32_t pathcrc;
    uint64_t datacrc;
    unsigned refs;
    unsigned index;
    uintptr_t zreftick;
    uint8_t forcefree : 1;
    uint8_t hasdatacrc : 1;
};
struct resource {
    struct rcheader header;
    union {
        void* data;
        struct {
            struct rc_config config;
            //struct rcopt_config config_opt;
            void* config_end[];
        };
        struct {
            struct rc_font font;
            //struct rcopt_font font_opt;
            void* font_end[];
        };
        struct {
            struct rc_map map;
            struct rcopt_map map_opt;
            void* map_end[];
        };
        struct {
            struct rc_model model;
            struct rcopt_model model_opt;
            void* model_end[];
        };
        struct {
            struct rc_script script;
            struct rcopt_script script_opt;
            void* script_end[];
        };
        struct {
            struct rc_sound sound;
            struct rcopt_sound sound_opt;
            void* sound_end[];
        };
        struct {
            struct rc_texture texture;
            struct rcopt_texture texture_opt;
            void* texture_end[];
        };
        struct {
            struct rc_values values;
            //struct rcopt_values values_opt;
            void* values_end[];
        };
    };
};

static size_t rcallocsz[RC__COUNT] = {
    offsetof(struct resource, config_end),
    offsetof(struct resource, font_end),
    offsetof(struct resource, map_end),
    offsetof(struct resource, model_end),
    offsetof(struct resource, script_end),
    offsetof(struct resource, sound_end),
    offsetof(struct resource, texture_end),
    offsetof(struct resource, values_end)
};

static struct {
    void* data;
    uint32_t* occ;
    uint32_t* zref;
    int len;
    int size;
} rcgroups[RC__COUNT] = {
    {.size = 2},
    {.size = 1},
    {.size = 1},
    {.size = 8},
    {.size = 4},
    {.size = 16},
    {.size = 16},
    {.size = 16}
};

static const void* const defaultrcopts[RC__COUNT] = {
    NULL,
    NULL,
    NULL,
    &(struct rcopt_model){0},
    &(struct rcopt_script){0},
    &(struct rcopt_sound){true},
    &(struct rcopt_texture){false, RCOPT_TEXTURE_QLT_HIGH},
    NULL
};

PACKEDENUM rcsource {
    RCSRC_FS
};
struct rcaccess {
    enum rcsource src;
    const char* ext;
    union {
        struct {
            char* path;
        } fs;
    };
};

static struct {
    struct modinfo* data;
    uint16_t len;
    uint16_t size;
    #ifndef PSRC_NOMT
    struct accesslock lock;
    #endif
} mods;

static void* rcmgr_malloc_nolock(size_t);
static void* rcmgr_calloc_nolock(size_t, size_t);
static void* rcmgr_realloc_nolock(void*, size_t);

static char* rcIdToPath(const char* id, bool allownative, enum rcprefix* p) {
    struct charbuf cb;
    cb_init(&cb, 128);
    {
        const char* tmp = id;
        char c;
        while ((c = *tmp)) {
            ++tmp;
            if (c == ':') {
                cb_nullterm(&cb);
                switch (*cb.data) {
                    case 0: *p = RCPREFIX_SELF; goto match;
                    case 's': if (!strcmp(cb.data + 1, "elf")) {*p = RCPREFIX_SELF; goto match;} break;
                    case 'i': if (!strcmp(cb.data + 1, "nternal")) {*p = RCPREFIX_INTERNAL; goto match;} break;
                    case 'g': if (!strcmp(cb.data + 1, "ame")) {*p = RCPREFIX_GAME; goto match;} break;
                    case 'u': if (!strcmp(cb.data + 1, "ser")) {*p = RCPREFIX_USER; goto match;} break;
                    case 'n': if (allownative && !strcmp(cb.data + 1, "ative")) {*p = RCPREFIX_NATIVE; goto nativematch;} break;
                    default: break;
                }
                plog(LL_ERROR, "Unknown prefix '%s' in resource identifier '%s'", cb.data, id);
                cb_dump(&cb);
                return NULL;
                match:;
                cb_clear(&cb);
                cb_addstr(&cb, tmp);
                goto foundprefix;
                nativematch:;
                cb_clear(&cb);
                return strpath(tmp);
            } else {
                cb_add(&cb, c);
            }
        }
    }
    *p = RCPREFIX_SELF;
    foundprefix:;
    char* tmp2 = restrictpath(cb_peek(&cb), "/", '/', '_');
    cb_dump(&cb);
    return tmp2;
}

static inline bool cmpRcOpt(enum rctype type, struct resource* rc, const void* opt) {
    switch (type) {
        case RC_MODEL: {
            if (((const struct rcopt_model*)opt)->flags != rc->model_opt.flags) return false;
        } return true;
        case RC_TEXTURE: {
            if (((const struct rcopt_texture*)opt)->needsalpha != rc->texture_opt.needsalpha) return false;
            if (((const struct rcopt_texture*)opt)->quality != rc->texture_opt.quality) return false;
        } return true;
        default: return true;
    }
    //return true;
}
static struct resource* findRc(enum rctype type, enum rcprefix prefix, const char* path, uint32_t pathcrc, const void* opt) {
    int e = rcgroups[type].len / 32;
    for (int i = 0; i < e; ++i) {
        register uint32_t occ = rcgroups[type].occ[i];
        if (!occ) continue;
        int j = 0;
        while (1) {
            if (occ & 1) {
                struct resource* rc = (void*)((char*)rcgroups[type].data + (i * 32 + j) * rcallocsz[type]);
                if (rc->header.prefix == prefix && rc->header.pathcrc == pathcrc && !strcmp(rc->header.path, path) && cmpRcOpt(type, rc, opt)) return rc;
            }
            if (j == 31) break;
            ++j;
            occ >>= 1;
        }
    }
    int e2 = rcgroups[type].len % 32;
    if (!e2) return NULL;
    register uint32_t occ = rcgroups[type].occ[e];
    if (!occ) return NULL;
    int i = 0;
    while (1) {
        if (occ & 1) {
            struct resource* rc = (void*)((char*)rcgroups[type].data + (e * 32 + i) * rcallocsz[type]);
            if (rc->header.prefix == prefix && rc->header.pathcrc == pathcrc && !strcmp(rc->header.path, path) && cmpRcOpt(type, rc, opt)) return rc;
        }
        if (i == e2) break;
        ++i;
        occ >>= 1;
    }
    return NULL;
}

static bool getRcAcc(enum rctype type, enum rcprefix prefix, const char* path, uint32_t pathcrc, struct rcaccess* acc) {
    return false;
}
static bool dsFromRcAcc(struct rcaccess* acc, struct datastream* ds) {
    switch (acc->src) {
        case RCSRC_FS:
            return ds_openfile(acc->fs.path, 0, ds);
    }
    //return false;
}
static void delRcAcc(struct rcaccess* acc) {
    (void)acc;
}

static struct resource* newRc(enum rctype type) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&rclock);
    #endif
    {
        int e = rcgroups[type].len / 32;
        for (int i = 0; i < e; ++i) {
            register uint32_t unocc = ~rcgroups[type].occ[i];
            if (!unocc) continue;
            int j = 0;
            while (1) {
                if (unocc & 1) {
                    int index = i * 32 + j;
                    rcgroups[type].occ[index / 32] |= 1 << (index % 32);
                    rcgroups[type].zref[index / 32] &= ~(1 << (index % 32));
                    struct resource* rc = (void*)((char*)rcgroups[type].data + index * rcallocsz[type]);
                    rc->header.type = type;
                    rc->header.refs = 1;
                    rc->header.index = index;
                }
                if (j == 31) break;
                ++j;
                unocc >>= 1;
            }
        }
        int e2 = rcgroups[type].len % 32;
        if (!e2) return NULL;
        register uint32_t unocc = ~rcgroups[type].occ[e];
        if (!unocc) return NULL;
        int i = 0;
        while (1) {
            if (unocc & 1) {
                int index = e * 32 + i;
                rcgroups[type].occ[index / 32] |= 1 << (index % 32);
                rcgroups[type].zref[index / 32] &= ~(1 << (index % 32));
                struct resource* rc = (void*)((char*)rcgroups[type].data + index * rcallocsz[type]);
                rc->header.type = type;
                rc->header.refs = 1;
                rc->header.index = index;
            }
            if (i == e2) break;
            ++i;
            unocc >>= 1;
        }
    }
    if (rcgroups[type].len == rcgroups[type].size) {
        rcgroups[type].size *= 2;
        rcgroups[type].data = rcmgr_realloc_nolock(rcgroups[type].data, rcgroups[type].size * rcallocsz[type]);
        rcgroups[type].occ = rcmgr_realloc_nolock(rcgroups[type].occ, (rcgroups[type].size / 32 + 1) * sizeof(*rcgroups[type].occ));
        rcgroups[type].zref = rcmgr_realloc_nolock(rcgroups[type].zref, (rcgroups[type].size / 32 + 1) * sizeof(*rcgroups[type].zref));
    }
    int i = rcgroups[type].len++;
    rcgroups[type].occ[i / 32] |= 1 << (i % 32);
    rcgroups[type].zref[i / 32] &= ~(1 << (i % 32));
    struct resource* rc = (void*)((char*)rcgroups[type].data + i * rcallocsz[type]);
    rc->header.type = type;
    rc->header.refs = 1;
    rc->header.index = i;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&rclock);
    #endif
}
void* getRc(enum rctype type, const char* id, const void* opt, unsigned flags, struct charbuf* err) {
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG, "Searching for %s at resource identifier '%s'", rctypenames[type], id);
    #endif
    enum rcprefix prefix;
    char* path = rcIdToPath(id, flags & LOADRC_FLAG_ALLOWNATIVE, &prefix);
    if (!path) {
        #if DEBUG(1)
        plog(LL_ERROR | LF_DEBUG, "Resource identifier '%s' is invalid", id);
        #endif
        return NULL;
    }
    if (!*path) {
        free(path);
        #if DEBUG(1)
        plog(LL_ERROR | LF_DEBUG, "Resolved resource path for identifier '%s' is empty", id);
        #endif
        return NULL;
    }
    uint32_t pathcrc = strcrc32(path);
    #ifndef PSRC_NOMT
    acquireReadAccess(&rclock);
    #endif
    struct resource* rc = findRc(type, prefix, path, pathcrc, opt);
    if (rc) {
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG, "Found %s at resource identifier '%s' already loaded", rctypenames[type], id);
        #endif
        free(path);
        #ifndef PSRC_NOMT
        readToWriteAccess(&rclock);
        #endif
        if (!rc->header.refs++) {
            int i = rc->header.index;
            rcgroups[rc->header.type].zref[i / 32] &= ~(1 << (i % 32));
        }
        #ifndef PSRC_NOMT
        releaseWriteAccess(&rclock);
        #endif
        return &rc->data;
    }
    #ifndef PSRC_NOMT
    releaseReadAccess(&rclock);
    #endif
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG, "Loading %s at resource identifier '%s'...", rctypenames[type], id);
    #endif
    struct rcaccess acc;
    if (!getRcAcc(type, prefix, path, pathcrc, &acc)) goto fail;
    if (!opt) opt = defaultrcopts[type];
    switch (type) {
        case RC_CONFIG: {
            struct datastream ds;
            if (!dsFromRcAcc(&acc, &ds)) goto fail;
            rc = newRc(RC_CONFIG);
            cfg_open(&ds, &rc->config.config);
            ds_close(&ds);
        } break;
        #ifndef PSRC_MODULE_SERVER
        case RC_FONT: {
            if (acc.src != RCSRC_FS) goto fail;
            SFT_Font* font = sft_loadfile(acc.fs.path);
            if (!font) goto fail;
            rc = newRc(RC_FONT);
            rc->font.font = font;
        } break;
        #endif
        case RC_MODEL: {
            if (acc.src != RCSRC_FS) goto fail;
            const struct rcopt_model* o = opt;
            struct p3m* m = p3m_loadfile(acc.fs.path, o->flags);
            if (!m) goto fail;
            rc = newRc(RC_MODEL);
            rc->model.model = m;
            rc->model_opt = *o;
        } break;
        case RC_SCRIPT: {
            if (acc.src != RCSRC_FS) goto fail;
            const struct rcopt_script* o = opt;
            struct pb_script s;
            goto fail;
            //if (!pb_compilefile(acc.fs.path, &o->compileropt, &s, err)) goto fail;
            rc = newRc(RC_SCRIPT);
            rc->script.script = s;
            rc->script_opt = *o;
        } break;
        #ifndef PSRC_MODULE_SERVER
        case RC_SOUND: {
            if (acc.src != RCSRC_FS) goto fail;
            const struct rcopt_sound* o = opt;
            if (acc.ext == rcextensions[RC_SOUND][0]) {
                if (o->decodewhole) {
                    stb_vorbis* v = stb_vorbis_open_filename(acc.fs.path, NULL, NULL);
                    if (!v) goto fail;
                    rc = newRc(RC_SOUND);
                    rc->sound.format = RC_SOUND_FRMT_WAV;
                    stb_vorbis_info info = stb_vorbis_get_info(v);
                    int len = stb_vorbis_stream_length_in_samples(v);
                    int ch = (info.channels > 1) + 1;
                    int size = len * ch * sizeof(int16_t);
                    rc->sound.len = len;
                    rc->sound.size = size;
                    rc->sound.data = rcmgr_malloc(size);
                    rc->sound.freq = info.sample_rate;
                    rc->sound.channels = info.channels;
                    rc->sound.is8bit = false;
                    rc->sound.stereo = (info.channels > 1);
                    rc->sound_opt = *o;
                    stb_vorbis_get_samples_short_interleaved(v, ch, (int16_t*)rc->sound.data, len * ch);
                    stb_vorbis_close(v);
                } else {
                    FILE* f = fopen(acc.fs.path, "rb");
                    if (!f) goto fail;
                    fseek(f, 0, SEEK_END);
                    long sz = ftell(f);
                    if (sz <= 0) {fclose(f); goto fail;}
                    uint8_t* data = rcmgr_malloc(sz);
                    fseek(f, 0, SEEK_SET);
                    fread(data, 1, sz, f);
                    fclose(f);
                    stb_vorbis* v = stb_vorbis_open_memory(data, sz, NULL, NULL);
                    if (!v) {free(data); goto fail;}
                    rc = newRc(RC_SOUND);
                    rc->sound.format = RC_SOUND_FRMT_VORBIS;
                    rc->sound.size = sz;
                    rc->sound.data = data;
                    rc->sound.len = stb_vorbis_stream_length_in_samples(v);
                    stb_vorbis_info info = stb_vorbis_get_info(v);
                    rc->sound.freq = info.sample_rate;
                    rc->sound.channels = info.channels;
                    rc->sound.stereo = (info.channels > 1);
                    rc->sound_opt = *o;
                    stb_vorbis_close(v);
                }
            } else if (acc.ext == rcextensions[RC_SOUND][1]) {
                #ifdef PSRC_USEMINIMP3
                mp3dec_ex_t* m = rcmgr_malloc(sizeof(*m));
                if (o->decodewhole) {
                    if (mp3dec_ex_open(m, acc.fs.path, MP3D_SEEK_TO_SAMPLE)) {free(m); goto fail;}
                    rc = newRc(RC_SOUND);
                    rc->sound.format = RC_SOUND_FRMT_WAV;
                    int len = m->samples / m->info.channels;
                    int size = m->samples * sizeof(mp3d_sample_t);
                    rc->sound.len = len;
                    rc->sound.size = size;
                    rc->sound.data = rcmgr_malloc(size);
                    rc->sound.freq = m->info.hz;
                    rc->sound.channels = m->info.channels;
                    rc->sound.is8bit = false;
                    rc->sound.stereo = (m->info.channels > 1);
                    rc->sound_opt = *o;
                    mp3dec_ex_read(m, (mp3d_sample_t*)rc->sound.data, m->samples);
                    mp3dec_ex_close(m);
                } else {
                    FILE* f = fopen(acc.fs.path, "rb");
                    if (!f) goto fail;
                    fseek(f, 0, SEEK_END);
                    long sz = ftell(f);
                    if (sz <= 0) {fclose(f); goto fail;}
                    uint8_t* data = rcmgr_malloc(sz);
                    fseek(f, 0, SEEK_SET);
                    fread(data, 1, sz, f);
                    fclose(f);
                    if (mp3dec_ex_open_buf(m, data, sz, MP3D_SEEK_TO_SAMPLE)) {
                        free(data);
                        free(m);
                        goto fail;
                    }
                    rc = newRc(RC_SOUND);
                    rc->sound.format = RC_SOUND_FRMT_MP3;
                    rc->sound.size = sz;
                    rc->sound.data = data;
                    rc->sound.len = m->samples / m->info.channels;
                    rc->sound.freq = m->info.hz;
                    rc->sound.channels = m->info.channels;
                    rc->sound.stereo = (m->info.channels > 1);
                    rc->sound_opt = *o;
                    mp3dec_ex_close(m);
                    free(data);
                }
                free(m);
                #else
                goto fail;
                #endif
            } else /*if (acc.ext == rcextensions[RC_SOUND][2])*/ {
                SDL_AudioSpec spec;
                uint8_t* data;
                uint32_t sz;
                {
                    SDL_RWops* rwops = SDL_RWFromFile(acc.fs.path, "rb");
                    if (!rwops) goto fail;
                    if (!SDL_LoadWAV_RW(rwops, false, &spec, &data, &sz)) {SDL_RWclose(rwops); goto fail;}
                    SDL_RWclose(rwops);
                }
                SDL_AudioCVT cvt;
                int destfrmt;
                {
                    bool is8bit;
                    #ifndef PSRC_USESDL1
                    is8bit = (SDL_AUDIO_BITSIZE(spec.format) == 8);
                    #else
                    is8bit = (spec.format == AUDIO_U8 || spec.format == AUDIO_S8);
                    #endif
                    destfrmt = (is8bit) ? AUDIO_U8 : AUDIO_S16SYS;
                }
                int ret = SDL_BuildAudioCVT(
                    &cvt,
                    spec.format, spec.channels, spec.freq,
                    destfrmt, (spec.channels > 1) + 1, spec.freq
                );
                if (!ret) {
                    rc = newRc(RC_SOUND);
                    rc->sound.format = RC_SOUND_FRMT_WAV;
                    rc->sound.size = sz;
                    rc->sound.data = data;
                    rc->sound.len = sz / spec.channels / ((destfrmt == AUDIO_S16SYS) + 1);
                    rc->sound.freq = spec.freq;
                    rc->sound.channels = spec.channels;
                    rc->sound.is8bit = (destfrmt == AUDIO_U8);
                    rc->sound.stereo = (spec.channels > 1);
                    rc->sound.sdlfree = 1;
                    rc->sound_opt = *o;
                } else if (ret == 1) {
                    cvt.len = sz;
                    data = SDL_realloc(data, cvt.len * cvt.len_mult);
                    cvt.buf = data;
                    if (SDL_ConvertAudio(&cvt)) {
                        SDL_FreeWAV(data);
                    } else {
                        data = SDL_realloc(data, cvt.len_cvt);
                        rc = newRc(RC_SOUND);
                        rc->sound.format = RC_SOUND_FRMT_WAV;
                        rc->sound.size = cvt.len_cvt;
                        rc->sound.data = data;
                        rc->sound.len = cvt.len_cvt / ((spec.channels > 1) + 1) / ((destfrmt == AUDIO_S16SYS) + 1);
                        rc->sound.freq = spec.freq;
                        rc->sound.channels = (spec.channels > 1) + 1;
                        rc->sound.is8bit = (destfrmt == AUDIO_U8);
                        rc->sound.stereo = (spec.channels > 1);
                        rc->sound.sdlfree = 1;
                        rc->sound_opt = *o;
                    }
                } else {
                    SDL_FreeWAV(data);
                    goto fail;
                }
            }
        } break;
        case RC_TEXTURE: {
            if (acc.src != RCSRC_FS) goto fail;
            const struct rcopt_texture* o = opt;
            if (acc.ext == rcextensions[RC_TEXTURE][0]) {
                unsigned r, c;
                uint8_t* data = ptf_loadfile(acc.fs.path, &r, &c);
                if (!data) goto fail;
                if (o->needsalpha && c == 3) {
                    c = 4;
                    data = rcmgr_realloc(data, r * r * 4);
                    for (int y = r - 1; y >= 0; --y) {
                        int b1 = y * r * 4, b2 = y * r * 3;
                        for (int x = r - 1; x >= 0; --x) {
                            data[b1 + x * 4 + 3] = 255;
                            data[b1 + x * 4 + 2] = data[b2 + x * 3 + 2];
                            data[b1 + x * 4 + 1] = data[b2 + x * 3 + 1];
                            data[b1 + x * 4] = data[b2 + x * 3];
                        }
                    }
                }
                if (o->quality != RCOPT_TEXTURE_QLT_HIGH) {
                    int r2 = r;
                    switch ((uint8_t)o->quality) {
                        case RCOPT_TEXTURE_QLT_MED: r2 /= 2; break;
                        case RCOPT_TEXTURE_QLT_LOW: r2 /= 4; break;
                    }
                    if (r2 < 1) r2 = 1;
                    unsigned char* data2 = rcmgr_malloc(r * r * c);
                    int status = stbir_resize_uint8_generic(
                        data, r, r, 0,
                        data2, r2, r2, 0,
                        c, -1, 0,
                        STBIR_EDGE_CLAMP, STBIR_FILTER_TRIANGLE, STBIR_COLORSPACE_LINEAR,
                        NULL
                    );
                    if (status) {
                        free(data);
                        r = r2;
                        data = data2;
                    } else {
                        free(data2);
                    }
                }
                rc = newRc(RC_TEXTURE);
                rc->texture.width = r;
                rc->texture.height = r;
                rc->texture.channels = c;
                rc->texture.data = data;
                rc->texture_opt = *o;
            } else {
                int w, h, c;
                if (stbi_info(acc.fs.path, &w, &h, &c)) goto fail;
                if (o->needsalpha) {
                    c = 4;
                } else {
                    if (c < 3) c += 2;
                }
                int c2;
                unsigned char* data = stbi_load(acc.fs.path, &w, &h, &c2, c);
                if (!data) goto fail;
                if (o->quality != RCOPT_TEXTURE_QLT_HIGH) {
                    int w2 = w, h2 = h;
                    switch ((uint8_t)o->quality) {
                        case RCOPT_TEXTURE_QLT_MED:
                            w2 /= 2;
                            h2 /= 2;
                            break;
                        case RCOPT_TEXTURE_QLT_LOW:
                            w2 /= 4;
                            h2 /= 4;
                            break;
                    }
                    if (w2 < 1) w2 = 1;
                    if (h2 < 1) h2 = 1;
                    unsigned char* data2 = rcmgr_malloc(w * h * c);
                    int status = stbir_resize_uint8_generic(
                        data, w, h, 0,
                        data2, w2, h2, 0,
                        c, -1, 0,
                        STBIR_EDGE_CLAMP, STBIR_FILTER_TRIANGLE, STBIR_COLORSPACE_LINEAR,
                        NULL
                    );
                    if (status) {
                        free(data);
                        w = w2;
                        h = h2;
                        data = data2;
                    } else {
                        free(data2);
                    }
                }
                rc = newRc(RC_TEXTURE);
                rc->texture.width = w;
                rc->texture.height = h;
                rc->texture.channels = c;
                rc->texture.data = data;
                rc->texture_opt = *o;
            }
        } break;
        #endif
        case RC_VALUES: {
            struct datastream ds;
            if (!dsFromRcAcc(&acc, &ds)) goto fail;
            rc = newRc(RC_VALUES);
            cfg_open(&ds, &rc->values.values);
            ds_close(&ds);
        } break;
        default: goto fail;
    }
    delRcAcc(&acc);
    return rc;
    fail:;
    free(path);
    plog(LL_ERROR, "Failed to load %s at resource identifier '%s'", rctypenames[type], id);
    return NULL;
}

void lockRc(void* rp) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&rclock);
    #endif
    struct resource* rc = (void*)((char*)rp - offsetof(struct resource, data));
    if (!rc->header.refs++) {
        int i = rc->header.index;
        rcgroups[rc->header.type].zref[i / 32] &= ~(1 << (i % 32));
    }
    #ifndef PSRC_NOMT
    releaseWriteAccess(&rclock);
    #endif
}

static inline void freeRcHeader(struct rcheader* rh) {
    free(rh->path);
}

static void freeRcData(enum rctype type, struct resource* rc) {
    switch (type) {
        case RC_MODEL: {
            p3m_free(rc->model.model);
        } break;
        case RC_SCRIPT: {
            //pb_deletescript(&rc->script.script);
        } break;
        case RC_SOUND: {
            if (rc->sound.format == RC_SOUND_FRMT_WAV && rc->sound.sdlfree) SDL_FreeWAV(rc->sound.data);
            else free(rc->sound.data);
        } break;
        case RC_TEXTURE: {
            free(rc->texture.data);
        } break;
        default: break;
    }
}

static inline void freeRc(enum rctype type, struct resource* rc) {
    rcgroups[type].occ[rc->header.index / 32] &= ~(1 << (rc->header.index % 32));
    freeRcData(type, rc);
    freeRcHeader(&rc->header);
}

void rlsRc(void* rp, bool force) {
    struct resource* rc = (void*)((char*)rp - offsetof(struct resource, data));
    #ifndef PSRC_NOMT
    acquireWriteAccess(&rclock);
    #endif
    --rc->header.refs;
    if (force) rc->header.forcefree = 1;
    if (!rc->header.refs) {
        enum rctype type = rc->header.type;
        if (rc->header.forcefree) freeRc(type, rc);
        else rcgroups[type].zref[rc->header.index / 32] |= 1 << (rc->header.index % 32);
    }
    #ifndef PSRC_NOMT
    releaseWriteAccess(&rclock);
    #endif
}

static ALWAYSINLINE void freeModListEntry(struct modinfo* m) {
    free(m->path);
    free(m->dir);
    free(m->name);
    free(m->author);
    free(m->desc);
}

static inline int loadMods_add(struct charbuf* cb) {
    {
        cb_nullterm(cb);
        int tmp = isFile(cb->data);
        if (tmp < 0) {
            #if DEBUG(1)
            plog(LL_WARN | LF_DEBUG, "'%s' does not exist", cb->data);
            #endif
            return -1;
        }
        if (tmp >= 1) {
            plog(LL_WARN, "'%s' is a file", cb->data);
            return -1;
        }
    }
    cb_add(cb, PATHSEP);
    cb_addstr(cb, "mod.cfg");
    cb_nullterm(cb);
    cb->len -= 8;
    struct cfg cfg;
    {
        struct datastream ds;
        if (!ds_openfile(cb->data, 0, &ds)) {
            plog(LL_WARN, "No mod.cfg in '%s'", cb_peek(cb));
            return -1;
        }
        cfg_open(&ds, &cfg);
        ds_close(&ds);
    }
    if (mods.len == mods.size) {
        mods.size = mods.size * 3 / 2;
        mods.data = rcmgr_realloc(mods.data, mods.size * sizeof(*mods.data));
    }
    mods.data[mods.len].name = cfg_getvar(&cfg, NULL, "name");
    mods.data[mods.len].author = cfg_getvar(&cfg, NULL, "author");
    mods.data[mods.len].desc = cfg_getvar(&cfg, NULL, "desc");
    char* tmp = cfg_getvar(&cfg, NULL, "version");
    if (tmp) {
        if (!strtover(tmp, &mods.data[mods.len].version)) mods.data[mods.len].version = MKVER_8_16_8(1, 0, 0);
        free(tmp);
    } else {
        mods.data[mods.len].version = MKVER_8_16_8(1, 0, 0);
    }
    cfg_close(&cfg);
    return mods.len++;
}
void loadMods(const char* const* l, int ct) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&mods.lock);
    #endif
    for (int i = 0; i < mods.len; ++i) {
        freeModListEntry(&mods.data[i]);
    }
    mods.len = 0;
    clRcCache();
    if (ct < 0) {
        do {
            ++ct;
        } while (l[ct]);
    }
    if (!ct) {
        mods.size = 0;
        free(mods.data);
        return;
    }
    if (!mods.size) {
        mods.size = 4;
        mods.data = rcmgr_malloc(mods.size * sizeof(*mods.data));
    }
    struct charbuf cb;
    cb_init(&cb, 256);
    #ifndef PSRC_MODULE_SERVER
    if (dirs[DIR_USERMODS]) {
        for (int i = 0; i < ct; ++i) {
            if (!*l[i]) continue;
            char* n = sanfilename(l[i], '_');
            cb_addstr(&cb, dirs[DIR_USERMODS]);
            cb_add(&cb, PATHSEP);
            cb_addstr(&cb, n);
            int mi = loadMods_add(&cb);
            if (mi < 0) {
                cb_clear(&cb);
                cb_addstr(&cb, dirs[DIR_MODS]);
                cb_add(&cb, PATHSEP);
                cb_addstr(&cb, n);
                mi = loadMods_add(&cb);
                if (mi < 0) {
                    plog(LL_ERROR, "Failed to load mod '%s'", n);
                    free(n);
                    cb_clear(&cb);
                    continue;
                }
            }
            mods.data[mi].path = cb_reinit(&cb, 256);
            mods.data[mi].dir = n;
            if (!mods.data[mi].name) mods.data[mi].name = strdup(l[i]);
        }
    } else {
    #endif
        for (int i = 0; i < ct; ++i) {
            if (!*l[i]) continue;
            char* n = sanfilename(l[i], '_');
            cb_addstr(&cb, dirs[DIR_MODS]);
            cb_add(&cb, PATHSEP);
            cb_addstr(&cb, n);
            int mi = loadMods_add(&cb);
            if (mi < 0) {
                plog(LL_ERROR, "Failed to load mod '%s'", n);
                free(n);
                cb_clear(&cb);
                continue;
            }
            mods.data[mi].path = cb_reinit(&cb, 256);
            mods.data[mi].dir = n;
            if (!mods.data[mi].name) mods.data[mi].name = strdup(l[i]);
        }
    #ifndef PSRC_MODULE_SERVER
    }
    #endif
    cb_dump(&cb);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&mods.lock);
    #endif
}

void freeModList(struct modinfo* m) {
    free(m->path);
    free(m);
}
struct modinfo* queryMods(int* len) {
    #ifndef PSRC_NOMT
    acquireReadAccess(&mods.lock);
    #endif
    if (!mods.len) {
        #ifndef PSRC_NOMT
        releaseReadAccess(&mods.lock);
        #endif
        *len = 0;
        return NULL;
    }
    if (len) *len = mods.len;
    struct modinfo* data = rcmgr_malloc(mods.len * sizeof(*data));
    struct charbuf cb;
    cb_init(&cb, 256);
    for (uint16_t i = 0; i < mods.len; ++i) {
        data[i].path = (char*)(uintptr_t)cb.len; cb_addstr(&cb, mods.data[i].path); cb_add(&cb, 0);
        data[i].dir = (char*)(uintptr_t)cb.len; cb_addstr(&cb, mods.data[i].dir); cb_add(&cb, 0);
        data[i].name = (char*)(uintptr_t)cb.len; cb_addstr(&cb, mods.data[i].name); cb_add(&cb, 0);
        if (mods.data[i].author) {data[i].author = (char*)(uintptr_t)cb.len; cb_addstr(&cb, mods.data[i].author); cb_add(&cb, 0);}
        else data[i].author = NULL;
        if (mods.data[i].desc) {data[i].desc = (char*)(uintptr_t)cb.len; cb_addstr(&cb, mods.data[i].desc); cb_add(&cb, 0);}
        else data[i].desc = NULL;
        data[i].version = mods.data[i].version;
    }
    --cb.len;
    cb_finalize(&cb);
    for (uint16_t i = 0; i < mods.len; ++i) {
        data[i].path += (uintptr_t)cb.data;
        data[i].dir += (uintptr_t)cb.data;
        data[i].name += (uintptr_t)cb.data;
        if (data[i].author) data[i].author += (uintptr_t)cb.data;
        if (data[i].desc) data[i].desc += (uintptr_t)cb.data;
    }
    #ifndef PSRC_NOMT
    releaseReadAccess(&mods.lock);
    #endif
    return data;
}

static void gcRcs_internal(bool ag) {
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG, "Running resource garbage collector...%s", (ag) ? " (aggressive)" : "");
    #endif
    for (int g = 0; g < RC__COUNT; ++g) {
        int e = rcgroups[g].len / 32;
        for (int i = 0; i < e; ++i) {
            register uint32_t occ = rcgroups[g].occ[i];
            if (!occ) continue;
            register uint32_t zref = rcgroups[g].zref[i];
            if (!zref) continue;
            int j = 0;
            while (1) {
                if (occ & 1 && zref & 1) {
                    struct resource* rc = (void*)((char*)rcgroups[g].data + (i * 32 + j) * rcallocsz[g]);
                    if (rctick - rc->header.zreftick >= 5) freeRc(g, rc);
                }
                if (j == 31) break;
                ++j;
                occ >>= 1;
                zref >>= 1;
            }
        }
        int e2 = rcgroups[g].len % 32;
        if (!e2) continue;
        register uint32_t occ = rcgroups[g].occ[e];
        if (!occ) continue;
        register uint32_t zref = rcgroups[g].zref[e];
        if (!zref) continue;
        int i = 0;
        while (1) {
            if (occ & 1 && zref & 1) {
                struct resource* rc = (void*)((char*)rcgroups[g].data + (e * 32 + i) * rcallocsz[g]);
                if (rctick - rc->header.zreftick >= 5) freeRc(g, rc);
            }
            if (i == e2) break;
            ++i;
            occ >>= 1;
            zref >>= 1;
        }
    }
}
static void gcRcs(bool ag) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&rclock);
    #endif
    gcRcs_internal(ag);
    #ifndef PSRC_NOMT
    releaseWriteAccess(&rclock);
    #endif
}

void* rcmgr_malloc(size_t size) {
    void* tmp = malloc(size);
    if (tmp) return tmp;
    gcRcs(true);
    return malloc(size);
}
void* rcmgr_calloc(size_t nmemb, size_t size) {
    void* tmp = calloc(nmemb, size);
    if (tmp) return tmp;
    gcRcs(true);
    return calloc(nmemb, size);
}
void* rcmgr_realloc(void* ptr, size_t size) {
    void* tmp = realloc(ptr, size);
    if (tmp) return tmp;
    else if (!size) return NULL;
    gcRcs(true);
    return realloc(ptr, size);
}
static void* rcmgr_malloc_nolock(size_t size) {
    void* tmp = malloc(size);
    if (tmp) return tmp;
    gcRcs_internal(true);
    return malloc(size);
}
static void* rcmgr_calloc_nolock(size_t nmemb, size_t size) {
    void* tmp = calloc(nmemb, size);
    if (tmp) return tmp;
    gcRcs_internal(true);
    return calloc(nmemb, size);
}
static void* rcmgr_realloc_nolock(void* ptr, size_t size) {
    void* tmp = realloc(ptr, size);
    if (tmp) return tmp;
    else if (!size) return NULL;
    gcRcs_internal(true);
    return realloc(ptr, size);
}

void clRcCache(void) {
    //lscDelAll();
    gcRcs(true);
}

static uint64_t lasttick;
void runRcMgr(uint64_t t) {
    {
        uint64_t tmp = t - lasttick;
        if (tmp < 1000000) return;
        lasttick += tmp / 1000000 * 1000000;
    }
    static int counter = 0;
    ++counter;
    #ifndef PSRC_NOMT
    acquireWriteAccess(&rclock);
    #endif
    if (counter == 5) {
        counter = 0;
        void* ptr = malloc(262144);
        bool ag = (ptr == NULL);
        free(ptr);
        gcRcs_internal(ag);
    } else {
        gcRcs_internal(false);
    }
    ++rctick;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&rclock);
    #endif
}

bool initRcMgr(void) {
    #ifndef PSRC_NOMT
    if (!createAccessLock(&rclock)) return false;
    if (!createAccessLock(&mods.lock)) return false;
    //if (!createAccessLock(&lscache.lock)) return false;
    #endif

    for (int i = 0; i < RC__COUNT; ++i) {
        rcgroups[i].data = malloc(rcgroups[i].size * sizeof(*rcgroups[i].data));
    }

    char* tmp = cfg_getvar(&config, NULL, "lscache.size");
    if (tmp) {
        //lscache.size = atoi(tmp);
        //if (lscache.size < 1) lscache.size = 1;
    } else {
        #if PLATFORM != PLAT_NXDK && (PLATFLAGS & (PLATFLAG_UNIXLIKE | PLATFLAG_WINDOWSLIKE))
        //lscache.size = 32;
        #else
        //lscache.size = 12;
        #endif
    }

    lasttick = altutime();

    return true;
}

void quitRcMgr(void) {
    // TODO: del all rc

    //lscDelAll();

    #ifndef PSRC_NOMT
    destroyAccessLock(&rclock);
    destroyAccessLock(&mods.lock);
    //destroyAccessLock(&lscache.lock);
    #endif
}
