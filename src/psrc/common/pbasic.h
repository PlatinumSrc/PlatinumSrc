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

struct pbc_opt {
    char _placeholder;
};

struct pb_script {
    char _placeholder;
};

#endif
