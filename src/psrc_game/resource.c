#include "resource.h"

#include "../psrc_aux/logging.h"
#include "../psrc_aux/string.h"
#include "../psrc_aux/filesystem.h"
#include "../psrc_aux/threading.h"
#include "../psrc_aux/config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../glue.h"

#undef loadResource
#undef freeResource

char* maindir;
char* userdir;

char* gamedir;

static int mods;
static char** modpaths;

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
    while (1) {
        char c = *tmp2;
        if (c == PATHSEP || !c) {
            char* tmp3;
            if (c) tmp3 = cb_reinit(&tmpcb, 256);
            else tmp3 = cb_finalize(&tmpcb);
            if (!strcmp(tmp3, "..")) {
                --level;
            } else if (strcmp(tmp3, ".")) {
                ++level;
            }
            free(tmp3);
            if (!c) break;
        } else {
            cb_add(&tmpcb, c);
        }
        ++tmp2;
    }
    if (level < 0) {
        plog(LL_ERROR, "%s reaches out of bounds", path);
        free(path);
        return NULL;
    }
    cb_init(&tmpcb, 256);
    int filestatus = -1;
    switch ((int8_t)prefix) {
        case RCPREFIX_SELF: {
            for (int i = 0; i < mods; ++i) {
                if ((filestatus = getRcPath_try(&tmpcb, type, modpaths[i], "games", gamedir, path, NULL)) >= 1) goto found;
                cb_clear(&tmpcb);
            }
            if ((filestatus = getRcPath_try(&tmpcb, type, maindir, "games", gamedir, path, NULL)) >= 1) goto found;
        } break;
        case RCPREFIX_COMMON: {
            for (int i = 0; i < mods; ++i) {
                if ((filestatus = getRcPath_try(&tmpcb, type, modpaths[i], "common", path, NULL)) >= 1) goto found;
                cb_clear(&tmpcb);
            }
            if ((filestatus = getRcPath_try(&tmpcb, type, maindir, "common", path, NULL)) >= 1) goto found;
        } break;
        case RCPREFIX_ENGINE: {
            for (int i = 0; i < mods; ++i) {
                if ((filestatus = getRcPath_try(&tmpcb, type, modpaths[i], "engine", path, NULL)) >= 1) goto found;
                cb_clear(&tmpcb);
            }
            if ((filestatus = getRcPath_try(&tmpcb, type, maindir, "engine", path, NULL)) >= 1) goto found;
        } break;
        case RCPREFIX_GAME: {
            for (int i = 0; i < mods; ++i) {
                if ((filestatus = getRcPath_try(&tmpcb, type, modpaths[i], "games", path, NULL)) >= 1) goto found;
                cb_clear(&tmpcb);
            }
            if ((filestatus = getRcPath_try(&tmpcb, type, maindir, "games", path, NULL)) >= 1) goto found;
        } break;
        case RCPREFIX_MOD: {
            for (int i = 0; i < mods; ++i) {
                if ((filestatus = getRcPath_try(&tmpcb, type, modpaths[i], path, NULL)) >= 1) goto found;
                cb_clear(&tmpcb);
            }
        } break;
        case RCPREFIX_USER: {
            if ((filestatus = getRcPath_try(&tmpcb, type, userdir, path, NULL)) >= 1) goto found;
        } break;
    }
    free(path);
    cb_dump(&tmpcb);
    plog(LL_ERROR, "Could not find %s", uri);
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
    if (!p) return NULL;
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

static void test_getRcPath(char* p, enum rctype t) {
    printf("{%s}: ", p);
    char* rp = getRcPath(p, t);
    if (rp) {
        printf("{%s}\n", rp);
    } else {
        printf("NULL\n");
    }
}

bool initResource(void) {
    if (!createMutex(&rclock)) return false;

    mods = 0;
    modpaths = calloc(1, sizeof(*modpaths));

    test_getRcPath("common:noexist", RC_CONFIG);
    test_getRcPath("common:textures/bricks", RC_TEXTURE);
    test_getRcPath("common:textures/bricks.png", RC_TEXTURE);

    return true;
}
