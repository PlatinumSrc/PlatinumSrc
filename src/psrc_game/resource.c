#include "resource.h"
#include "game.h"

#include "../psrc_aux/logging.h"
#include "../psrc_aux/string.h"
#include "../psrc_aux/filesystem.h"
#include "../psrc_aux/threading.h"
#include "../psrc_aux/config.h"
#include "../psrc_aux/crc.h"

#include "../debug.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../glue.h"

#undef loadResource
#undef freeResource

static struct {
    int len;
    int size;
    char** paths;
} modinfo;

static char** extlist[RC__COUNT] = {
    (char*[3]){".cfg", ".txt", NULL},
    (char*[3]){".psh", ".txt", NULL},
    (char*[2]){".txt", NULL},
    (char*[3]){".pgs", ".txt", NULL},
    (char*[2]){".pmf", NULL},
    (char*[2]){".txt", NULL},
    (char*[2]){".p3m", NULL},
    (char*[2]){".txt", NULL},
    (char*[2]){".txt", NULL},
    (char*[3]){".ogg", ".wav", NULL},
    (char*[6]){".png", ".jpg", ".tga", ".bmp", "", NULL}
};

struct __attribute__((packed)) rcdata {
    struct rcheader header;
    union __attribute__((packed)) {
        struct __attribute__((packed)) {
            struct rc_config config;
            //struct rcopt_config configopt;
        };
        struct __attribute__((packed)) {
            //struct rc_consolescript consolescript;
            //struct rcopt_consolescript consolescriptopt;
        };
        struct __attribute__((packed)) {
            struct rc_entity entity;
            //struct rcopt_entity entityopt;
        };
        struct __attribute__((packed)) {
            //struct rc_gamescript gamescript;
            //struct rcopt_gamescript gamescriptopt;
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
            //struct rc_prop prop;
            //struct rcopt_prop propopt;
        };
        struct __attribute__((packed)) {
            struct rc_sound sound;
            //struct rcopt_sound soundopt;
        };
        struct __attribute__((packed)) {
            struct rc_texture texture;
            struct rcopt_texture textureopt;
        };
    };
};

struct __attribute__((packed)) rcgroup {
    struct rcdata** data;
    int size;
    int len;
};

static struct rcgroup data[RC__COUNT];
static mutex_t rclock;

