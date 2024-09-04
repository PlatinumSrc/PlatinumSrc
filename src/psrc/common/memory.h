#ifndef PSRC_COMMON_MEMORY_H
#define PSRC_COMMON_MEMORY_H

#include "../platform.h"

#include <stdint.h>
#include <string.h>

struct membuf {
    void* data;
    unsigned long len;
    unsigned long size;
};

// NOTE: do not pass in anything less than 2
static inline void mb_init(struct membuf* b, unsigned long sz) {
    b->data = malloc(sz);
    b->size = sz;
    b->len = 0;
}
#define mb__incsize(a) do {\
    b->len += (a);\
    if (b->len > b->size) {\
        do {\
            b->size = b->size * 3 / 2;\
        } while (b->len > b->size);\
        b->data = realloc(b->data, b->size);\
    }\
} while (0)
// TODO: handle misaligned access on non-x86
static inline void mb_put8(struct membuf* b, uint8_t v) {
    uintptr_t p = b->len;
    mb__incsize(1);
    p += (uintptr_t)b->data;
    *(uint8_t*)p = v;
}
static inline void mb_put16(struct membuf* b, uint16_t v) {
    uintptr_t p = b->len;
    mb__incsize(2);
    p += (uintptr_t)b->data;
    *(uint16_t*)p = v;
}
static inline void mb_put32(struct membuf* b, uint32_t v) {
    uintptr_t p = b->len;
    mb__incsize(4);
    p += (uintptr_t)b->data;
    *(uint32_t*)p = v;
}
static inline void mb_put64(struct membuf* b, uint64_t v) {
    uintptr_t p = b->len;
    mb__incsize(8);
    p += (uintptr_t)b->data;
    *(uint64_t*)p = v;
}
static inline void mb_put8at(struct membuf* b, uint8_t v, unsigned long o) {
    *(uint8_t*)((uintptr_t)b->data + o) = v;
}
static inline void mb_put16at(struct membuf* b, uint16_t v, unsigned long o) {
    *(uint16_t*)((uintptr_t)b->data + o) = v;
}
static inline void mb_put32at(struct membuf* b, uint32_t v, unsigned long o) {
    *(uint32_t*)((uintptr_t)b->data + o) = v;
}
static inline void mb_put64at(struct membuf* b, uint64_t v, unsigned long o) {
    *(uint64_t*)((uintptr_t)b->data + o) = v;
}
static inline void mb_put(struct membuf* b, void* d, unsigned long sz) {
    uintptr_t p = b->len;
    mb__incsize(sz);
    p += (uintptr_t)b->data;
    memcpy((void*)p, d, sz);
}
static inline void mb_putz(struct membuf* b, unsigned long sz) {
    uintptr_t p = b->len;
    mb__incsize(sz);
    p += (uintptr_t)b->data;
    memset((void*)p, 0, sz);
}
static inline void mb_putat(struct membuf* b, void* d, unsigned long sz, unsigned long o) {
    memcpy((void*)((uintptr_t)b->data + o), d, sz);
}
static inline void mb_putfake(struct membuf* b, unsigned long sz) {
    mb__incsize(sz);
}
static inline void mb_dump(struct membuf* b) {
    free(b->data);
}

#endif
