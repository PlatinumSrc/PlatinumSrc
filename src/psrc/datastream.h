#ifndef PSRC_DATASTREAM_H
#define PSRC_DATASTREAM_H

#ifndef PSRC_REUSABLE

#include "attribs.h"

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
struct ds_cbfuncs {
    void* readctx;
    void* seekctx;
    void* closectx;
    bool (*read)(void* ctx, void* buf, size_t lenrq, size_t* lenout);
    bool (*seek)(void* ctx, size_t off);
    void (*close)(void* ctx);
};

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
            #ifndef PSRC_DATASTREAM_USESTDIO
            int fd;
            #else
            FILE* f;
            #endif
        } file;
        struct ds_cbfuncs cb;
        struct {
            struct datastream* ds;
            size_t base;
            size_t lim;
        } sect;
    };
    char* path;
    uint8_t atend : 1;
    enum ds_mode mode : 6;
};

void ds_openmem(void* buf, size_t sz, ds_mem_freecb freecb, void* freectx, struct datastream*);
bool ds_openfile(const char* path, size_t bufsz, struct datastream*);
bool ds_opencb(struct ds_cbfuncs*, size_t bufsz, struct datastream*);
bool ds_opensect(struct datastream*, size_t lim, size_t bufsz, struct datastream*);

size_t ds_read(struct datastream*, size_t len, void* outbuf);
size_t ds_skip(struct datastream*, size_t len);
static ALWAYSINLINE int ds_getc(struct datastream*);
bool ds_seek(struct datastream*, size_t off);
static ALWAYSINLINE size_t ds_tell(struct datastream*);
static ALWAYSINLINE bool ds_atend(struct datastream*);

int ds_text_getc(struct datastream*);
static inline int ds_text_getc_inline(struct datastream*);
static inline int ds_text_getc_fullinline(struct datastream*);

void ds_close(struct datastream*);

int ds_text__getc(struct datastream*);
bool ds__refill(struct datastream*);

static ALWAYSINLINE int ds_getc(struct datastream* ds) {
    if (ds->pos == ds->datasz && !ds__refill(ds)) return DS_END;
    return ds->buf[ds->pos++];
}
static ALWAYSINLINE uint8_t ds_getc_noerr(struct datastream* ds) {
    if (ds->pos == ds->datasz && !ds__refill(ds)) return 0;
    return ds->buf[ds->pos++];
}

static ALWAYSINLINE int ds_text__getc_inline(struct datastream* ds) {
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

static ALWAYSINLINE size_t ds_tell(struct datastream* ds) {
    return ds->passed + ds->pos;
}

static ALWAYSINLINE bool ds_atend(struct datastream* ds) {
    return ds->atend;
}

#define PSRC_DATASTREAM_T struct datastream*

#else

#include <stdio.h>
#define PSRC_DATASTREAM_T FILE*
#define DS_END EOF
static inline size_t ds_read(FILE* f, size_t l, void* o) {return fread(o, 1, l, f);}
static inline size_t ds_skip(FILE* f, size_t l) {size_t d = ftell(f); fseek(f, l, SEEK_CUR); return ftell(f) - d;}
#define ds_getc fgetc
#define ds_seek do {} while (0) /* TODO */
#define ds_atend feof
#define ds_text_getc fgetc
#define ds_text_getc_inline fgetc
#define ds_text_getc_fullinline fgetc

#endif

#endif
