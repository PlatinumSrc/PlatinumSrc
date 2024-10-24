// TODO: rewrite? (too much crust)

#include "resource.h"

#include "../common.h"
#include "../debug.h"

#include "logging.h"
#include "string.h"
#include "memory.h"
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

#undef loadResource
#undef freeResource
#undef grabResource
#undef releaseResource

#ifndef PSRC_NOMT
static mutex_t rclock;
#endif

union resource {
    void* ptr;
    struct rc_config* config;
    struct rc_font* font;
    struct rc_map* map;
    struct rc_model* model;
    struct rc_script* script;
    struct rc_sound* sound;
    struct rc_texture* texture;
    struct rc_values* values;
};

union rcopt {
    void* ptr;
    //struct rcopt_config* config;
    //struct rcopt_font* font;
    struct rcopt_map* map;
    struct rcopt_model* model;
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
            struct rc_model data;
            struct rcopt_model opt;
        } model;
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

struct rcgroup {
    int len;
    int size;
    struct rcdata** data;
};

static struct rcgroup groups[RC__COUNT];
static int groupsizes[RC__COUNT] = {2, 1, 1, 8, 4, 16, 16, 16};

static struct rcopt_model modelopt_default = {
    0, RCOPT_TEXTURE_QLT_HIGH
};
static struct rcopt_script scriptopt_default = {
    0
};
static struct rcopt_sound soundopt_default = {
    true
};
static struct rcopt_texture textureopt_default = {
    false, RCOPT_TEXTURE_QLT_HIGH
};

static void* const defaultopt[RC__COUNT] = {
    NULL,
    NULL,
    NULL,
    &modelopt_default,
    &scriptopt_default,
    &soundopt_default,
    &textureopt_default,
    NULL
};

static const char* const typenames[RC__COUNT] = {
    "config",
    "font",
    "map",
    "model",
    "script",
    "sound",
    "texture",
    "values"
};

static struct {
    struct modinfo* data;
    uint16_t len;
    uint16_t size;
    #ifndef PSRC_NOMT
    struct accesslock lock;
    #endif
} mods;

static const char* const* const extlist[RC__COUNT] = {
    (const char* const[]){"cfg", NULL},
    (const char* const[]){"ttf", "otf", NULL},
    (const char* const[]){"pmf", NULL},
    (const char* const[]){"p3m", NULL},
    (const char* const[]){"bas", NULL},
    (const char* const[]){"ogg", "mp3", "wav", NULL},
    (const char* const[]){"ptf", "png", "jpg", "tga", "bmp", NULL},
    (const char* const[]){"txt", NULL}
};

