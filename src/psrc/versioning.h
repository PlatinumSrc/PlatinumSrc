#ifndef PSRC_VERSIONING_H
#define PSRC_VERSIONING_H

#include "attribs.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

enum vertype {
    VERTYPE_32,
    VERTYPE_16_16,
    VERTYPE_8_16_8,
    VERTYPE_8_8_8_8
};

struct version {
    enum vertype t;
    union {
        struct {
            uint32_t build;
        } v_32;
        struct {
            uint16_t major;
            uint16_t minor;
        } v_16_16;
        struct {
            uint8_t major;
            uint16_t minor;
            uint8_t patch;
        } v_8_16_8;
        struct {
            uint8_t major;
            uint8_t minor;
            uint8_t patch;
            uint8_t build;
        } v_8_8_8_8;
    };
};

#define MKVER_32(b) ((struct version){.t = VERTYPE_32, .v_32 = {(b)}})
#define MKVER_16_16(M, m) ((struct version){.t = VERTYPE_16_16, .v_16_16 = {(M), (m)}})
#define MKVER_8_16_8(M, m, p) ((struct version){.t = VERTYPE_8_16_8, .v_8_16_8 = {(M), (m), (p)}})
#define MKVER_8_8_8_8(M, m, p, b) ((struct version){.t = VERTYPE_8_8_8_8, .v_8_8_8_8 = {(M), (m), (p), (b)}})

static ALWAYSINLINE int vercmp__cmp(const unsigned a[4], const unsigned b[4]) {
    if (a[0] > b[0]) return 1;
    else if (a[0] < b[0]) return -1;
    if (a[1] > b[1]) return 1;
    else if (a[1] < b[1]) return -1;
    if (a[2] > b[2]) return 1;
    else if (a[2] < b[2]) return -1;
    if (a[3] > b[3]) return 1;
    else if (a[3] < b[3]) return -1;
    return 0;
}

static ALWAYSINLINE void vercmp__unpack(const struct version* v, unsigned f[4]) {
    switch (v->t) {
        case VERTYPE_32:
            f[0] = v->v_32.build;
            f[1] = 0;
            f[2] = 0;
            f[3] = 0;
            break;
        case VERTYPE_16_16:
            f[0] = v->v_16_16.major;
            f[1] = v->v_16_16.minor;
            f[2] = 0;
            f[3] = 0;
            break;
        case VERTYPE_8_16_8:
            f[0] = v->v_8_16_8.major;
            f[1] = v->v_8_16_8.minor;
            f[2] = v->v_8_16_8.patch;
            f[3] = 0;
            break;
        case VERTYPE_8_8_8_8:
            f[0] = v->v_8_8_8_8.major;
            f[1] = v->v_8_8_8_8.minor;
            f[2] = v->v_8_8_8_8.patch;
            f[3] = v->v_8_8_8_8.build;
            break;
    }
}

static inline int verstrcmp(const char* sa, const char* sb) {
    unsigned a[4] = {0}, b[4] = {0};
    sscanf(sa, "%u.%u.%u.%u", &a[0], &a[1], &a[2], &a[3]);
    sscanf(sb, "%u.%u.%u.%u", &b[0], &b[1], &b[2], &b[3]);
    return vercmp__cmp(a, b);
}

static inline bool strtover(const char* s, struct version* v) {
    unsigned tmp[4];
    int ct = sscanf(s, "%u.%u.%u.%u", &tmp[0], &tmp[1], &tmp[2], &tmp[3]) - 1;
    switch (ct) {
        default:
            return false;
        case 0:
            v->v_32.build = tmp[0];
            break;
        case 1:
            v->v_16_16.major = tmp[0];
            v->v_16_16.minor = tmp[1];
            break;
        case 2:
            v->v_8_16_8.major = tmp[0];
            v->v_8_16_8.minor = tmp[1];
            v->v_8_16_8.patch = tmp[2];
            break;
        case 3:
            v->v_8_8_8_8.major = tmp[0];
            v->v_8_8_8_8.minor = tmp[1];
            v->v_8_8_8_8.patch = tmp[2];
            v->v_8_8_8_8.build = tmp[3];
            break;
    }
    v->t = ct;
    return true;
}
static inline void vertostr(const struct version* v, char s[16]) {
    switch (v->t) {
        case VERTYPE_32:
            sprintf(s, "%u", (unsigned)v->v_32.build);
            break;
        case VERTYPE_16_16:
            sprintf(s, "%u.%u", v->v_16_16.major, v->v_16_16.minor);
            break;
        case VERTYPE_8_16_8:
            sprintf(
                s, "%u.%u.%u",
                v->v_8_16_8.major,
                v->v_8_16_8.minor,
                v->v_8_16_8.patch
            );
            break;
        case VERTYPE_8_8_8_8:
            sprintf(
                s, "%u.%u.%u.%u",
                v->v_8_8_8_8.major,
                v->v_8_8_8_8.minor,
                v->v_8_8_8_8.patch,
                v->v_8_8_8_8.build
            );
            break;
    }
}
static inline char* vertostrdup(const struct version* v) {
    char s[16];
    vertostr(v, s);
    return strdup(s);
}

static inline int vercmp(const struct version* a, const struct version* b) {
    unsigned ua[4], ub[4];
    vercmp__unpack(a, ua);
    vercmp__unpack(b, ub);
    return vercmp__cmp(ua, ub);
}

#endif
