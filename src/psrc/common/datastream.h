#ifndef PSRC_COMMON_DATASTREAM_H
#define PSRC_COMMON_DATASTREAM_H

#include "../attribs.h"
#include "../platform.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

PACKEDENUM ds_mode {
    DS_MODE_MEM,
    DS_MODE_FILE,
    DS_MODE_CB
};

#define DS_END (-1)

typedef void (*ds_mem_freecb)(void* ctx, void* buf);
typedef bool (*ds_cb_readcb)(void* ctx, void* buf, size_t lenrq, size_t* lenout);
typedef void (*ds_cb_closecb)(void* ctx);

struct datastream {
    uint8_t* buf;
    size_t pos;
    size_t datasz;
    size_t bufsz;
    union {
        struct {
            ds_mem_freecb free;
            void* freectx;
        } mem;
        struct {
            #ifndef PSRC_COMMON_DATASTREAM_USESTDIO
            int fd;
            #else
            FILE* f;
            #endif
        } file;
        struct {
            ds_cb_readcb read;
            ds_cb_closecb close;
            void* readctx;
            void* closectx;
        } cb;
    };
    char* path;
    uint8_t unget : 1;
    uint8_t atend : 1;
    enum ds_mode mode : 6;
    uint8_t last;
};

void ds_openmem(void* buf, size_t sz, ds_mem_freecb freecb, void* freectx, struct datastream*);
bool ds_openfile(const char* path, size_t bufsz, struct datastream*);
void ds_opencb(ds_cb_readcb readcb, void* readctx, size_t bufsz, ds_cb_closecb closecb, void* closectx, struct datastream*);
size_t ds_bin_read(struct datastream*, void* outbuf, size_t len);
int ds_text_getc(struct datastream*);
void ds_close(struct datastream*);

bool ds__refill(struct datastream*);
int ds_text__getc(struct datastream*);

static inline int ds_bin_getc(struct datastream* ds) {
    if (ds->pos == ds->datasz && !ds__refill(ds)) return DS_END;
    return ds->buf[ds->pos++];
}

static ALWAYSINLINE int ds_text__getc_inline(struct datastream* ds) {
    if (ds->unget) {
        ds->unget = 0;
        return ds->last;
    }
    if (ds->pos == ds->datasz && !ds__refill(ds)) return DS_END;
    return ds->buf[ds->pos++];
}
static inline int ds_text_getc_inline(struct datastream* ds) {
    int c;
    do {
        c = ds_text__getc(ds);
    } while (c == '\r' || !c);
    return c;
}
static inline int ds_text_getc_fullinline(struct datastream* ds) {
    int c;
    do {
        c = ds_text__getc_inline(ds);
    } while (c == '\r' || !c);
    return c;
}
static inline void ds_text_ungetc(struct datastream* ds, uint8_t c) {
    ds->unget = 1;
    ds->last = c;
}

static inline bool ds_atend(struct datastream* ds) {
    return (!ds->unget && ds->atend);
}

#endif
