#ifndef PSRC_EDITOR_PMP_H
#define PSRC_EDITOR_PMP_H

#include "../string.h"
#include "../attribs.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

struct pmp_read {
    FILE* f;
    uint8_t istext : 1;
    uint8_t lastwasvar : 1;
    uint8_t : 6;
    long skipsz;
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

PACKEDENUM pmp_type {
    PMP_TYPE_GROUP,
    PMP_TYPE_ENDGROUP,
    PMP_TYPE_BOOL,
    PMP_TYPE_I8,
    PMP_TYPE_I16,
    PMP_TYPE_I32,
    PMP_TYPE_I64,
    PMP_TYPE_U8,
    PMP_TYPE_U16,
    PMP_TYPE_U32,
    PMP_TYPE_U64,
    PMP_TYPE_F32,
    PMP_TYPE_F64,
    PMP_TYPE_STR
};
struct pmp_vartype {
    enum pmp_vartype type : 7;
    uint8_t isarray : 1;
    uint32_t size;
}

bool pmp_read_open(char* p, struct pmp_read*);
bool pmp_read_next(struct pmp_read*, struct charbuf* name, struct pmp_vartype*);
void* pmp_read_readvar(struct pmp_read*);
void pmp_read_close(struct pmp_read*);

bool pmp_write_open(char* p, struct pmp_write*, bool text, enum pmp_write_comp);
void pmp_write_next(struct pmp_write*, const char* name, long namelen, struct pmp_vartype*, void* data);
void pmp_write_close(struct pmp_write*);

#endif
