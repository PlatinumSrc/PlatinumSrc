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

static const char* const rcprefixnames[RCPREFIX__COUNT] = {
    "internal",
    "game",
    "user",
    "native"
};

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
            void* config_end;
        };
        struct {
            struct rc_font font;
            //struct rcopt_font font_opt;
            void* font_end;
        };
        struct {
            struct rc_map map;
            struct rcopt_map map_opt;
            void* map_end;
        };
        struct {
            struct rc_model model;
            struct rcopt_model model_opt;
            void* model_end;
        };
        struct {
            struct rc_script script;
            struct rcopt_script script_opt;
            void* script_end;
        };
        struct {
            struct rc_sound sound;
            struct rcopt_sound sound_opt;
            void* sound_end;
        };
        struct {
            struct rc_texture texture;
            struct rcopt_texture texture_opt;
            void* texture_end;
        };
        struct {
            struct rc_values values;
            //struct rcopt_values values_opt;
            void* values_end;
        };
    };
};

static const size_t rcallocsz[RC__COUNT] = {
    offsetof(struct resource, config_end),
    offsetof(struct resource, font_end),
    offsetof(struct resource, map_end),
    offsetof(struct resource, model_end),
    offsetof(struct resource, script_end),
    offsetof(struct resource, sound_end),
    offsetof(struct resource, texture_end),
    offsetof(struct resource, values_end)
};

struct rcgrouppage {
    void* data;
    uint16_t occ;
    uint16_t zref;
};
static struct {
    struct rcgrouppage* pages;
    unsigned pagect;
    unsigned zrefct;
} rcgroups[RC__COUNT];

static const void* const defaultrcopts[RC__COUNT] = {
    NULL,
    NULL,
    &(struct rcopt_map){0},
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
    int len;
    int size;
    #ifndef PSRC_NOMT
    struct accesslock lock;
    #endif
} mods;

struct lscache_dir {
    enum rcprefix prefix;
    char* path;
    uint32_t pathcrc;
    int prev;
    int next;
    struct rcls l;
};
static struct {
    struct lscache_dir* data;
    int size;
    int head;
    int tail;
    #ifndef PSRC_NOMT
    struct accesslock lock;
    #endif
} lscache;

static void* rcmgr_malloc_nolock(size_t);
#if 0
static void* rcmgr_calloc_nolock(size_t, size_t);
static void* rcmgr_realloc_nolock(void*, size_t);
#endif

#if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
static const uint8_t* lscFind(enum rctype t, enum rcprefix p, const char* path);
#endif

