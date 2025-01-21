#ifndef PSRC_COMMON_PKD_H
#define PSRC_COMMON_PKD_H

#include "../platform.h"
#include "../attribs.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#define PKD_VER 0

struct pkd {
    FILE* f;
    #if (PLATFLAGS & PLATFLAG_UNIXLIKE) || PLATFORM == PLAT_WIN32
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
    enum pkd_type type : 7;
    uint8_t isarray : 1;
    uint32_t size;
};

struct pkd_keylist {
    char** names;
    struct pkd_keytype* types;
};

enum pkd_setop {
    PKD_SETOP_SET, // will always be 0
    PKD_SETOP_APPEND
};

int pkd_open(const char* p, bool nolock, struct pkd*);
void pkd_close(struct pkd*);

int pkd_querytype(struct pkd*, const char* const* p, struct pkd_keytype* t);
int pkd_list(struct pkd*, const char* const* p, struct pkd_keylist*);
void pkd_freelist(struct pkd_keylist*);
void* pkd_get(struct pkd*, const char* const* p, uint32_t i, struct pkd_keytype* t);
int pkd_set(struct pkd*, const char* const* p, uint32_t i, enum pkd_setop, const struct pkd_keytype* t, void* d);
int pkd_ren(struct pkd*, const char* const* oldp, const char* const* newp);
int pkd_del(struct pkd*, const char* const* p);

#endif
