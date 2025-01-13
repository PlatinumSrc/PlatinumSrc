#ifndef PSRC_STRING_H
#define PSRC_STRING_H

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "vlb.h"

struct charbuf VLB(char);

char* strcombine(const char*, ...);
char** splitstrlist(const char*, char delim, bool nullterm, int* len);
char** splitstr(const char*, const char* delims, bool nullterm, int* len);
char* makestrlist(const char* const* str, int len, char delim);
int strbool(const char*, int);
uint64_t strsec(const char*, uint64_t);

static inline void cb_init(struct charbuf* b, uintptr_t sz) {
    b->data = malloc(sz);
    b->size = sz;
    b->len = 0;
}
static inline void cb_add(struct charbuf* b, char c) {
    if (b->len == b->size) {
        b->size *= 2;
        b->data = realloc(b->data, b->size);
    }
    b->data[b->len++] = c;
}
static inline void cb_addpartstr(struct charbuf* b, const char* s, uintptr_t l) {
    uintptr_t ol = b->len;
    b->len += l;
    if (b->len > b->size) {
        do {b->size *= 2;} while (b->len > b->size);
        b->data = realloc(b->data, b->size);
    }
    memcpy(b->data + ol, s, l);
}
static inline void cb_addstr(struct charbuf* b, const char* s) {
    cb_addpartstr(b, s, strlen(s));
}
static inline void cb_addfake(struct charbuf* b) {
    if (b->len == b->size) {
        b->size *= 2;
        b->data = realloc(b->data, b->size);
    }
    ++b->len;
}
static inline void cb_addmultifake(struct charbuf* b, uintptr_t l) {
    b->len += l;
    if (b->len > b->size) {
        do {b->size *= 2;} while (b->len > b->size);
        b->data = realloc(b->data, b->size);
    }
}
static inline char* cb_finalize(struct charbuf* b) {
    b->data = realloc(b->data, b->len + 1);
    ((volatile char*)b->data)[b->len] = 0;
    return b->data;
}
static inline char* cb_reinit(struct charbuf* b, uintptr_t sz) {
    char* d = cb_finalize(b);
    cb_init(b, sz);
    return d;
}
static inline void cb_dump(struct charbuf* b) {
    free(b->data);
}
static inline void cb_reset(struct charbuf* b, uintptr_t sz) {
    cb_dump(b);
    cb_init(b, sz);
}
static inline void cb_clear(struct charbuf* b) {
    b->len = 0;
}
static inline void cb_nullterm(struct charbuf* b) {
    if (b->len == b->size) {
        b->size *= 2;
        b->data = realloc(b->data, b->size);
    }
    ((volatile char*)b->data)[b->len] = 0;
}
static inline void cb_undo(struct charbuf* b, uintptr_t l) {
    if (l > b->len) {
        b->len = 0;
    } else {
        b->len -= l;
    }
}
static inline char* cb_peek(struct charbuf* b) {
    cb_nullterm(b);
    return b->data;
}

#endif