static void lsRc_dup(const struct rcls* in, struct rcls* out) {
    char* onames = rcmgr_malloc(in->nameslen);
    char* inames = in->names;
    out->names = onames;
    memcpy(onames, inames, in->nameslen);
    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    uint8_t* odirbmp = rcmgr_malloc(in->dirbmplen);
    uint8_t* idirbmp = in->dirbmp;
    out->dirbmp = odirbmp;
    memcpy(odirbmp, idirbmp, in->dirbmplen);
    #endif
    for (int ri = 0; ri < RC__DIR + 1; ++ri) {
        int ct = in->count[ri];
        out->count[ri] = ct;
        if (ct) {
            struct rcls_file* ofl = rcmgr_malloc(ct * sizeof(*ofl));
            out->files[ri] = ofl;
            struct rcls_file* ifl = in->files[ri];
            for (int i = 0; i < ct; ++i) {
                ofl[i] = ifl[i];
                ofl[i].name -= (uintptr_t)inames;
                ofl[i].name += (uintptr_t)onames;
                #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
                ofl[i].dirbits -= (uintptr_t)idirbmp;
                ofl[i].dirbits += (uintptr_t)odirbmp;
                #endif
            }
        } else {
            out->files[ri] = NULL;
        }
    }
}
static bool lsRc_norslv(enum rcprefix p, const char* r, struct rcls* l) {
    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    const uint8_t* dirbits;
    if (*r) {
        if (!(dirbits = lscFind(RC__DIR, p, r))) return false;
    } else {
        dirbits = NULL;
    }
    #endif
    struct {
        int len;
        int size;
    } ld[RC__DIR + 1] = {0};
    struct charbuf cb;
    cb_init(&cb, 256);
    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    struct membuf dirbmp;
    mb_init(&dirbmp, 8 * (mods.len + 1));
    #endif
    bool ret = false;
    int dirind = 0;
    char* dir;
    while (1) {
        struct lsstate s;
        if (p != RCPREFIX_NATIVE) {
            switch (p) {
                default:
                case RCPREFIX_INTERNAL:
                    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
                    if (dirbits && !((dirbits[dirind / 8] >> (dirind % 8)) & 1)) {if (dirind > mods.len) goto longbreak; else goto longcont;}
                    #endif
                    if (dirind < mods.len) dir = mkpath(mods.data[dirind].path, "internal" PATHSEPSTR "resources", r, NULL);
                    else if (dirind == mods.len) dir = mkpath(dirs[DIR_INTERNALRC], r, NULL);
                    else goto longbreak;
                    break;
                case RCPREFIX_GAME:
                    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
                    if (dirbits && !((dirbits[dirind / 8] >> (dirind % 8)) & 1)) {if (dirind > mods.len) goto longbreak; else goto longcont;}
                    #endif
                    if (dirind < mods.len) dir = mkpath(mods.data[dirind].path, "games", r, NULL);
                    else if (dirind == mods.len) dir = mkpath(dirs[DIR_GAMES], r, NULL);
                    else goto longbreak;
                    break;
                case RCPREFIX_USER:
                    #ifndef PSRC_MODULE_SERVER
                    if (dirind) goto longbreak;
                    else dir = mkpath(dirs[DIR_USERRC], r, NULL);
                    #else
                    goto longbreak;
                    #endif
                    break;
            }
            bool status = startls(dir, &s);
            free(dir);
            if (!status) goto longcont;
        } else {
            if (!startls(r, &s)) goto longcont;
        }
        const char* n;
        const char* ln;
        while (getls(&s, &n, &ln)) {
            int ind;
            long unsigned ol = cb.len;
            {
                int isfile = isFile(ln);
                if (isfile == 1) {
                    cb_addstr(&cb, n);
                    cb_add(&cb, 0);
                    again:;
                    --cb.len;
                    if (cb.len == ol) continue;
                    if (cb.data[cb.len] == '.') {
                        char* tmp = &cb.data[cb.len + 1];
                        for (int j = 0; j < RC__COUNT; ++j) {
                            const char* const* exts = rcextensions[j];
                            do {
                                if (!strcmp(tmp, *exts)) {
                                    ind = j;
                                    goto foundext;
                                }
                                ++exts;
                            } while (*exts);
                        }
                        cb.len = ol;
                        continue;
                        foundext:;
                        cb_add(&cb, 0);
                    } else {
                        goto again;
                    }
                } else if (!isfile) {
                    cb_addstr(&cb, n);
                    cb_add(&cb, 0);
                    ind = RC__DIR;
                } else {
                    continue;
                }
            }
            int fi;
            char* tmp = &cb.data[ol];
            uint32_t crc = strcrc32(tmp);
            if (ret) {
                int i = 0;
                again2:;
                if (i < ld[ind].len) {
                    struct rcls_file* f = &l->files[ind][i];
                    if (crc == f->namecrc && !strcmp(tmp, &cb.data[(uintptr_t)f->name])) {
                        #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
                        ((uint8_t*)dirbmp.data)[(uintptr_t)f->dirbits + dirind / 8] |= 1 << (dirind % 8);
                        #endif
                        cb.len = ol;
                        continue;
                    }
                    ++i;
                    goto again2;
                }
            }
            fi = ld[ind].len++;
            if (!fi) {
                ld[ind].size = 4;
                l->files[ind] = rcmgr_malloc(ld[ind].size * sizeof(**l->files));
            } else if (ld[ind].len == ld[ind].size) {
                ld[ind].size = ld[ind].size * 3 / 2;
                l->files[ind] = rcmgr_realloc(l->files[ind], ld[ind].size * sizeof(**l->files));
            }
            struct rcls_file* f = &l->files[ind][fi];
            f->name = (char*)(uintptr_t)ol;
            f->namecrc = crc;
            #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
            f->dirbits = (uint8_t*)(uintptr_t)dirbmp.len;
            mb_putz(&dirbmp, (mods.len + 8) / 8);
            ((uint8_t*)dirbmp.data)[(uintptr_t)f->dirbits + dirind / 8] |= 1 << (dirind % 8);
            #endif
            //printf("[%d]: [%d]: {%s}\n", fi, ind, &cb.data[ol]);
        }
        ret = true;
        endls(&s);
        longcont:;
        ++dirind;
    }
    longbreak:;
    if (ret) {
        cb.data = rcmgr_realloc(cb.data, cb.len);
        l->names = cb.data;
        l->nameslen = cb.len;
        #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        dirbmp.data = rcmgr_realloc(dirbmp.data, dirbmp.len);
        l->dirbmp = dirbmp.data;
        l->dirbmplen = dirbmp.len;
        #endif
        for (int ri = 0; ri < RC__DIR + 1; ++ri) {
            int ct = ld[ri].len;
            l->count[ri] = ct;
            if (ct) {
                struct rcls_file* fl = rcmgr_realloc(l->files[ri], ct * sizeof(**l->files));
                l->files[ri] = fl;
                for (int i = 0; i < ct; ++i) {
                    fl[i].name += (uintptr_t)cb.data;
                    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
                    fl[i].dirbits += (uintptr_t)dirbmp.data;
                    #endif
                }
            } else {
                l->files[ri] = NULL;
            }
        }
    } else {
        #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        mb_dump(&dirbmp);
        #endif
        cb_dump(&cb);
    }
    return ret;
}
void freeRcLs(struct rcls* l) {
    free(l->names);
    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    free(l->dirbmp);
    #endif
    for (int i = 0; i < RC__DIR + 1; ++i) {
        free(l->files[i]);
    }
}

