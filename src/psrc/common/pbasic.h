#ifndef PSRC_COMMON_PBASIC_H
#define PSRC_COMMON_PBASIC_H

#include "string.h"
#include "memory.h"
#include "threading.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "../util.h"

PACKEDENUM(pbtype {
    PBTYPE__SPECIAL = -1,
    PBTYPE_VOID,
    PBTYPE_BOOL,
    PBTYPE_I8,
    PBTYPE_I16,
    PBTYPE_I32,
    PBTYPE_I64,
    PBTYPE_U8,
    PBTYPE_U16,
    PBTYPE_U32,
    PBTYPE_U64,
    PBTYPE_F32,
    PBTYPE_F64,
    PBTYPE_VEC,
    PBTYPE_STR
});

struct pbdata_vec {
    float x;
    float y;
    float z;
};
union pbdata {
    uint8_t b;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    float f32;
    double f64;
    struct pbdata_vec vec;
    struct charbuf str;
    struct {
        union {
            uint8_t* b; // packed bitmap
            int8_t* i8;
            int16_t* i16;
            int32_t* i32;
            int64_t* i64;
            uint8_t* u8;
            uint16_t* u16;
            uint32_t* u32;
            uint64_t* u64;
            float* f32;
            double* f64;
            struct pbdata_vec* vec;
            struct charbuf* str;
        };
        unsigned len;
        unsigned size;
        uint8_t copied : 1;
    } array;
};

struct pb_constref {
    union {
        uintptr_t pos;
        union pbdata* data;
    };
    enum pbtype type;
    unsigned dim;
    union {
        unsigned size;
        unsigned sizeindex;
    };
};
struct pb_script {
    void* ops;
    void* data;
    struct pb_constref* consts;
    unsigned subcount;
    unsigned varcount;
};

struct pbvm;
struct pbvm_ccall_arg;
struct pbvm_ccall_ret;
struct pbc;

typedef void (*pb_cfunc)(struct pbvm*, void* data, struct pbvm_ccall_arg*, struct pbvm_ccall_ret**);
typedef bool (*pb_findcmd)(struct pbc*, const char*, bool isfunc);
typedef bool (*pb_findpv)(const char*, int*);

struct pbc_opt {
    pb_findcmd findcmd;
    pb_findpv findpv;
};
struct pbc_stream { // TODO: add pb_compilemem() later if needed
    FILE* f;
    unsigned line;
    unsigned col;
    unsigned lastline;
    unsigned lastcol;
    int last;
    bool undo;
};
struct pbc_name {
    char* name;
    uint32_t namecrc;
};
struct pbc_label {
    char* name;
    uint32_t namecrc;
    int location;
};
struct pbc_scope {
    struct {
        int* data;
        int len;
        int size;
    } locals;
    struct {
        struct pbc_label* data;
        int len;
        int size;
    } labels;
};
struct pbc {
    struct pbc_stream input;
    struct pbc_opt* opt;
    struct {
        struct pbc_scope* data;
        int current;
        int size;
    } scopes;
    struct membuf ops;
    struct membuf constdata;
    struct {
        struct pbc_name* data;
        int len;
        int size;
    } vars;
    struct {
        struct pbc_name* data;
        int len;
        int size;
    } subs;
};

bool pb_compilefile(const char*, struct pbc_opt*, struct pb_script* out, struct charbuf* err);
void pb_deletescript(struct pb_script*);

#endif