static inline int getRcPath_try(struct charbuf* tmpcb, enum rctype type, const char* s, ...) {
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
        if (status >= 1) return status;
        cb_undo(tmpcb, len);
        ++exts;
    }
    return -1;
}
static char* getRcPath(const char* uri, enum rctype type) {
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
    switch ((int8_t)prefix) {
        case RCPREFIX_SELF: {
            for (int i = 0; i < modinfo.len; ++i) {
                if ((filestatus = getRcPath_try(&tmpcb, type, modinfo.paths[i], "games", gamedir, path, NULL)) >= 1) goto found;
                cb_clear(&tmpcb);
            }
            if ((filestatus = getRcPath_try(&tmpcb, type, maindir, "games", gamedir, path, NULL)) >= 1) goto found;
        } break;
        case RCPREFIX_COMMON: {
            for (int i = 0; i < modinfo.len; ++i) {
                if ((filestatus = getRcPath_try(&tmpcb, type, modinfo.paths[i], "common", path, NULL)) >= 1) goto found;
                cb_clear(&tmpcb);
            }
            if ((filestatus = getRcPath_try(&tmpcb, type, maindir, "common", path, NULL)) >= 1) goto found;
        } break;
        case RCPREFIX_ENGINE: {
            for (int i = 0; i < modinfo.len; ++i) {
                if ((filestatus = getRcPath_try(&tmpcb, type, modinfo.paths[i], "engine", path, NULL)) >= 1) goto found;
                cb_clear(&tmpcb);
            }
            if ((filestatus = getRcPath_try(&tmpcb, type, maindir, "engine", path, NULL)) >= 1) goto found;
        } break;
        case RCPREFIX_GAME: {
            for (int i = 0; i < modinfo.len; ++i) {
                if ((filestatus = getRcPath_try(&tmpcb, type, modinfo.paths[i], "games", path, NULL)) >= 1) goto found;
                cb_clear(&tmpcb);
            }
            if ((filestatus = getRcPath_try(&tmpcb, type, maindir, "games", path, NULL)) >= 1) goto found;
        } break;
        case RCPREFIX_MOD: {
            if ((filestatus = getRcPath_try(&tmpcb, type, userdir, "mods", path, NULL)) >= 1) goto found;
            cb_clear(&tmpcb);
            if ((filestatus = getRcPath_try(&tmpcb, type, maindir, "mods", path, NULL)) >= 1) goto found;
        } break;
        case RCPREFIX_USER: {
            if ((filestatus = getRcPath_try(&tmpcb, type, userdir, path, NULL)) >= 1) goto found;
        } break;
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

static struct rcdata* getRcHandle(enum rctype t, const char* p) {

}

static struct rcdata* loadResource_internal(enum rctype t, const char* p, union rcopt* o) {
    p = getRcPath(p, t);
    if (!p) {
        plog(LL_ERROR, "Could not find %s", p);
        return NULL;
    }
    uint32_t pcrc = strcrc32(p);
    switch ((uint8_t)t) {
        case RC_CONFIG: {
            //return cfg_open(p);
        } break;
        default: {
            return NULL;
        } break;
    }
    return NULL;
}

union resource loadResource(enum rctype t, const char* p, union rcopt* o) {
    lockMutex(&rclock);
    struct rcdata* r = loadResource_internal(t, p, o);
    unlockMutex(&rclock);
    if (!r) return (union resource){.ptr = NULL};
    return (union resource){.ptr = r + sizeof(struct rcheader)};
}

static void freeResource_internal(const struct rcdata* _r) {
    enum rctype type = _r->header.type;
    int index = _r->header.index;
    struct rcdata* r = data[type].data[index];
    --r->header.refs;
    if (!r->header.refs) {
        switch ((uint8_t)type) {
            case RC_CONFIG: {
                cfg_close(r->config.config);
            } break;
        }
        free(r);
        data[type].data[index] = NULL;
    }
}

void freeResource(union resource _r) {
    if (_r.ptr) {
        const struct rcdata* r = _r.ptr - sizeof(struct rcheader);
        lockMutex(&rclock);
        freeResource_internal(r);
        unlockMutex(&rclock);
    }
}

#if 0
static void test_getRcPath(char* p, enum rctype t) {
    char* rp = getRcPath(p, t);
    if (rp) {
        plog(LL_PLAIN, "{%s}: {%s}", p, rp);
    } else {
        plog(LL_PLAIN, "{%s}: NULL", p);
    }
    free(rp);
}
#endif

static inline void loadMods_addpath(char* p) {
    ++modinfo.len;
    if (modinfo.len == modinfo.size) {
        modinfo.size *= 2;
        modinfo.paths = realloc(modinfo.paths, modinfo.size * sizeof(*modinfo.paths));
    }
    modinfo.paths[modinfo.len - 1] = p;
}

void loadMods(const char* const* modnames, int modcount) {
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
            plog(LL_INFO, "Requested mods: %s", cb_peek(&cb));
            cb_dump(&cb);
        }
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
        #endif
    } else {
        modinfo.size = 0;
        free(modinfo.paths);
        modinfo.paths = NULL;
    }
}

bool initResource(void) {
    if (!createMutex(&rclock)) return false;

    {
        char* modstr = cfg_getvar(config, NULL, "mods");
        if (modstr) {
            int modcount;
            char** modnames = splitstrlist(modstr, ',', false, &modcount);
            free(modstr);
            loadMods((const char* const*)modnames, modcount);
            for (int i = 0; i < modcount; ++i) {
                free(modnames[i]);
            }
            free(modnames);
        } else {
            loadMods(NULL, 0);
        }
    }

    return true;
}