static inline void lscFreeDir(struct lscache_dir* d) {
    free(d->path);
    freeRcLs(&d->l);
}
static inline void lscMoveToFront(int di, bool exists) {
    #if 0
    puts("STATE BEFORE");
    for (int i = 0; i < lscache.size; ++i) {
        printf("[%d]: [%d,%d]\n", i, lscache.data[i].prev, lscache.data[i].next);
    }
    printf("MOVE [%d:%d] TO FRONT [%d,%d]", di, exists, lscache.head, lscache.tail);
    #endif
    if (di != lscache.head) {
        struct lscache_dir* d = &lscache.data[di];
        d->next = lscache.head;
        lscache.data[lscache.head].prev = di;
        lscache.head = di;
        if (di == lscache.tail) {
            lscache.tail = d->prev;
        } else if (exists) {
            lscache.data[d->prev].next = d->next;
            lscache.data[d->next].prev = d->prev;
        }
    }
    #if 0
    printf(" -> [%d,%d]\n", lscache.head, lscache.tail); 
    puts("STATE AFTER");
    for (int i = 0; i < lscache.size; ++i) {
        printf("[%d]: [%d,%d]\n", i, lscache.data[i].prev, lscache.data[i].next);
    }
    #endif
}
static int lscAdd(void) {
    if (!lscache.data) {
        lscache.data = rcmgr_malloc(lscache.size * sizeof(*lscache.data));
        for (int i = 0; i < lscache.size; ++i) {
            lscache.data[i].path = NULL;
        }
        lscache.head = 0;
        lscache.tail = 0;
        return 0;
    }
    for (int i = 0; i < lscache.size; ++i) {
        if (!lscache.data[i].path) {
            lscMoveToFront(i, false);
            return i;
        }
    }
    int i = lscache.tail;
    lscFreeDir(&lscache.data[i]);
    lscMoveToFront(i, true);
    return i;
}
static void lscDelAll(void) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&lscache.lock);
    #endif
    if (lscache.data) {
        for (int i = 0; i < lscache.size; ++i) {
            if (lscache.data[i].path) lscFreeDir(&lscache.data[i]);
        }
        free(lscache.data);
        lscache.data = NULL;
    }
    #ifndef PSRC_NOMT
    releaseWriteAccess(&lscache.lock);
    #endif
}
#if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
static struct rcls* lscGet_len(enum rcprefix p, const char* path, uint32_t crc, int len) {
    if (!lscache.data) return NULL;
    int di = lscache.head;
    while (1) {
        struct lscache_dir* d = &lscache.data[di];
        if (d->prefix == p && d->pathcrc == crc && !strncmp(d->path, path, len)) {
            #ifndef PSRC_NOMT
            readToWriteAccess(&lscache.lock);
            #endif
            lscMoveToFront(di, true);
            #ifndef PSRC_NOMT
            writeToReadAccess(&lscache.lock);
            #endif
            return &d->l;
        }
        if (di == lscache.tail) break;
        di = d->next;
    }
    return NULL;
}
#endif
static struct rcls* lscGet(enum rcprefix p, const char* path, uint32_t crc) {
    if (!lscache.data) return NULL;
    int di = lscache.head;
    while (1) {
        struct lscache_dir* d = &lscache.data[di];
        if (d->prefix == p && d->pathcrc == crc && !strcmp(d->path, path)) {
            #ifndef PSRC_NOMT
            readToWriteAccess(&lscache.lock);
            #endif
            lscMoveToFront(di, true);
            #ifndef PSRC_NOMT
            writeToReadAccess(&lscache.lock);
            #endif
            return &d->l;
        }
        if (di == lscache.tail) break;
        di = d->next;
    }
    return NULL;
}
#if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
static const uint8_t* lscFind(enum rctype t, enum rcprefix p, const char* path) {
    //printf("LSCFIND: [%d] [%d] {%s}\n", t, p, path);
    int spos = 0;
    const char* f = path;
    {
        int i = 0;
        while (1) {
            char c = path[i];
            if (c == '/') {spos = i; f = &path[i + 1];}
            else if (!c) break;
            ++i;
        }
    }
    struct rcls* l = lscGet_len(p, path, crc32(path, spos), spos);
    if (!l) {
        struct rcls tl;
        char* tmp = rcmgr_malloc(spos + 1);
        memcpy(tmp, path, spos);
        tmp[spos] = 0;
        if (lsRc_norslv(p, tmp, &tl)) {
            #ifndef PSRC_NOMT
            readToWriteAccess(&lscache.lock);
            #endif
            int i = lscAdd();
            lscache.data[i].prefix = p;
            lscache.data[i].path = tmp;
            lscache.data[i].pathcrc = strcrc32(tmp);
            lscache.data[i].l = tl;
            #ifndef PSRC_NOMT
            writeToReadAccess(&lscache.lock);
            #endif
            l = &lscache.data[i].l;
        } else {
            free(tmp);
            return NULL;
        }
    }
    uint32_t fcrc = strcrc32(f);
    //printf("FIND: [%08X] {%s}\n", fcrc, f);
    int ct = l->count[t];
    struct rcls_file* fl = l->files[t];
    for (int i = 0; i < ct; ++i) {
        //printf("CMP [%08X] {%s}\n", fl[i].namecrc, fl[i].name);
        if (fl[i].namecrc == fcrc && !strcmp(fl[i].name, f)) return fl[i].dirbits;
    }
    return NULL;
}
#endif

