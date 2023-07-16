#ifndef AUX_STRING_H
#define AUX_STRING_H

#include <stdlib.h>

struct charbuf {
    char* data;
    int size;
    int len;
};

char* strcombine(char*, ...);

static inline void cb_init(struct charbuf* b, int sz) {
    if (sz < 1) sz = 1;
    b->data = malloc(sz);
    b->size = sz;
    b->len = 0;
}
static inline void cb_add(struct charbuf* b, char c) {
    b->data[b->len++] = c;
    if (b->len == b->size) {
        b->size *= 2;
        b->data = realloc(b->data, b->size);
    }
}
static inline void cb_addstr(struct charbuf* b, char* s) {
    // TODO: make it so that this function instead will see how much space is
    //       left before a realloc and manually copy up to that amount of chars
    //       before performing a realloc
    while(*s) cb_add(b, *(s++));
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
}
static inline void cb_clear(struct charbuf* b, int sz) {
    cb_dump(b);
    cb_init(b, sz);
}

#endif
