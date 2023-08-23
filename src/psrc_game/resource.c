#include "resource.h"

#include "../psrc_aux/string.h"
#include "../psrc_aux/filesystem.h"
#include "../psrc_aux/threading.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../glue.h"

#undef freeResource

struct __attribute__((packed)) rcdata {
    struct rcheader header;
    union __attribute__((packed)) {
        struct __attribute__((packed)) {
            struct rc_config config;
            //struct rcopt_config configopt;
        };
        struct __attribute__((packed)) {
            struct rc_entity entity;
            //struct rcopt_entity entityopt;
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
            //struct rc_prop prop;
            //struct rcopt_prop propopt;
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
    };
};

struct __attribute__((packed)) rcsubgroup {
    struct rcdata** data;
    int size;
    int len;
};

struct __attribute__((packed)) rcgroup {
    struct rcsubgroup data[RC__COUNT];
};

static struct rcgroup data[RCSRC__COUNT];
static mutex_t rclock;

static char* getRcPath(char* uri) {
    struct charbuf cb;
    cb_init(&cb, 256);
    char* tmp = uri;
    char c;
    char* prefix;
    while (1) {
        c = *tmp++;
        if (c) {
            if (c == ':') {
                char* srcstr = cb_finalize(&cb);
                break;
            } else {
                cb_add(&cb, c);
            }
        } else {
            cb_dump(&cb);
            prefix = strdup(dirs[DIR_SELF]);
            break;
        }
    }
    return NULL;
}

static struct rcdata* loadResource_internal(enum rctype t, char* p, union rcopt* o) {
    switch ((uint8_t)t) {
        case RC_CONFIG: {
            return NULL;
        } break;
        case RC_ENTITY: {
            return NULL;
        } break;
        case RC_MAP: {
            return NULL;
        } break;
        case RC_MATERIAL: {
            return NULL;
        } break;
        case RC_MODEL: {
            return NULL;
        } break;
        case RC_PROP: {
            return NULL;
        } break;
        case RC_SCRIPT: {
            return NULL;
        } break;
        case RC_SOUND: {
            return NULL;
        } break;
        case RC_TEXTURE: {
            return NULL;
        } break;
    }
    return NULL;
}

union resource loadResource(enum rctype t, char* p, union rcopt* o) {
    struct rcdata* r = loadResource_internal(t, p, o);
    if (!r) return (union resource){.ptr = NULL};
    return (union resource){.ptr = r + sizeof(struct rcheader)};
}

static void freeResource_internal(const struct rcdata* _r) {
    enum rctype type = _r->header.type;
    enum rcsrc src = _r->header.source;
    int index = _r->header.index;
    struct rcdata* r = data[src].data[type].data[index];
    --r->header.refs;
    if (!r->header.refs) {
        switch ((uint8_t)type) {
            case RC_CONFIG: {
            } break;
            case RC_ENTITY: {
            } break;
            case RC_MAP: {
            } break;
            case RC_MATERIAL: {
            } break;
            case RC_MODEL: {
            } break;
            case RC_PROP: {
            } break;
            case RC_SCRIPT: {
            } break;
            case RC_SOUND: {
            } break;
            case RC_TEXTURE: {
            } break;
        }
        free(r);
        data[src].data[type].data[index] = NULL;
    }
}

void freeResource(union resource _r) {
    if (_r.ptr) {
        const struct rcdata* r = _r.ptr - sizeof(struct rcheader);
        freeResource_internal(r);
    }
}

bool initResource(void) {
    if (!createMutex(&rclock)) return false;
    return true;
}
