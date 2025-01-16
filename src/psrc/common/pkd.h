#ifndef PSRC_COMMON_PKD_H
#define PSRC_COMMON_PKD_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

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
    PKD_ERR_NOTFOUND,
    PKD_ERR_EXISTS,
    PKD_ERR_TYPE,
    PKD_ERR_BOUNDS,
    PKD_ERR_FORMAT,
    PKD_ERR_OPEN_CANTOPEN,
    PKD_ERR_OPEN_LOCKED
};

PACKEDENUM pkd_type {
    PKD_TYPE_NONE,
    PKD_TYPE_BOOL,
    PKD_TYPE_I8,
    PKD_TYPE_I16,
    PKD_TYPE_I32,
    PKD_TYPE_I64,
    PKD_TYPE_U8,
    PKD_TYPE_U16,
    PKD_TYPE_U32,
    PKD_TYPE_U64,
    PKD_TYPE_F32,
    PKD_TYPE_F64,
    PKD_TYPE_STR
};
struct pkd_keytype {
    enum pkd_type type;
    uint8_t isarray : 1;
    uint8_t : 7;
    unsigned size;
};

enum pkd_setop {
    PKD_SETOP_SET, // will always be 0
    PKD_SETOP_APPEND
};

int pkd_open(const char* p, struct pkd*);
void pkd_close(struct pkd*);

int pkd_querytype(struct pkd*, const char* const* p, struct pkd_keytype* t);
void* pkd_get(struct pkd*, const char* const* p, unsigned i, struct pkd_keytype* t);
int pkd_set(struct pkd*, const char* const* p, unsigned i, enum pkd_setop, const struct pkd_keytype* t, void* d);
int pkd_ren(struct pkd*, const char* const* oldp, const char* const* newp);
int pkd_swap(struct pkd*, const char* const* p, unsigned i1, unsigned i2);
int pkd_del(struct pkd*, const char* const* p);

#endif
