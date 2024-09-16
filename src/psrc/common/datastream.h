#ifndef PSRC_COMMON_DATASTREAM_H
#define PSRC_COMMON_DATASTREAM_H

#include "../attribs.h"
#include "../platform.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#if (PLATFLAGS & PLATFLAG_UNIXLIKE) || PLATFORM == PLAT_DREAMCAST
    #include <fcntl.h>
    #include <unistd.h>
#endif

PACKEDENUM ds_mode {
    DS_MODE_MEM,
    DS_MODE_FILE,
    DS_MODE_CB
};

typedef bool (*ds_callback)(void* ctx, void* buf, size_t lenrq, size_t* lenout);

struct datastream {
    uint8_t* buf;
    size_t pos;
    size_t datasz;
    size_t bufsz;
    union {
        struct {
            size_t bufpos;
            #if (PLATFLAGS & PLATFLAG_UNIXLIKE) || PLATFORM == PLAT_DREAMCAST
            int fd;
            #else
            FILE* f;
            #endif
            uint8_t last;
        };
        struct {
            ds_callback cb;
            void* ctx;
        };
    };
    uint8_t atend : 1;
    uint8_t unget : 1;
    enum ds_mode mode : 6;
};

#define DS_OPENFILE_BINARY (1 << 0)

size_t ds_read(struct datastream* ds, void* b, size_t l);
bool ds__refill_internal(struct datastream* ds);

static inline void ds_openmem(struct datastream* ds, void* b, size_t sz) {
    ds->buf = b;
    ds->pos = 0;
    ds->datasz = sz;
    ds->atend = 0;
    ds->unget = 0;
    ds->mode = DS_MODE_MEM;
}
static inline bool ds_openfile(struct datastream* ds, const char* p, unsigned f, size_t bufsz) {
    #if (PLATFLAGS & PLATFLAG_UNIXLIKE) || PLATFORM == PLAT_DREAMCAST
    if ((ds->fd = open(p, O_RDONLY, 0)) < 0) return false;
    #else
    ds->f = fopen(p, (f & DS_OPENFILE_BINARY) ? "rb" : "r");
    if (!ds->f) return false;
    #endif
    if (!bufsz) {
        #if PLATFORM == PLAT_DREAMCAST
        bufsz = 2048 * 4;
        #else
        bufsz = 512 * 8;
        #endif
    }
    ds->buf = malloc(bufsz);
    ds->pos = 0;
    ds->datasz = 0;
    ds->bufsz = bufsz;
    ds->atend = 0;
    ds->unget = 0;
    ds->mode = DS_MODE_FILE;
    return true;
}
static inline void ds_opencb(struct datastream* ds, ds_callback cb, void* ctx, size_t bufsz) {
    if (!bufsz) bufsz = 4096;
    ds->buf = malloc(bufsz);
    ds->pos = 0;
    ds->datasz = 0;
    ds->bufsz = bufsz;
    ds->cb = cb;
    ds->ctx = ctx;
    ds->atend = 0;
    ds->unget = 0;
    ds->mode = DS_MODE_CB;
}

static ALWAYSINLINE bool ds__refill(struct datastream* ds) {
    if (ds->mode == DS_MODE_MEM) return false;
    return ds__refill_internal(ds);
}

static inline int ds_getc(struct datastream* ds) {
    if (!ds->unget) {
        ds->unget = 0;
        return ds->last;
    }
    if (ds->atend) return EOF;
    if (ds->pos >= ds->datasz && !ds__refill(ds)) return EOF;
    char c = ds->buf[ds->pos++];
    ds->last = c;
    return c;
}
static inline void ds_ungetc(struct datastream* ds, int c) {
    ds->last = c;
    ds->unget = 1;
}

#endif
