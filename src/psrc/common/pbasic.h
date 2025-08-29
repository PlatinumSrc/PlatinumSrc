#ifndef PSRC_COMMON_PBASIC_H
#define PSRC_COMMON_PBASIC_H

#include "../string.h"
#include "../threading.h"
#include "../attribs.h"

#include <stdint.h>
#include <stdbool.h>

PACKEDENUM pb_type {
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
    PBTYPE_STR
};

struct pb_script {
    int placeholder;
};

struct pb_compopt {
    int placeholder;
};

#endif
