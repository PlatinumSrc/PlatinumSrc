#ifndef PSRC_COMMON_DATASTREAM_H
#define PSRC_COMMON_DATASTREAM_H

#ifndef PSRC_REUSABLE

#include "../attribs.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

PACKEDENUM ds_mode {
    DS_MODE_MEM,
    DS_MODE_FILE,
    DS_MODE_CB,
    DS_MODE_SECT
};

#define DS_END (-1)

typedef void (*ds_mem_freecb)(void* ctx, void* buf);
typedef bool (*ds_cb_readcb)(void* ctx, void* buf, size_t lenrq, size_t* lenout);
typedef void (*ds_cb_closecb)(void* ctx);

struct datastream {
    uint8_t* buf;
    size_t pos;
    size_t passed;
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
        struct {
            struct datastream* ds;
            size_t lim;
        } sect;
    };
    char* path;
    uint8_t unget : 1;
    uint8_t atend : 1;
    enum ds_mode mode : 6;
    uint8_t last;
};

void ds_openmem(void* buf, size_t sz, ds_mem_freecb freecb, void* freectx, struct datastream*);
bool ds_openfile(const char* path, size_t bufsz, struct datastream*);
bool ds_opencb(ds_cb_readcb readcb, void* readctx, size_t bufsz, ds_cb_closecb closecb, void* closectx, struct datastream*);
bool ds_opensect(struct datastream*, size_t lim, size_t bufsz, struct datastream*);

size_t ds_bin_read(struct datastream*, size_t len, void* outbuf);
static ALWAYSINLINE int ds_bin_getc(struct datastream*);
size_t ds_bin_skip(struct datastream*, size_t len);
static ALWAYSINLINE bool ds_bin_atend(struct datastream*);

int ds_text_getc(struct datastream*);
static inline int ds_text_getc_inline(struct datastream*);
static inline int ds_text_getc_fullinline(struct datastream*);
static ALWAYSINLINE void ds_text_ungetc(struct datastream*, uint8_t);
static ALWAYSINLINE bool ds_text_atend(struct datastream*);

static inline size_t ds_tell(struct datastream*);

void ds_close(struct datastream*);

bool ds__refill(struct datastream*);
int ds_text__getc(struct datastream*);

static ALWAYSINLINE int ds_bin_getc(struct datastream* ds) {
    if (ds->pos == ds->datasz && !ds__refill(ds)) return DS_END;
    return ds->buf[ds->pos++];
}
static ALWAYSINLINE uint8_t ds_bin_getc_noerr(struct datastream* ds) {
    if (ds->pos == ds->datasz && !ds__refill(ds)) return 0;
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
static ALWAYSINLINE void ds_text_ungetc(struct datastream* ds, uint8_t c) {
    ds->unget = 1;
    ds->last = c;
}

static ALWAYSINLINE bool ds_bin_atend(struct datastream* ds) {
    return ds->atend;
}
static ALWAYSINLINE bool ds_text_atend(struct datastream* ds) {
    return (!ds->unget && ds->atend);
}

static inline size_t ds_tell(struct datastream* ds) {
    return ds->passed + ds->pos;
}

#define PSRC_DATASTREAM_T struct datastream*

#else

#include <stdio.h>
#define PSRC_DATASTREAM_T FILE*
#define DS_END EOF
#define ds_bin_read(ds, l, o) fread((o), 1, (l), (ds))
#define ds_bin_getc fgetc
static inline size_t ds_bin_skip(FILE* f, size_t l) {size_t d = ftell(f); fseek(f, l, SEEK_CUR); return ftell(f) - d;}
#define ds_bin_atend feof
#define ds_text_getc fgetc
#define ds_text_getc_inline fgetc
#define ds_text_getc_fullinline fgetc
#define ds_text_ungetc(ds, c) ungetc((c), (ds))
#define ds_text_atend feof

#endif

#endif
