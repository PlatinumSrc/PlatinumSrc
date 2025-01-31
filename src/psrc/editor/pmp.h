#ifndef PSRC_EDITOR_PMP_H
#define PSRC_EDITOR_PMP_H

#include "../string.h"
#include "../attribs.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define PMP_VER 0

PACKEDENUM pmp_type {
    PMP_TYPE_GROUP,
    PMP_TYPE_ENDGROUP,
    PMP_TYPE__VAR,
    PMP_TYPE_BOOL = PMP_TYPE__VAR,  // bool / uint8_t
    PMP_TYPE_I8,    // int8_t
    PMP_TYPE_I16,   // int16_t
    PMP_TYPE_I32,   // int32_t
    PMP_TYPE_I64,   // int64_t
    PMP_TYPE_U8,    // uint8_t
    PMP_TYPE_U16,   // uint16_t
    PMP_TYPE_U32,   // uint32_t
    PMP_TYPE_U64,   // uint64_t
    PMP_TYPE_F32,   // float
    PMP_TYPE_F64,   // double
    PMP_TYPE_STR,   // struct pmp_string
    PMP_TYPE__COUNT
};
struct pmp_vartype {
    enum pmp_type type : 7;
    uint8_t isarray : 1;
    uint32_t size;
};

struct pmp_string {
    char* data;
    uint32_t len;
};

struct pmp_read {
    FILE* f;
    uint8_t istext : 1;
    uint8_t lastwasvar : 1;
    uint8_t : 6;
    struct pmp_vartype lasttype;
};

enum pmp_write_comp {
    PMP_WRITE_COMP_NONE,
    PMP_WRITE_COMP_GIT,
    PMP_WRITE_COMP_FULL,
};
struct pmp_write {
    FILE* f;
    uint8_t istext : 1;
    uint8_t : 7;
    enum pmp_write_comp comp;
};

bool pmp_read_open(char* p, bool text, struct pmp_read*);
bool pmp_read_next(struct pmp_read*, struct charbuf* name, struct pmp_vartype*);
void pmp_read_readvar(struct pmp_read*, void*); // type* if not array, type** if array
static inline void pmp_read_freevar(struct pmp_vartype*, void*); // use with arrays and strings
void pmp_read_close(struct pmp_read*);

bool pmp_write_open(char* p, struct pmp_write*, bool text, enum pmp_write_comp);
void pmp_write_next(struct pmp_write*, const char* name, uint32_t namelen, const struct pmp_vartype*, const void* data); // type* if not array, type* if array
void pmp_write_close(struct pmp_write*);

static inline void pmp_read_freevar(struct pmp_vartype* t, void* d) {
    if (t->type == PMP_TYPE_STR) {
        if (!t->isarray) {
            free(((struct pmp_string*)d)->data);
        } else {
            struct pmp_string* a = *(struct pmp_string**)d;
            uint32_t sz = t->size;
            for (uint32_t i = 0; i < sz; ++i) {
                free(a[i].data);
            }
            free(a);
        }
    } else if (t->isarray) {
        free(*(void**)d);
    }
}

#endif
