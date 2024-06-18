#ifndef PSRC_COMMON_MEMORY_H
#define PSRC_COMMON_MEMORY_H

struct membuf {
    void* data;
    int len;
    int size;
};

static inline void mb_init(struct membuf* b, int sz) {
    b->data = malloc(sz);
    b->size = sz;
    b->len = 0;
}

#endif