static char* rcIdToPath(const char* id, bool allownative, enum rcprefix* p) {
    unsigned i = 0;
    while (1) {
        char c = id[i];
        if (!c) break;
        if (c == ':') {
            switch (*id) {
                case ':': ++id; goto selfmatch;
                case 's': if (i == 4 && !strncmp(id + 1, "elf", 3)) {id += 5; goto selfmatch;} break;
                case 'i': if (i == 8 && !strncmp(id + 1, "nternal", 7)) {*p = RCPREFIX_INTERNAL; id += 9; goto match;} break;
                case 'g': if (i == 4 && !strncmp(id + 1, "ame", 3)) {*p = RCPREFIX_GAME; id += 5; goto match;} break;
                case 'u': if (i == 4 && !strncmp(id + 1, "ser", 3)) {*p = RCPREFIX_USER; id += 5; goto match;} break;
                case 'n': if (allownative && i == 6 && !strncmp(id + 1, "ative", 5)) {id += 7; goto nativematch;} break;
                default: break;
            }
            plog(LL_ERROR, "Unknown prefix '%.*s' in resource identifier '%s'", i, id, id);
            return NULL;
        }
        ++i;
    }
    selfmatch:;
        *p = RCPREFIX_GAME;
        struct charbuf cb;
        cb_init(&cb, 128);
        cb_add(&cb, '/');
        cb_addstr(&cb, gameinfo.dir);
        restrictpath_cb(id, "/", '/', '_', &cb);
        return cb_finalize(&cb);
    nativematch:;
        *p = RCPREFIX_NATIVE;
        return strpath(id);
    match:;
        return restrictpath(id, "/", '/', '_');
}

