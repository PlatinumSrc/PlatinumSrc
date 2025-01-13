#ifndef PSRC_MEMORY_H
#define PSRC_MEMORY_H

#include "platform.h"

#include <stdint.h>
#include <string.h>

#include "vlb.h"

struct membuf VLB(void);

// NOTE: do not pass in anything less than 2
static inline void mb_init(struct membuf* b, uintptr_t sz) {
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
static inline void mb_put8at(struct membuf* b, uint8_t v, uintptr_t o) {
    *(uint8_t*)((uintptr_t)b->data + o) = v;
}
static inline void mb_put16at(struct membuf* b, uint16_t v, uintptr_t o) {
    *(uint16_t*)((uintptr_t)b->data + o) = v;
}
static inline void mb_put32at(struct membuf* b, uint32_t v, uintptr_t o) {
    *(uint32_t*)((uintptr_t)b->data + o) = v;
}
static inline void mb_put64at(struct membuf* b, uint64_t v, uintptr_t o) {
    *(uint64_t*)((uintptr_t)b->data + o) = v;
}
static inline void mb_put(struct membuf* b, void* d, uintptr_t sz) {
    uintptr_t p = b->len;
    mb__incsize(sz);
    p += (uintptr_t)b->data;
    memcpy((void*)p, d, sz);
}
static inline void mb_putz(struct membuf* b, uintptr_t sz) {
    uintptr_t p = b->len;
    mb__incsize(sz);
    p += (uintptr_t)b->data;
    memset((void*)p, 0, sz);
}
static inline void mb_putat(struct membuf* b, void* d, uintptr_t sz, uintptr_t o) {
    memcpy((void*)((uintptr_t)b->data + o), d, sz);
}
static inline void mb_putfake(struct membuf* b, uintptr_t sz) {
    mb__incsize(sz);
}
static inline void mb_dump(struct membuf* b) {
    free(b->data);
}

#endif
