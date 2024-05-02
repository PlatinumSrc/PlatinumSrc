#ifndef PSRC_COMMON_PBASIC_H
#define PSRC_COMMON_PBASIC_H

#include "string.h"
#include "threading.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

enum __attribute__((packed)) pbtype {
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
};

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
    unsigned pos;
    char* name;
    uint32_t namecrc;
    enum pbtype type;
    unsigned dim;
    union {
        unsigned size;
        unsigned sizeindex;
    };
};
struct pb_varref {
    char* name;
    uint32_t namecrc;
};
struct pb_subref {
    char* name;
    uint32_t namecrc;
};
struct script {
    void* ops;
    void* consts;
    unsigned constcount;
    unsigned varcount;
    unsigned subcount;
    struct pb_constref* constrefs;
    unsigned* constsizes;
    struct pb_varref* varrefs;
    struct pb_subref* subrefs;
};

struct pbvm;
struct pbvm_ccall_arg;
struct pbvm_ccall_ret;
struct pbc;

typedef void (*pb_cfunc)(struct pbvm*, void*, struct pbvm_ccall_arg*, struct pbvm_ccall_ret**);
typedef bool (*pb_findext)(struct pbc*, char*);

struct pbc_opt {
    pb_findext findext;
    bool fakedata; // turn DATA statements into vars
};
struct pbc_stream { // TODO: add pb_compilemem() later if needed
    FILE* f;
    int line;
    int col;
    int lastc;
};
struct pbc_name {
    char* name;
    uint32_t namecrc;
};
struct pbc_scope {
    struct {
        struct pbc_name* data;
        int len;
        int size;
    } locals;
};
struct pbc {
    struct pbc_stream* input;
    struct {
        struct pbc_scope* data;
        int current;
        int size;
    } scopes;
    struct {
        void* data;
        int len;
        int size;
    } ops;
    struct {
        void* data;
        int len;
        int size;
    } constdata;
    struct {
        struct pbc_name* data;
        int len;
        int size;
    } consts;
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
    struct {
        int* data;
        int len;
        int size;
    } labels;
};

bool pb_compilefile(const char*, struct pbc_opt*, struct script* out, struct charbuf* err);
bool pb_cleanscript(struct script* err);

#endif
