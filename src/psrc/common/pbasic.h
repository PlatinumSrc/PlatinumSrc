#ifndef PSRC_COMMON_PBASIC_H
#define PSRC_COMMON_PBASIC_H

#include "../string.h"
#include "../memory.h"
#include "../threading.h"
#include "../attribs.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

PACKEDENUM pbtype {
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
struct pbdata {
    enum pbtype type;
    uint32_t dim;
    union {
        uintptr_t size;
        uintptr_t* sizes;
    };
    union {
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
        } array;
    } data;
};

struct pbc_opt {
    char _placeholder;
};

struct pbscript_string {
    uintptr_t len;
    char data[];
};
struct pbscript_const {
    enum pbtype type;
    uint32_t dim;
    union {
        uintptr_t size;
        uintptr_t* sizes;
    };
    union {
        uint8_t b;
        int8_t i8;
        int16_t i16;
        int32_t i32;
        int64_t* i64;
        uint8_t u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t* u64;
        float f32;
        double* f64;
        float* vec;
        struct pbscript_string* str;
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
                float* vec;
                struct pbscript_string* str;
            };
        } array;
    } data;
};
struct pbscript {
    const uint8_t* program;
    const char* names;
    struct {
        uint8_t* data;
        struct pbscript_const* values;
        uintptr_t* sizes;
        //unsigned count;
    } consts;
    struct {
        const char** names;
        uint8_t* data;
        struct pbscript_const* values;
        uintptr_t* sizes;
    } globals;
    struct {
        const char** names;
    } subs;
};

#endif
