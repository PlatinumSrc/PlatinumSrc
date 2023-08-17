#include "resource.h"
#include "../psrc_aux/threading.h"

#include <stddef.h>
#include <stdlib.h>

#undef freeResource

struct __attribute__((packed)) rcdata {
    struct rcheader header;
    union __attribute__((packed)) {
        struct __attribute__((packed)) {
            struct rc_entity* entity;
            //struct rcopt_entity* entityopt;
        };
        struct __attribute__((packed)) {
            struct rc_map* map;
            struct rcopt_map* mapopt;
        };
        struct __attribute__((packed)) {
            struct rc_material* material;
            struct rcopt_material* materialopt;
        };
        struct __attribute__((packed)) {
            struct rc_model* model;
            struct rcopt_model* modelopt;
        };
        struct __attribute__((packed)) {
            //struct rc_prop* prop;
            //struct rcopt_prop* propopt;
        };
        struct __attribute__((packed)) {
            //struct rc_script* script;
            //struct rcopt_script* scriptopt;
        };
        struct __attribute__((packed)) {
            struct rc_sound* sound;
            //struct rcopt_sound* soundopt;
        };
        struct __attribute__((packed)) {
            struct rc_texture* texture;
            struct rcopt_texture* textureopt;
        };
    };
};

static int rccount = 0;
static struct rcdata** rcdata = NULL;
static mutex_t rclock;

static struct rcdata* loadResource_internal(enum rctype t, char* p, union rcopt* o) {
    switch (t) {
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
    struct rcdata* r = rcdata[_r->header.index];
    --r->header.refs;
    if (!r->header.refs) {
        switch (r->header.type) {
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
        rcdata[_r->header.index] = NULL;
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
