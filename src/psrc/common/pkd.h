#ifndef PSRC_COMMON_PKD_H
#define PSRC_COMMON_PKD_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "../platform.h"
#include "../attribs.h"

#define PKD_VER 0

struct pkd {
    FILE* f;
    #if (PLATFLAGS & PLATFLAG_UNIXLIKE) || (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    char* lockpath;
    #endif
};

enum pkd_err {
    PKD_ERR_NONE,
    PKD_ERR_TYPE,
    PKD_ERR_CANTFIND,
    PKD_ERR_OPEN_CANTOPEN,
    PKD_ERR_OPEN_LOCKED
};

PACKEDENUM pkd_dtype {
    PKD_DTYPE_BOOL,
    PKD_DTYPE_I8,
    PKD_DTYPE_I16,
    PKD_DTYPE_I32,
    PKD_DTYPE_I64,
    PKD_DTYPE_U8,
    PKD_DTYPE_U16,
    PKD_DTYPE_U32,
    PKD_DTYPE_U64,
    PKD_DTYPE_F32,
    PKD_DTYPE_F64,
    PKD_DTYPE_STR
};
struct pkd_vtype {
    enum pkd_dtype type;
    unsigned dim;
    union {
        unsigned size;
        unsigned* sizes;
    };
};
struct pkd_vindex {
    unsigned dim;
    union {
        unsigned ind;
        unsigned* inds;
    };
};

enum pkd_setop {
    PKD_SETOP_SET, // will always be 0
    PKD_SETOP_APPEND,
    PKD_SETOP_DELETE,
};

int pkd_open(const char* p, struct pkd*);
void pkd_close(struct pkd*);

int pkd_set(struct pkd*, const char* const* p, const struct pkd_vindex* i, enum pkd_setop, const struct pkd_vtype* t, void* d);
int pkd_gettype(struct pkd*, const char* const* p, const struct pkd_vindex* i, struct pkd_vtype* t);
void* pkd_get(struct pkd*, const char* const* p, const struct pkd_vindex* i, struct pkd_vtype* t);

static ALWAYSINLINE void pkd_freevtype(struct pkd_vtype* t) {
    if (t->dim > 1) free(t->sizes);
}

#endif
