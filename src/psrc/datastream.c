#include "datastream.h"
#include "filesystem.h"
#include "logging.h"
#include "debug.h"
#include "platform.h"

#include <string.h>
#include <stdlib.h>
#if defined(PSRC_DATASTREAM_USESTDIO)
    #include <stdio.h>
#elif !defined(PSRC_DATASTREAM_USESDL)
    #include <fcntl.h>
    #include <unistd.h>
    #ifndef O_BINARY
        #define O_BINARY 0
    #endif
#endif

void ds_openmem(const void* b, size_t sz, const char* n, bool fn, ds_mem_freecb freecb, void* freectx, struct datastream* ds) {
    ds->buf = (uint8_t*)b;
    ds->pos = 0;
    ds->passed = 0;
    ds->datasz = sz;
    ds->mem.free = freecb;
    ds->mem.freectx = freectx;
    ds->name = (n) ? n : "<memory>";
    ds->freename = fn;
    ds->atend = 0;
    ds->type = DS_TYPE_MEM;
}
bool ds_openfile(const char* p, const char* n, bool fn, size_t bufsz, struct datastream* ds) {
    {
        int tmp = isFile(p);
        if (tmp < 1) {
            if (tmp) {
                #if DEBUG(1)
                plog(LL_ERROR | LF_DEBUG | LF_FUNC, LE_NOEXIST(p));
                #endif
            } else {
                plog(LL_ERROR | LF_FUNC, LE_ISDIR(p));
            }
            return false;
        }
    }
    #if defined(PSRC_DATASTREAM_USESTDIO)
        if (!(ds->file.f = fopen(p, "rb"))) {
            plog(LL_WARN | LF_FUNC, LE_CANTOPEN(p, errno));
            return false;
        }
    #elif defined(PSRC_DATASTREAM_USESDL)
//        #if PLATFORM == PLAT_ANDROID
//            if (p[0] == '.' && p[1] == '/') p += 2; // Skip ./ on Android so that SDL can correctly search in the embedded assets
//        #endif
        if (!(ds->file.rwo = SDL_RWFromFile(p, "rb"))) {
            plog(LL_WARN | LF_FUNC, LE_CANTOPEN(p, errno));
            return false;
        }
    #else
        if ((ds->file.fd = open(p, O_RDONLY | O_BINARY, 0)) < 0) {
            plog(LL_WARN | LF_FUNC, LE_CANTOPEN(p, errno));
            return false;
        }
    #endif
    if (!bufsz) bufsz = 4096; // 8 HDD sectors or 2 CD sectors
    ds->buf = malloc(bufsz);
    if (!ds->buf) {
        #if defined(PSRC_DATASTREAM_USESTDIO)
            fclose(ds->file.f);
        #elif defined(PSRC_DATASTREAM_USESDL)
            SDL_RWclose(ds->file.rwo);
        #else
            close(ds->file.fd);
        #endif
        return false;
    }
    ds->pos = 0;
    ds->passed = 0;
    ds->datasz = 0;
    ds->bufsz = bufsz;
    if (n) {
        ds->name = n;
        ds->freename = fn;
    } else {
        ds->name = strpath(p);
        ds->freename = 1;
    }
    ds->atend = 0;
    ds->type = DS_TYPE_FILE;
    return true;
}
bool ds_opencb(struct ds_cb_funcs* funcs, const char* n, bool fn, size_t bufsz, struct datastream* ds) {
    if (!bufsz) bufsz = 4096;
    ds->buf = malloc(bufsz);
    if (!ds->buf) return false;
    ds->pos = 0;
    ds->passed = 0;
    ds->datasz = 0;
    ds->bufsz = bufsz;
    ds->cb = *funcs;
    ds->name = (n) ? n : "<callback>";
    ds->freename = fn;
    ds->atend = 0;
    ds->type = DS_TYPE_CB;
    return true;
}
bool ds_opensect(struct datastream* ids, size_t lim, const char* n, bool fn, size_t bufsz, struct datastream* ds) {
    if (!bufsz) bufsz = 256;
    ds->buf = malloc(bufsz);
    if (!ds->buf) return false;
    ds->pos = 0;
    ds->passed = 0;
    ds->datasz = 0;
    ds->bufsz = bufsz;
    ds->sect.ds = ids;
    ds->sect.base = ds_tell(ids);
    ds->sect.lim = lim;
    ds->name = (n) ? n : ids->name;
    ds->freename = fn;
    ds->atend = 0;
    ds->type = DS_TYPE_SECT;
    return true;
}

void ds_close(struct datastream* ds) {
    switch (ds->type) {
        case DS_TYPE_MEM:
            if (ds->mem.free) ds->mem.free(ds->mem.freectx, ds->buf);
            break;
        case DS_TYPE_FILE:
            free(ds->buf);
            #if defined(PSRC_DATASTREAM_USESTDIO)
                fclose(ds->file.f);
            #elif defined(PSRC_DATASTREAM_USESDL)
                SDL_RWclose(ds->file.rwo);
            #else
                close(ds->file.fd);
            #endif
            break;
        case DS_TYPE_CB:
            free(ds->buf);
            if (ds->cb.close) ds->cb.close(ds->cb.ctx);
            break;
        case DS_TYPE_SECT:
            free(ds->buf);
            break;
    }
    if (ds->freename) free((char*)ds->name);
}

int ds_text_getc(struct datastream* ds) {
    int c;
    do {
        c = ds_getc(ds);
    } while (c == '\r' || !c);
    return c;
}