bool lsRc(const char* id, bool allownative, struct rcls* l) {
    enum rcprefix p;
    char* r = rcIdToPath(id, allownative, &p);
    if (!r) return false;
    #if !defined(PSRC_NOMT) && (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    acquireReadAccess(&lscache.lock);
    #endif
    bool ret = lsRc_norslv(p, r, l);
    #if !defined(PSRC_NOMT) && (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    releaseReadAccess(&lscache.lock);
    #endif
    free(r);
    return ret;
}
bool lsCacheRc(const char* id, bool allownative, struct rcls* l) {
    enum rcprefix p;
    char* r = rcIdToPath(id, allownative, &p);
    if (!r) return false;
    uint32_t rcrc = strcrc32(r);
    #ifndef PSRC_NOMT
    acquireReadAccess(&lscache.lock);
    #endif
    {
        struct rcls* tl = lscGet(p, r, rcrc);
        if (tl) {
            lsRc_dup(tl, l);
            #ifndef PSRC_NOMT
            releaseReadAccess(&lscache.lock);
            #endif
            free(r);
            return true;
        }
    }
    #if !!defined(PSRC_NOMT) && (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    releaseReadAccess(&lscache.lock);
    #endif
    bool ret = lsRc_norslv(p, r, l);
    if (!ret) {
        #if !defined(PSRC_NOMT) && (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        releaseReadAccess(&lscache.lock);
        #endif
        free(r);
        return false;
    }
    #ifndef PSRC_NOMT
    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    acquireWriteAccess(&lscache.lock);
    #else
    readToWriteAccess(&lscache.lock);
    #endif
    #endif
    int i = lscAdd();
    lscache.data[i].prefix = p;
    lscache.data[i].path = r;
    lscache.data[i].pathcrc = rcrc;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&lscache.lock);
    #endif
    lsRc_dup(l, &lscache.data[i].l);
    return true;
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
    for (unsigned p = 0; p < rcgroups[type].pagect; ++p) {
        register uint16_t occ = rcgroups[type].pages[p].occ;
        if (!occ) continue;
        unsigned i = 0;
        while (1) {
            if (occ & 1) {
                struct resource* rc = (void*)((char*)rcgroups[type].pages[p].data + i * rcallocsz[type]);
                if (rc->header.prefix == prefix && rc->header.pathcrc == pathcrc && !strcmp(rc->header.path, path) && cmpRcOpt(type, rc, opt)) return rc;
            }
            if (i == 15) break;
            ++i;
            occ >>= 1;
            //if (!occ) break;
        }
    }
    return NULL;
}

static int getRcAcc_findInFS(struct charbuf* cb, enum rctype type, const char** ext, const char* s, ...) {
    {
        cb_addstr(cb, s);
        va_list v;
        va_start(v, s);
        while ((s = va_arg(v, const char*))) {
            cb_addstr(cb, s);
        }
        va_end(v);
    }
    const char* const* exts = rcextensions[type];
    const char* tmp;
    while ((tmp = *exts)) {
        unsigned long l = cb->len;
        if (*tmp) {
            cb_add(cb, '.');
            cb_add(cb, *tmp++);
            while (*tmp) {
                cb_add(cb, *tmp);
                ++tmp;
            }
        }
        //printf("TRY: {%s}\n", cb_peek(cb));
        int status = isFile(cb_peek(cb));
        if (status == 1) {
            if (ext) *ext = *exts;
            return status;
        }
        cb->len = l;
        ++exts;
    }
    return -1;
}
#define GRA_TRYFS(...) do {\
    if (getRcAcc_findInFS(&cb, type, &acc->ext, __VA_ARGS__, NULL) == 1) {\
        acc->src = RCSRC_FS;\
        acc->fs.path = cb_finalize(&cb);\
        return true;\
    }\
} while (0)
#if defined(PSRC_NOMT) || !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #define GRA_TRYFS_UNLOCKLSCRD GRA_TRYFS
#else
    #define GRA_TRYFS_UNLOCKLSCRD(...) do {\
        if (getRcAcc_findInFS(&cb, type, &acc->ext, __VA_ARGS__, NULL) == 1) {\
            releaseReadAccess(&lscache.lock);\
            acc->src = RCSRC_FS;\
            acc->fs.path = cb_finalize(&cb);\
            return true;\
        }\
    } while (0)
