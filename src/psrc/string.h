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

static inline bool cb_init(struct charbuf* b, uintptr_t sz) {
    char* d = malloc(sz);
    if (!d) return false;
    b->data = d;
    b->size = sz;
    b->len = 0;
    return true;
}
static inline bool cb_add(struct charbuf* b, char c) {
    if (b->len == b->size) {
        b->size *= 2;
        char* d = realloc(b->data, b->size);
        if (!d) return false;
        b->data = d;
    }
    b->data[b->len++] = c;
    return true;
}
static inline bool cb_addpartstr(struct charbuf* b, const char* s, uintptr_t l) {
    uintptr_t ol = b->len;
    b->len += l;
    if (b->len > b->size) {
        do {b->size *= 2;} while (b->len > b->size);
        char* d = realloc(b->data, b->size);
        if (!d) return false;
        b->data = d;
    }
    memcpy(b->data + ol, s, l);
    return true;
}
static inline bool cb_addstr(struct charbuf* b, const char* s) {
    return cb_addpartstr(b, s, strlen(s));
}
static inline bool cb_addfake(struct charbuf* b) {
    if (b->len == b->size) {
        b->size *= 2;
        char* d = realloc(b->data, b->size);
        if (!d) return false;
        b->data = d;
    }
    ++b->len;
    return true;
}
static inline bool cb_addmultifake(struct charbuf* b, uintptr_t l) {
    b->len += l;
    if (b->len > b->size) {
        do {b->size *= 2;} while (b->len > b->size);
        char* d = realloc(b->data, b->size);
        if (!d) return false;
        b->data = d;
    }
    return true;
}
static inline char* cb_finalize(struct charbuf* b) {
    char* d = realloc(b->data, b->len + 1);
    if (!d) return NULL;
    b->data = d;
    ((volatile char*)b->data)[b->len] = 0;
    return d;
}
static inline bool cb_reinit(struct charbuf* b, uintptr_t sz, char** o) {
    *o = cb_finalize(b);
    if (!*o) return false;
    return cb_init(b, sz);
}
static inline void cb_dump(struct charbuf* b) {
    free(b->data);
}
static inline void cb_clear(struct charbuf* b) {
    b->len = 0;
}
static inline bool cb_nullterm(struct charbuf* b) {
    if (b->len == b->size) {
        b->size *= 2;
        char* d = realloc(b->data, b->size);
        if (!d) return false;
        b->data = d;
    }
    ((volatile char*)b->data)[b->len] = 0;
    return true;
}
static inline void cb_undo(struct charbuf* b, uintptr_t l) {
    if (l > b->len) {
        b->len = 0;
    } else {
        b->len -= l;
    }
}
static inline char* cb_peek(struct charbuf* b) {
    if (!cb_nullterm(b)) return NULL;
    return b->data;
}

#endif