size_t ds_read(struct datastream* ds, size_t l, void* b) {
    if (!l) return 0;
    size_t a = ds->datasz - ds->pos;
    if (!a) {
        if (!ds__refill(ds)) return 0;
        a = ds->datasz/* - ds->pos*/;
        if (!a) return 0;
    }
    size_t r = 0;
    while (a < l) {
        memcpy((uint8_t*)b + r, ds->buf + ds->pos, a);
        r += a;
        l -= a;
        if (!ds__refill(ds)) return r;
        a = ds->datasz/* - ds->pos*/;
        if (!a) return r;
    }
    memcpy((uint8_t*)b + r, ds->buf + ds->pos, l);
    ds->pos += l;
    return r + l;
}
size_t ds_skip(struct datastream* ds, size_t l) {
    if (!l) return 0;
    size_t r = 0;
    size_t a = ds->datasz - ds->pos;
    if (!a) {
        if (!ds__refill(ds)) return r;
        a = ds->datasz/* - ds->pos*/;
        if (!a) return r;
    }
    while (a < l) {
        r += a;
        l -= a;
        if (!ds__refill(ds)) return r;
        a = ds->datasz/* - ds->pos*/;
        if (!a) return r;
    }
    ds->pos += l;
    return r + l;
}
void* ds_readchunk(struct datastream* ds, size_t* lout) {
    size_t a = ds->datasz - ds->pos;
    if (!a) {
        if (!ds__refill(ds)) return NULL;
        a = ds->datasz/* - ds->pos*/;
        if (!a) return NULL;
    }
    *lout = a;
    a = ds->pos;
    ds->pos = ds->datasz;
    return ds->buf + a;
}

bool ds_seek(struct datastream* ds, size_t o) {
    switch (ds->type) {
        case DS_TYPE_MEM: {
            if (o >= ds->datasz) return false;
            ds->pos = o;
            return true;
        } break;
        case DS_TYPE_FILE: {
            #if defined(PSRC_DATASTREAM_USESTDIO)
                if (fseek(ds->file.f, o, SEEK_SET) == -1) return false;
            #elif defined(PSRC_DATASTREAM_USESDL)
                if (SDL_RWseek(ds->file.rwo, o, SEEK_SET) == -1) return false;
            #else
                if (lseek(ds->file.fd, o, SEEK_SET) == -1) return false;
            #endif
        } break;
        case DS_TYPE_CB: {
            if (!ds->cb.seek) return false;
            if (!ds->cb.seek(ds->cb.ctx, o)) return false;
        } break;
        case DS_TYPE_SECT: {
            if (o >= ds->sect.lim) return false;
            if (!ds_seek(ds->sect.ds, ds->sect.base + o)) return false;
        } break;
    }
    ds->passed = o;
    ds->pos = 0;
    ds->datasz = 0;
    return true;
}

size_t ds_getsz(struct datastream* ds) {
    switch (ds->type) {
        case DS_TYPE_MEM: {
            return ds->datasz;
        } break;
        case DS_TYPE_FILE: {
            #if defined(PSRC_DATASTREAM_USESTDIO)
                long c = ftell(ds->file.f);
                if (c == -1) break;
                fseek(ds->file.fd, 0, SEEK_END);
                long e = ftell(ds->file.f);
                fseek(ds->file.fd, c, SEEK_SET);
                return e;
            #elif defined(PSRC_DATASTREAM_USESDL)
                int64_t sz = SDL_RWsize(ds->file.rwo);
                if (sz < 0) break;
                return sz;
            #else
                off_t c = lseek(ds->file.fd, 0, SEEK_CUR);
                if (c == -1) break;
                off_t e = lseek(ds->file.fd, 0, SEEK_END);
                lseek(ds->file.fd, c, SEEK_SET);
                return e;
            #endif
        } break;
        case DS_TYPE_CB: {
            if (!ds->cb.getsz) break;
            return ds->cb.getsz(ds->cb.ctx);
        } break;
        case DS_TYPE_SECT: {
            return ds->sect.lim;
        } break;
    }
    return -1;
}

int ds_text__getc(struct datastream* ds) {
    return ds_getc(ds);
}

bool ds__refill(struct datastream* ds) {
    if (ds->atend) return false;
    if (ds->type == DS_TYPE_MEM) {
        ds->atend = 1;
        return false;
    }
    ds->passed += ds->datasz;
    ds->pos = 0;
    if (ds->type == DS_TYPE_FILE) {
        #if defined(PSRC_DATASTREAM_USESTDIO)
            if (feof(ds->file.f)) {
                ds->datasz = 0;
                ds->atend = 1;
                return false;
            }
            ds->datasz = fread(ds->buf, 1, ds->bufsz, ds->file.f);
        #elif defined(PSRC_DATASTREAM_USESDL)
            size_t r = SDL_RWread(ds->file.rwo, ds->buf, 1, ds->bufsz);
            if (!r) {
                ds->datasz = 0;
                ds->atend = 1;
                return false;
            }
            ds->datasz = r;
        #else
            ssize_t r = read(ds->file.fd, ds->buf, ds->bufsz);
            if (!r || r == -1) {
                ds->datasz = 0;
                ds->atend = 1;
                return false;
            }
            ds->datasz = r;
        #endif
    } else if (ds->type == DS_TYPE_CB) {
        if (!ds->cb.read(ds->cb.ctx, ds->buf, ds->bufsz, &ds->datasz)) {
            ds->datasz = 0;
            ds->atend = 1;
            return false;
        }
    } else {
        size_t tmp = ds->sect.lim - ds->passed;
        if (!tmp) {
            ds->datasz = 0;
            ds->atend = 1;
            return false;
        }
        if (tmp > ds->bufsz) tmp = ds->bufsz;
        size_t r = ds_read(ds->sect.ds, tmp, ds->buf);
        if (!r && ds_atend(ds->sect.ds)) {
            ds->datasz = 0;
            ds->atend = 1;
            return false;
        }
        ds->datasz = r;
    }
    return true;
}