#endif
static bool getRcAcc(enum rctype type, enum rcprefix prefix, const char* path, uint32_t pathcrc, struct rcaccess* acc) {
    (void)pathcrc;
    switch (prefix) {
        default:
        case RCPREFIX_INTERNAL: {
            #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
            #ifndef PSRC_NOMT
            acquireReadAccess(&lscache.lock);
            #endif
            const uint8_t* dirbits = lscFind(type, prefix, path);
            if (!dirbits) {
                #ifndef PSRC_NOMT
                releaseReadAccess(&lscache.lock);
                #endif
                return false;
            }
            #endif
            struct charbuf cb;
            cb_init(&cb, 256);
            for (int i = 0; i < mods.len; ++i) {
                #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
                if (!((dirbits[i / 8] >> (i % 8)) & 1)) continue;
                #endif
                GRA_TRYFS_UNLOCKLSCRD(mods.data[i].path, PATHSEPSTR "internal" PATHSEPSTR "resources", path);
                cb_clear(&cb);
            }
            #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
            bool tmp = !((dirbits[mods.len / 8] >> (mods.len % 8)) & 1);
            #ifndef PSRC_NOMT
            releaseReadAccess(&lscache.lock);
            #endif
            if (tmp) break;
            #endif
            GRA_TRYFS(dirs[DIR_INTERNALRC], path);
            cb_dump(&cb);
        } break;
        case RCPREFIX_GAME: {
            #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
            #ifndef PSRC_NOMT
            acquireReadAccess(&lscache.lock);
            #endif
            const uint8_t* dirbits = lscFind(type, prefix, path);
            if (!dirbits) {
                #ifndef PSRC_NOMT
                releaseReadAccess(&lscache.lock);
                #endif
                return false;
            }
            #endif
            struct charbuf cb;
            cb_init(&cb, 256);
            for (int i = 0; i < mods.len; ++i) {
                #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
                if (!((dirbits[i / 8] >> (i % 8)) & 1)) continue;
                #endif
                GRA_TRYFS_UNLOCKLSCRD(mods.data[i].path, PATHSEPSTR "games", path);
                cb_clear(&cb);
            }
            #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
            bool tmp = !((dirbits[mods.len / 8] >> (mods.len % 8)) & 1);
            #ifndef PSRC_NOMT
            releaseReadAccess(&lscache.lock);
            #endif
            if (tmp) break;
            #endif
            GRA_TRYFS(dirs[DIR_GAMES], path);
            cb_dump(&cb);
        } break;
        case RCPREFIX_USER: {
            #ifndef PSRC_MODULE_SERVER
            struct charbuf cb;
            cb_init(&cb, 256);
            GRA_TRYFS(dirs[DIR_USERRC], path);
            cb_dump(&cb);
            #endif
        } break;
        case RCPREFIX_NATIVE: {
            const char* tmpext = NULL;
            {
                const char* tmp = path;
                char c = *tmp;
                while (c) {
                    if (c == '.') tmpext = tmp + 1;
                    else if (ispathsep(c)) tmpext = NULL;
                    c = *tmp;
                }
            }
            if (!tmpext || !*tmpext) break;
            const char* const* exts = rcextensions[type];
            do {
                if (!strcasecmp(tmpext, *exts) && isFile(path) == 1) {
                    acc->src = RCSRC_FS;
                    acc->ext = *exts;
                    acc->fs.path = strdup(path);
                    return true;
                }
                ++exts;
            } while (*exts);
        } break;
    }
    return false;
}
#undef GRA_TRYFS
#undef GRA_TRYFS_UNLOCKLSCRD
static bool dsFromRcAcc(struct rcaccess* acc, struct datastream* ds) {
    switch (acc->src) {
        case RCSRC_FS: return ds_openfile(acc->fs.path, 0, ds);
    }
    return false;
}
static void delRcAcc(struct rcaccess* acc) {
    switch (acc->src) {
        case RCSRC_FS:
            free(acc->fs.path);
            break;
    }
}

