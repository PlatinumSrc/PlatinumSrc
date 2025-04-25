#ifndef PSRC_COMMON_WORLD_H
#define PSRC_COMMON_WORLD_H

#include <stdint.h>

extern struct worldinfo {
    unsigned chunksize;
} worldinfo;

struct worldcoord {
    uint32_t chunk; // <8 bits: Y> <12 bits: X> <12 bits: Z>
    float pos[3];
};
#define WORLDCOORD(cx, cy, cz, x, y, z) ((struct worldcoord){\
    .chunk = (((cy) & 0xFF) << 24) | (((cx) & 0xFFF) << 12) | ((cz) & 0xFFF),\
    .pos = {(x), (y), (z)}\
})

static inline void wc_addwc(struct worldcoord* to, const struct worldcoord* a) {
    to->pos[0] += a->pos[0];
    to->pos[1] += a->pos[1];
    to->pos[2] += a->pos[2];
}
static inline void wc_subwc(struct worldcoord* from, const struct worldcoord* a) {
    from->pos[0] -= a->pos[0];
    from->pos[1] -= a->pos[1];
    from->pos[2] -= a->pos[2];
}
static inline void wc_add(struct worldcoord* to, const float a[3]) {
    to->pos[0] += a[0];
    to->pos[1] += a[1];
    to->pos[2] += a[2];
}
static inline void wc_sub(struct worldcoord* from, const float a[3]) {
    from->pos[0] -= a[0];
    from->pos[1] -= a[1];
    from->pos[2] -= a[2];
}
static inline void wc_diff(const struct worldcoord* a, const struct worldcoord* b, float o[3]) {
    o[0] = b->pos[0] - a->pos[0];
    o[1] = b->pos[1] - a->pos[1];
    o[2] = b->pos[2] - a->pos[2];
}

#endif
