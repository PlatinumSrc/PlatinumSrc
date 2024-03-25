#ifndef PSRC_COMMON_STRING_H
#define PSRC_COMMON_STRING_H

#include <stdlib.h>
#include <stdbool.h>

struct charbuf {
    char* data;
    int len;
    int size;
};

char* strcombine(const char*, ...);
char** splitstrlist(const char* str, char delim, bool nullterm, int* len);
char* makestrlist(const char* const* str, int len, char delim);
int strbool(const char*, int);

static inline void cb_init(struct charbuf* b, int sz) {
    if (sz < 1) sz = 1;
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
static inline void cb_addstr(struct charbuf* b, const char* s) {
    while(*s) cb_add(b, *(s++));
}
static inline void cb_addpartstr(struct charbuf* b, const char* s, long l) {
    // TODO: make it so that this function instead will see how much space is left before a realloc and manually copy
    //       up to that amount of chars before performing a realloc
    for (long i = 0; i < l; ++i) {
        cb_add(b, *(s++));
    }
}
static inline char* cb_finalize(struct charbuf* b) {
    b->data = realloc(b->data, b->len + 1);
    b->data[b->len] = 0;
    char* d = b->data;
    b->data = NULL;
    return d;
}
static inline char* cb_reinit(struct charbuf* b, int sz) {
    char* d = cb_finalize(b);
    cb_init(b, sz);
    return d;
}
static inline void cb_dump(struct charbuf* b) {
    free(b->data);
    b->data = NULL;
}
static inline void cb_reset(struct charbuf* b, int sz) {
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
    b->data[b->len] = 0;
}
static inline void cb_undo(struct charbuf* b, int l) {
    b->len -= l;
    if (b->len < 0) b->len = 0;
}
static inline char* cb_peek(struct charbuf* b) {
    cb_nullterm(b);
    return b->data;
}

#endif