static struct resource* newRc(enum rctype type) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&rclock);
    #endif
    for (unsigned p = 0; p < rcgroups[type].pagect; ++p) {
        register uint16_t unocc = ~rcgroups[type].pages[p].occ;
        if (!unocc) continue;
        unsigned i = 0;
        while (1) {
            if (unocc & 1) {
                rcgroups[type].pages[p].occ |= 1 << i;
                struct resource* rc = (void*)((char*)rcgroups[type].pages[p].data + i * rcallocsz[type]);
                rc->header.type = type;
                rc->header.refs = 1;
                rc->header.index = p * 16 + i;
                rc->header.forcefree = 0;
                #ifndef PSRC_NOMT
                releaseWriteAccess(&rclock);
                #endif
                return rc;
            }
            if (i == 15) break;
            ++i;
            unocc >>= 1;
            //if (!unocc) break;
        }
    }
    int p = rcgroups[type].pagect++;
    rcgroups[type].pages = rcmgr_realloc(rcgroups[type].pages, rcgroups[type].pagect * sizeof(*rcgroups[type].pages));
    void* data = rcmgr_malloc_nolock(16 * rcallocsz[type]);
    rcgroups[type].pages[p].data = data;
    rcgroups[type].pages[p].occ = 1;
    rcgroups[type].pages[p].zref = 0;
    struct resource* rc = data;
    rc->header.type = type;
    rc->header.refs = 1;
    rc->header.index = p * 16;
    rc->header.forcefree = 0;
    #ifndef PSRC_NOMT
    releaseWriteAccess(&rclock);
    #endif
    return rc;
}
void* getRc(enum rctype type, const char* id, const void* opt, unsigned flags, struct charbuf* err) {
    enum rcprefix prefix;
    char* path = rcIdToPath(id, flags & LOADRC_FLAG_ALLOWNATIVE, &prefix);
    if (!path) {
        plog(LL_ERROR, "Resource identifier '%s' is invalid", id);
        return NULL;
    }
    if (!*path) {
        free(path);
        plog(LL_ERROR, "Resolved resource path for identifier '%s' is empty", id);
        return NULL;
    }
    uint32_t pathcrc = strcrc32(path);
    #ifndef PSRC_NOMT
    acquireReadAccess(&rclock);
    #endif
    #if DEBUG(2)
    plog(LL_INFO | LF_DEBUG, "Searching for an already loaded %s with resource identifier '%s'", rctypenames[type], id);
    #endif
    struct resource* rc = findRc(type, prefix, path, pathcrc, opt);
    if (rc) {
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG, "Found already loaded %s with resource identifier '%s'", rctypenames[type], id);
        #endif
        free(path);
        #ifndef PSRC_NOMT
        readToWriteAccess(&rclock);
        #endif
        if (!rc->header.refs++) {
            unsigned i = rc->header.index;
            rcgroups[rc->header.type].pages[i / 16].zref &= ~(1 << (i % 16));
            --rcgroups[rc->header.type].zrefct;
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
    plog(LL_INFO | LF_DEBUG, "Loading %s '%s:%s'...", rctypenames[type], rcprefixnames[prefix], path);
    #endif
    struct rcaccess acc;
    if (!getRcAcc(type, prefix, path, pathcrc, &acc)) {
        plog(LL_ERROR, "Failed to find %s '%s:%s'", rctypenames[type], rcprefixnames[prefix], path);
        free(path);
        return NULL;
    }
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
            (void)err;
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
    rc->header.prefix = prefix;
    rc->header.path = path;
    rc->header.pathcrc = pathcrc;
    rc->header.hasdatacrc = 0;
    return &rc->data;
    fail:;
    plog(LL_ERROR, "Failed to load %s '%s:%s'", rctypenames[type], rcprefixnames[prefix], path);
    delRcAcc(&acc);
    free(path);
    return NULL;
}

void lockRc(void* rp) {
    #ifndef PSRC_NOMT
    acquireWriteAccess(&rclock);
    #endif
    struct resource* rc = (void*)((char*)rp - offsetof(struct resource, data));
    if (!rc->header.refs++) {
        rcgroups[rc->header.type].pages[rc->header.index / 16].zref &= ~(1 << (rc->header.index % 16));
        --rcgroups[rc->header.type].zrefct;
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
    //printf("FREERC: %d %d %zu %p %p\n", type, rc->header.type, rcallocsz[type], rcgroups[type].data, rc);
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG, "Freeing %s '%s:%s'...", rctypenames[rc->header.type], rcprefixnames[rc->header.prefix], rc->header.path);
    #endif
    freeRcData(type, rc);
    freeRcHeader(&rc->header);
}

void rlsRc(void* rp, bool force) {
    struct resource* rc = (void*)((char*)rp - offsetof(struct resource, data));
    #ifndef PSRC_NOMT
    acquireWriteAccess(&rclock);
    #endif
    if (force) rc->header.forcefree = 1;
    if (!--rc->header.refs) {
        enum rctype type = rc->header.type;
        if (rc->header.forcefree) {
            rcgroups[type].pages[rc->header.index / 16].occ &= ~(1 << (rc->header.index % 16));
            rcgroups[type].pages[rc->header.index / 16].zref &= ~(1 << (rc->header.index % 16));
            --rcgroups[type].zrefct;
            freeRc(type, rc);
        } else {
            rc->header.zreftick = rctick;
            rcgroups[type].pages[rc->header.index / 16].zref |= 1 << (rc->header.index % 16);
            ++rcgroups[type].zrefct;
        }
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
        if (tmp) {
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
    for (int i = 0; i < mods.len; ++i) {
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
    for (int i = 0; i < mods.len; ++i) {
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

static uintptr_t gcrcs_freeafter = 3;
static void gcRcs_internal(bool ag) {
    #if DEBUG(2)
    plog(LL_INFO | LF_DEBUG, "Running resource garbage collector...%s", (ag) ? " (aggressive)" : "");
    #endif
    for (int g = 0; g < RC__COUNT; ++g) {
        if (!rcgroups[g].zrefct) continue;
        //printf("GC GROUP [%d]\n", g);
        for (unsigned p = 0; p < rcgroups[g].pagect; ++p) {
            register uint16_t zref = rcgroups[g].pages[p].zref;
            if (!zref) continue;
            register uint16_t occ = rcgroups[g].pages[p].occ;
            if (!occ) continue;
            unsigned i = 0;
            while (1) {
                if (occ & 1 && zref & 1) {
                    struct resource* rc = (void*)((char*)rcgroups[g].pages[p].data + i * rcallocsz[g]);
                    if (ag || rctick - rc->header.zreftick >= gcrcs_freeafter) {
                        rcgroups[g].pages[p].occ &= ~(1 << i);
                        rcgroups[g].pages[p].zref &= ~(1 << i);
                        --rcgroups[g].zrefct;
                        freeRc(g, rc);
                    }
                }
                if (i == 15) break;
                ++i;
                occ >>= 1;
                zref >>= 1;
                //if (!occ || !zref) break;
            }
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
#if 0
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
#endif

void clRcCache(void) {
    lscDelAll();
    gcRcs(true);
}

static uint64_t lasttick;
static uint64_t ticktime = 1000000;
static unsigned long alloccheck = 262144;
void runRcMgr(uint64_t t) {
    {
        uint64_t tmp = t - lasttick;
        if (tmp < ticktime) return;
        lasttick += tmp / ticktime * ticktime;
    }
    static unsigned counter = 0;
    ++counter;
    #ifndef PSRC_NOMT
    acquireWriteAccess(&rclock);
    #endif
    if (counter == 5) {
        counter = 0;
        void* ptr = malloc(alloccheck);
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
    if (!createAccessLock(&lscache.lock)) return false;
    #endif

    char* tmp = cfg_getvar(&config, "Resource Manager", "gc.ticktime");
    if (tmp) {
        ticktime = strsec(tmp, ticktime);
        free(tmp);
    }
    tmp = cfg_getvar(&config, "Resource Manager", "gc.freeafter");
    if (tmp) {
        gcrcs_freeafter = strtoul(tmp, NULL, 10);
        free(tmp);
    }
    tmp = cfg_getvar(&config, "Resource Manager", "gc.alloccheck");
    if (tmp) {
        alloccheck = strtoul(tmp, NULL, 10);
        free(tmp);
    }
    tmp = cfg_getvar(&config, "Resource Manager", "lscache.size");
    if (tmp) {
        lscache.size = atoi(tmp);
        if (lscache.size < 1) lscache.size = 1;
    } else {
        #if PLATFORM != PLAT_NXDK && (PLATFLAGS & (PLATFLAG_UNIXLIKE | PLATFLAG_WINDOWSLIKE))
        lscache.size = 32;
        #else
        lscache.size = 12;
        #endif
    }

    lasttick = altutime();

    return true;
}

void quitRcMgr(void) {
    for (unsigned g = 0; g < RC__COUNT; ++g) {
        for (unsigned p = 0; p < rcgroups[g].pagect; ++p) {
            register uint16_t occ = rcgroups[g].pages[p].occ;
            if (!occ) continue;
            unsigned i = 0;
            while (1) {
                if (occ & 1) freeRc(g, (void*)((char*)rcgroups[g].pages[p].data + i * rcallocsz[g]));
                if (i == 15) break;
                ++i;
                occ >>= 1;
                //if (!occ) break;
            }
            free(rcgroups[g].pages[p].data);
        }
        rcgroups[g].pagect = 0;
        rcgroups[g].zrefct = 0;
        free(rcgroups[g].pages);
    }

    lscDelAll();

    #ifndef PSRC_NOMT
    destroyAccessLock(&rclock);
    destroyAccessLock(&mods.lock);
    destroyAccessLock(&lscache.lock);
    #endif
}