struct lscache_dir {
    enum rcprefix prefix;
    char* rcpath;
    uint32_t rcpathcrc;
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

#if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
static uint8_t* lscFind(enum rctype t, enum rcprefix p, const char* uri);
#endif

static void lsRc_dup(const struct rcls* in, struct rcls* out) {
    char* onames = malloc(in->nameslen);
    char* inames = in->names;
    out->names = onames;
    memcpy(onames, inames, in->nameslen);
    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    uint8_t* odirbmp = malloc(in->dirbmplen);
    uint8_t* idirbmp = in->dirbmp;
    out->dirbmp = odirbmp;
    memcpy(odirbmp, idirbmp, in->dirbmplen);
    #endif
    for (int ri = 0; ri < RC__DIR + 1; ++ri) {
        int ct = in->count[ri];
        out->count[ri] = ct;
        if (ct) {
            struct rcls_file* ofl = malloc(ct * sizeof(*ofl));
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
    uint8_t* dirbits;
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
        switch (p) {
            default:
                #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
                if (dirbits && !((dirbits[dirind / 8] >> (dirind % 8)) & 1)) {if (dirind > mods.len) goto longbreak; else goto longcont;}
                #endif
                if (dirind < mods.len) dir = mkpath(mods.data[dirind].path, "games", gameinfo.dir, r, NULL);
                else if (dirind == mods.len) dir = mkpath(dirs[DIR_GAME], r, NULL);
                else goto longbreak;
                break;
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
        struct lsstate s;
        if (!startls(dir, &s)) {
            free(dir);
            goto longcont;
        }
        free(dir);
        const char* n;
        const char* ln;
        while (getls(&s, &n, &ln)) {
            int ind;
            long unsigned ol = cb.len;
            {
                int isfile = isFile(ln);
                if (isfile > 0) {
                    cb_addstr(&cb, n);
                    cb_add(&cb, 0);
                    again:;
                    --cb.len;
                    if (cb.len == ol) continue;
                    if (cb.data[cb.len] == '.') {
                        char* tmp = &cb.data[cb.len + 1];
                        for (int j = 0; j < RC__COUNT; ++j) {
                            const char* const* exts = extlist[j];
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
                l->files[ind] = malloc(ld[ind].size * sizeof(**l->files));
            } else if (ld[ind].len == ld[ind].size) {
                ld[ind].size = ld[ind].size * 3 / 2;
                l->files[ind] = realloc(l->files[ind], ld[ind].size * sizeof(**l->files));
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
        cb.data = realloc(cb.data, cb.len);
        l->names = cb.data;
        l->nameslen = cb.len;
        #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        dirbmp.data = realloc(dirbmp.data, dirbmp.len);
        l->dirbmp = dirbmp.data;
        l->dirbmplen = dirbmp.len;
        #endif
        for (int ri = 0; ri < RC__DIR + 1; ++ri) {
            int ct = ld[ri].len;
            l->count[ri] = ct;
            if (ct) {
                struct rcls_file* fl = realloc(l->files[ri], ct * sizeof(**l->files));
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
void freeRcls(struct rcls* l) {
    free(l->names);
    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    free(l->dirbmp);
    #endif
    for (int i = 0; i < RC__DIR + 1; ++i) {
        free(l->files[i]);
    }
}

static inline void lscFreeDir(struct lscache_dir* d) {
    free(d->rcpath);
    freeRcls(&d->l);
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
        lscache.data = malloc(lscache.size * sizeof(*lscache.data));
        for (int i = 0; i < lscache.size; ++i) {
            lscache.data[i].rcpath = NULL;
        }
        lscache.head = 0;
        lscache.tail = 0;
        return 0;
    }
    for (int i = 0; i < lscache.size; ++i) {
        if (!lscache.data[i].rcpath) {
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
    if (!lscache.data) return;
    for (int i = 0; i < lscache.size; ++i) {
        if (lscache.data[i].rcpath) lscFreeDir(&lscache.data[i]);
    }
    free(lscache.data);
    lscache.data = NULL;
}
#if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
static struct rcls* lscGet_len(enum rcprefix p, const char* uri, uint32_t crc, int len) {
    if (!lscache.data) return NULL;
    int di = lscache.head;
    while (1) {
        struct lscache_dir* d = &lscache.data[di];
        if (d->prefix == p && d->rcpathcrc == crc && !strncmp(d->rcpath, uri, len)) {
            lscMoveToFront(di, true);
            return &d->l;
        }
        if (di == lscache.tail) break;
        di = d->next;
    }
    return NULL;
}
#endif
static struct rcls* lscGet(enum rcprefix p, const char* uri, uint32_t crc) {
    if (!lscache.data) return NULL;
    int di = lscache.head;
    while (1) {
        struct lscache_dir* d = &lscache.data[di];
        if (d->prefix == p && d->rcpathcrc == crc && !strcmp(d->rcpath, uri)) {
            lscMoveToFront(di, true);
            return &d->l;
        }
        if (di == lscache.tail) break;
        di = d->next;
    }
    return NULL;
}
#if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
static uint8_t* lscFind(enum rctype t, enum rcprefix p, const char* uri) {
    //printf("LSCFIND: [%d] [%d] {%s}\n", t, p, uri);
    int spos = 0;
    const char* f = uri;
    {
        int i = 0;
        while (1) {
            char c = uri[i];
            if (c == '/') {spos = i; f = &uri[i + 1];}
            else if (!c) break;
            ++i;
        }
    }
    struct rcls* l = lscGet_len(p, uri, crc32(uri, spos), spos);
    if (!l) {
        struct rcls tl;
        char* tmp = malloc(spos + 1);
        memcpy(tmp, uri, spos);
        tmp[spos] = 0;
        if (lsRc_norslv(p, tmp, &tl)) {
            int i = lscAdd();
            lscache.data[i].prefix = p;
            lscache.data[i].rcpath = tmp;
            lscache.data[i].rcpathcrc = strcrc32(tmp);
            lscache.data[i].l = tl;
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

char* rslvRcPath(const char* uri, enum rcprefix* p) {
    struct charbuf cb;
    cb_init(&cb, 128);
    {
        const char* tmp = uri;
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
                    default: break;
                }
                plog(LL_ERROR, "Unknown prefix '%s' in resource path '%s'", cb.data, uri);
                cb_dump(&cb);
                return NULL;
                match:;
                cb_clear(&cb);
                while ((c = *tmp)) {
                    cb_add(&cb, c);
                    ++tmp;
                }
                goto foundprefix;
            } else {
                cb_add(&cb, c);
            }
        }
    }
    *p = RCPREFIX_SELF;
    foundprefix:;
    char* tmp = restrictpath(cb_peek(&cb), "/", '/', '_');
    cb_dump(&cb);
    return tmp;
}

bool lsRc(const char* uri, struct rcls* l) {
    enum rcprefix p;
    char* r = rslvRcPath(uri, &p);
    if (!r) return false;
    bool ret = lsRc_norslv(p, r, l);
    free(r);
    return ret;
}
bool lsCacheRc(const char* uri, struct rcls* l) {
    enum rcprefix p;
    char* r = rslvRcPath(uri, &p);
    if (!r) return false;
    uint32_t rcrc = strcrc32(r);
    {
        struct rcls* tl = lscGet(p, r, rcrc);
        if (tl) {
            lsRc_dup(tl, l);
            free(r);
            return true;
        }
    }
    bool ret = lsRc_norslv(p, r, l);
    if (!ret) {
        free(r);
        return false;
    }
    int i = lscAdd();
    lscache.data[i].prefix = p;
    lscache.data[i].rcpath = r;
    lscache.data[i].rcpathcrc = rcrc;
    lsRc_dup(l, &lscache.data[i].l);
    return true;
}


static int getRcPath_try(struct charbuf* cb, enum rctype type, const char** ext, const char* s, ...) {
    {
        cb_addstr(cb, s);
        va_list v;
        va_start(v, s);
        while ((s = va_arg(v, const char*))) {
            cb_addstr(cb, s);
        }
        va_end(v);
    }
    const char* const* exts = extlist[type];
    const char* tmp;
    while ((tmp = *exts)) {
        int l = cb->len;
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
        if (status >= 1) {
            if (ext) *ext = *exts;
            return status;
        }
        cb->len = l;
        ++exts;
    }
    return -1;
}
#define GRP_TRY(...) do {\
    if ((filestatus = getRcPath_try(&cb, type, ext, __VA_ARGS__, NULL)) >= 1) goto found;\
} while (0)
char* getRcPath(enum rctype type, const char* uri, enum rcprefix* p, char** rp, const char** ext) {
    enum rcprefix prefix;
    char* uripath = rslvRcPath(uri, &prefix);
    if (!uripath) {
        #if DEBUG(1)
        plog(LL_ERROR | LF_DEBUG, "Resource path '%s' is invalid", uri);
        #endif
        return NULL;
    }
    if (!*uripath) {
        free(uripath);
        #if DEBUG(1)
        plog(LL_ERROR | LF_DEBUG, "Resolved resource path '%s' is empty", uri);
        #endif
        return NULL;
    }
    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    uint8_t* dirbits = lscFind(type, prefix, uripath);
    if (!dirbits) {
        free(uripath);
        return NULL;
    }
    #endif
    struct charbuf cb;
    cb_init(&cb, 256);
    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    for (int i = 0; uripath[i]; ++i) {
        if (uripath[i] == '/') uripath[i] = PATHSEP;
    }
    #endif
    int filestatus;
    switch (prefix) {
        default:
            for (int i = 0; i < mods.len; ++i) {
                #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
                if (!((dirbits[i / 8] >> (i % 8)) & 1)) continue;
                #endif
                GRP_TRY(mods.data[i].path, PATHSEPSTR "games" PATHSEPSTR, gameinfo.dir, uripath);
                cb_clear(&cb);
            }
            #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
            if (!((dirbits[mods.len / 8] >> (mods.len % 8)) & 1)) break;
            #endif
            GRP_TRY(dirs[DIR_GAME], uripath);
            break;
        case RCPREFIX_INTERNAL:
            for (int i = 0; i < mods.len; ++i) {
                #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
                if (!((dirbits[i / 8] >> (i % 8)) & 1)) continue;
                #endif
                GRP_TRY(mods.data[i].path, PATHSEPSTR "internal" PATHSEPSTR "resources", uripath);
                cb_clear(&cb);
            }
            #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
            if (!((dirbits[mods.len / 8] >> (mods.len % 8)) & 1)) break;
            #endif
            GRP_TRY(dirs[DIR_INTERNALRC], uripath);
            break;
        case RCPREFIX_GAME:
            for (int i = 0; i < mods.len; ++i) {
                #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
                if (!((dirbits[i / 8] >> (i % 8)) & 1)) continue;
                #endif
                GRP_TRY(mods.data[i].path, PATHSEPSTR "games", uripath);
                cb_clear(&cb);
            }
            #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
            if (!((dirbits[mods.len / 8] >> (mods.len % 8)) & 1)) break;
            #endif
            GRP_TRY(dirs[DIR_GAMES], uripath);
            break;
        case RCPREFIX_USER:
            #ifndef PSRC_MODULE_SERVER
            GRP_TRY(dirs[DIR_USERRC], uripath);
            #endif
            break;
    }
    cb_dump(&cb);
    free(uripath);
    #if DEBUG(1)
    plog(LL_ERROR | LF_DEBUG, "Could not find resource path '%s'", uri);
    #endif
    return NULL;
    found:;
    if (p) *p = prefix;
    if (rp) {
        #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        for (int i = 0; uripath[i]; ++i) {
            if (uripath[i] == PATHSEP) uripath[i] = '/';
        }
        #endif
        *rp = uripath;
    } else {
        free(uripath);
    }
    return cb_finalize(&cb);
}
#undef GRP_TRY

static struct rcdata* loadResource_newptr(enum rctype t) {
    struct rcgroup* g = &groups[t];
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
        case RC_MODEL:
            size = offsetof(struct rcdata, model) + sizeof(ptr->model);
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
    return ptr;
}

static struct rcdata* loadResource_internal(enum rctype, const char*, union rcopt, struct charbuf* e);

static inline void* loadResource_wrapper(enum rctype t, const char* uri, union rcopt o, struct charbuf* e) {
    struct rcdata* r = loadResource_internal(t, uri, o, e);
    if (!r) return NULL;
    return (void*)(((uint8_t*)r) + sizeof(struct rcheader));
}
#define loadResource_wrapper(t, p, o, e) loadResource_wrapper((t), (p), (union rcopt){.ptr = (void*)(o)}, e)

#define LR_NEWPTR() do {\
    d = loadResource_newptr(t);\
    d->header.type = t;\
    d->header.rcpath = rp;\
    d->header.path = p;\
    d->header.rcpathcrc = rpcrc;\
    d->header.pathcrc = pcrc;\
    d->header.hasdatacrc = false;\
    d->header.refs = 1;\
} while (0)
static struct rcdata* loadResource_internal(enum rctype t, const char* uri, union rcopt o, struct charbuf* e) {
    enum rcprefix prefix;
    char* rp;
    const char* ext;
    char* p = getRcPath(t, uri, &prefix, &rp, &ext);
    if (!p) {
        plog(LL_ERROR, "Could not find %s at resource path '%s'", typenames[t], uri);
        return NULL;
    }
    uint32_t pcrc = strcrc32(p);
    uint32_t rpcrc = strcrc32(p);
    if (!o.ptr) o.ptr = defaultopt[t];
    struct rcdata* d;
    {
        struct rcgroup* g = &groups[t];
        for (int i = 0; i < g->len; ++i) {
            d = g->data[i];
            if (d && d->header.pathcrc == pcrc && !strcmp(p, d->header.path)) {
                switch (t) {
                    case RC_MODEL: {
                        if ((o.model->flags & P3M_LOADFLAG_IGNOREVERTS) < (d->model.opt.flags & P3M_LOADFLAG_IGNOREVERTS)) goto nomatch;
                        if ((o.model->flags & P3M_LOADFLAG_IGNOREBONES) < (d->model.opt.flags & P3M_LOADFLAG_IGNOREBONES)) goto nomatch;
                        if ((o.model->flags & P3M_LOADFLAG_IGNOREANIMS) < (d->model.opt.flags & P3M_LOADFLAG_IGNOREANIMS)) goto nomatch;
                        if (o.model->texture_quality != d->model.opt.texture_quality) goto nomatch;
                    } break;
                    case RC_SCRIPT: {
                        if (o.script->compileropt.findcmd != d->script.opt.compileropt.findcmd) goto nomatch;
                        if (o.script->compileropt.findpv != d->script.opt.compileropt.findpv) goto nomatch;
                    } break;
                    case RC_TEXTURE: {
                        if (o.texture->needsalpha && d->texture.data.channels != RC_TEXTURE_FRMT_RGBA) goto nomatch;
                        if (o.texture->quality != d->texture.opt.quality) goto nomatch;
                    } break;
                    default: break;
                }
                ++d->header.refs;
                free(rp);
                free(p);
                return d;
                nomatch:;
            }
        }
    }
    d = NULL;
    switch (t) {
        case RC_CONFIG: {
            struct datastream ds;
            if (ds_openfile(&ds, p, 0)) {
                LR_NEWPTR();
                cfg_open(&d->config.data.config, &ds);
                ds_close(&ds);
            }
        } break;
        #ifndef PSRC_MODULE_SERVER
        case RC_FONT: {
            SFT_Font* font = sft_loadfile(p);
            if (font) {
                LR_NEWPTR();
                d->font.data.font = font;
            }
        } break;
        #endif
        case RC_MODEL: {
            struct p3m* m = p3m_loadfile(p, o.model->flags);
            if (m) {
                LR_NEWPTR();
                d->model.data.model = m;
                d->model.data.textures = malloc(m->indexgroupcount * sizeof(*d->model.data.textures));
                //struct rcopt_texture texopt = {false, o.model->texture_quality};
                for (int i = 0; i < m->indexgroupcount; ++i) {
                    //d->model.data.textures[i] = loadResource_wrapper(RC_TEXTURE, m->indexgroups[i].texture, &texopt, NULL);
                    d->model.data.textures[i] = NULL;
                }
                d->model.opt = *o.model;
            }
        } break;
        case RC_SCRIPT: {
            struct pb_script s;
            if (pb_compilefile(p, &o.script->compileropt, &s, e)) {
                LR_NEWPTR();
                d->script.data.script = s;
                d->script.opt = *o.script;
            }
        } break;
        #ifndef PSRC_MODULE_SERVER
        case RC_SOUND: {
            FILE* f = fopen(p, "rb");
            if (f) {
                if (ext == extlist[RC_SOUND][0]) {
                    fseek(f, 0, SEEK_END);
                    long sz = ftell(f);
                    if (sz > 0) {
                        uint8_t* data = malloc(sz);
                        fseek(f, 0, SEEK_SET);
                        fread(data, 1, sz, f);
                        stb_vorbis* v = stb_vorbis_open_memory(data, sz, NULL, NULL);
                        if (v) {
                            LR_NEWPTR();
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
                } else if (ext == extlist[RC_SOUND][1]) {
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
                            LR_NEWPTR();
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
                } else if (ext == extlist[RC_SOUND][2]) {
                    SDL_RWops* rwops;
                    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
                    rwops = SDL_RWFromFP(f, false);
                    #else
                    rwops = SDL_RWFromFile(p, "rb");
                    #endif
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
                                destfrmt = (is8bit) ? AUDIO_U8 : AUDIO_S16SYS;
                            }
                            int ret = SDL_BuildAudioCVT(
                                &cvt,
                                spec.format, spec.channels, spec.freq,
                                destfrmt, (spec.channels > 1) + 1, spec.freq
                            );
                            if (!ret) {
                                LR_NEWPTR();
                                d->sound.data.format = RC_SOUND_FRMT_WAV;
                                d->sound.data.size = sz;
                                d->sound.data.data = data;
                                d->sound.data.len = sz / spec.channels / ((destfrmt == AUDIO_S16SYS) + 1);
                                d->sound.data.freq = spec.freq;
                                d->sound.data.channels = spec.channels;
                                d->sound.data.is8bit = (destfrmt == AUDIO_U8);
                                d->sound.data.stereo = (spec.channels > 1);
                                d->sound.data.sdlfree = 1;
                                d->sound.opt = *o.sound;
                            } else if (ret == 1) {
                                cvt.len = sz;
                                data = SDL_realloc(data, cvt.len * cvt.len_mult);
                                cvt.buf = data;
                                if (SDL_ConvertAudio(&cvt)) {
                                    SDL_FreeWAV(data);
                                } else {
                                    data = SDL_realloc(data, cvt.len_cvt);
                                    LR_NEWPTR();
                                    d->sound.data.format = RC_SOUND_FRMT_WAV;
                                    d->sound.data.size = cvt.len_cvt;
                                    d->sound.data.data = data;
                                    d->sound.data.len = cvt.len_cvt / ((spec.channels > 1) + 1) / ((destfrmt == AUDIO_S16SYS) + 1);
                                    d->sound.data.freq = spec.freq;
                                    d->sound.data.channels = (spec.channels > 1) + 1;
                                    d->sound.data.is8bit = (destfrmt == AUDIO_U8);
                                    d->sound.data.stereo = (spec.channels > 1);
                                    d->sound.data.sdlfree = 1;
                                    d->sound.opt = *o.sound;
                                }
                            } else {
                                SDL_FreeWAV(data);
                            }
                        }
                        SDL_RWclose(rwops);
                    }
                }
                fclose(f);
            }
        } break;
        case RC_TEXTURE: {
            if (ext == extlist[RC_TEXTURE][0]) {
                unsigned r, c;
                uint8_t* data = ptf_loadfile(p, &r, &c);
                if (data) {
                    if (o.texture->needsalpha && c == 3) {
                        c = 4;
                        data = realloc(data, r * r * 4);
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
                    if (o.texture->quality != RCOPT_TEXTURE_QLT_HIGH) {
                        int r2 = r;
                        switch ((uint8_t)o.texture->quality) {
                            case RCOPT_TEXTURE_QLT_MED: {
                                r2 /= 2;
                            } break;
                            case RCOPT_TEXTURE_QLT_LOW: {
                                r2 /= 4;
                            } break;
                        }
                        if (r2 < 1) r2 = 1;
                        unsigned char* data2 = malloc(r * r * c);
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
                        }
                    }
                    LR_NEWPTR();
                    d->texture.data.width = r;
                    d->texture.data.height = r;
                    d->texture.data.channels = c;
                    d->texture.data.data = data;
                    d->texture.opt = *o.texture;
                }
            } else {
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
                                STBIR_EDGE_CLAMP, STBIR_FILTER_TRIANGLE, STBIR_COLORSPACE_LINEAR,
                                NULL
                            );
                            if (status) {
                                free(data);
                                w = w2;
                                h = h2;
                                data = data2;
                            }
                        }
                        LR_NEWPTR();
                        d->texture.data.width = w;
                        d->texture.data.height = h;
                        d->texture.data.channels = c;
                        d->texture.data.data = data;
                        d->texture.opt = *o.texture;
                    }
                }
            }
        } break;
        #endif
        case RC_VALUES: {
            struct datastream ds;
            if (ds_openfile(&ds, p, 0)) {
                LR_NEWPTR();
                cfg_open(&d->values.data.values, &ds);
                ds_close(&ds);
            }
        } break;
        default: break;
    }
    if (!d) {
        plog(LL_ERROR, "Failed to load %s at resource path '%s'", typenames[t], uri);
        free(p);
        free(rp);
    }
    return d;
}
#undef LR_NEWPTR

void* loadResource(enum rctype t, const char* uri, void* o, struct charbuf* e) {
    #ifndef PSRC_NOMT
    lockMutex(&rclock);
    #endif
    void* r = loadResource_wrapper(t, uri, o, e);
    #ifndef PSRC_NOMT
    unlockMutex(&rclock);
    #endif
    return r;
}

static void freeResource_internal(struct rcdata*);

static inline void freeResource_wrapper(void* r) {
    if (r) freeResource_internal((void*)(((uint8_t*)r) - sizeof(struct rcheader)));
}

static inline void freeResource_force(enum rctype type, struct rcdata* r) {
    switch (type) {
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
            if (r->sound.data.format == RC_SOUND_FRMT_WAV && r->sound.data.sdlfree) SDL_FreeWAV(r->sound.data.data);
            else free(r->sound.data.data);
        } break;
        case RC_TEXTURE: {
            free(r->texture.data.data);
        } break;
        default: break;
    }
    free(r->header.path);
    free(r->header.rcpath);
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
        freeResource_internal((void*)(((uint8_t*)r) - sizeof(struct rcheader)));
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
        struct rcdata* r = (void*)(((uint8_t*)_r) - sizeof(struct rcheader));
        ++r->header.refs;
        #ifndef PSRC_NOMT
        unlockMutex(&rclock);
        #endif
    }
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
        if (!ds_openfile(&ds, cb->data, 0)) {
            plog(LL_WARN, "No mod.cfg in '%s'", cb_peek(cb));
            return -1;
        }
        cfg_open(&cfg, &ds);
        ds_close(&ds);
    }
    if (mods.len == mods.size) {
        mods.size = mods.size * 3 / 2;
        mods.data = realloc(mods.data, mods.size * sizeof(*mods.data));
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
    lscDelAll();
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
        mods.data = malloc(mods.size * sizeof(*mods.data));
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
    struct modinfo* data = malloc(mods.len * sizeof(*data));
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

bool initResource(void) {
    #ifndef PSRC_NOMT
    if (!createMutex(&rclock)) return false;
    if (!createAccessLock(&mods.lock)) return false;
    if (!createAccessLock(&lscache.lock)) return false;
    #endif

    for (int i = 0; i < RC__COUNT; ++i) {
        groups[i].len = 0;
        groups[i].size = groupsizes[i];
        groups[i].data = malloc(groups[i].size * sizeof(*groups[i].data));
    }

    scriptopt_default.compileropt.findpv = common_findpv;

    char* tmp = cfg_getvar(&config, NULL, "lscache.size");
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

    lscDelAll();

    #ifndef PSRC_NOMT
    destroyMutex(&rclock);
    destroyAccessLock(&mods.lock);
    destroyAccessLock(&lscache.lock);
    #endif
}
