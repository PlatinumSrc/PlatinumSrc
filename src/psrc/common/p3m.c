#include "p3m.h"

#include "../debug.h"

#include "../utils/filesystem.h"
#include "../utils/logging.h"
#include "../utils/byteorder.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define P3M_FILEFLAG_HASANIMATIONS (1 << 0)

static inline bool get8(FILE* f, uint8_t* d) {
    int tmp = fgetc(f);
    if (tmp == EOF) return false;
    *d = tmp;
    return true;
}
static inline bool get16(FILE* f, uint16_t* d) {
    size_t tmp = fread(d, sizeof(*d), 1, f);
    if (tmp < sizeof(*d)) return false;
    swapinplacele16(d);
    return true;
}
static inline bool get32(FILE* f, uint32_t* d) {
    size_t tmp = fread(d, sizeof(*d), 1, f);
    if (tmp < sizeof(*d)) return false;
    swapinplacele32(d);
    return true;
}

struct p3m* p3m_loadfile(const char* p) {
    FILE* f;
    {
        int tmp = isFile(p);
        if (tmp < 1) {
            #if DEBUG(1)
            int e = (tmp) ? ENOENT : EISDIR;
            plog(LL_ERROR | LF_DEBUG | LF_FUNC, LE_CANTOPEN(p, e));
            #endif
            return NULL;
        }
        f = fopen(p, "rb");
        if (!f) {
            plog(LL_WARN | LF_FUNC, LE_CANTOPEN(p, errno));
            return NULL;
        }
    }
    struct p3m* d = malloc(sizeof(*d));
    d->data = NULL;
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG | LF_FUNC, "Checking header...");
    #endif
    {
        char m[4];
        fread(m, 1, 4, f);
        if (m[0] != 'P' || m[1] != '3' || m[2] != 'M' || m[3] != 0) {
            plog(
                LL_ERROR | LF_FUNC,
                "P3M header magic wrong (%02X %02X %02X %02X != 50 33 4D 00)",
                m[0], m[1], m[2], m[3]
            );
            goto retnull;
        }
    }
    {
        uint8_t v[2] = {fgetc(f), fgetc(f)};
        if (v[0] != P3M_VER_MAJOR || v[1] != P3M_VER_MINOR) {
            plog(LL_ERROR | LF_FUNC, "P3M version wrong (%d.%d != 1.0)", v[0], v[1]);
            goto retnull;
        }
    }
    fseek(f, -1, SEEK_END);
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG | LF_FUNC, "Checking string table...");
    #endif
    if (fgetc(f) != 0) {
        plog(LL_ERROR | LF_FUNC, "P3M string table is not null-terminated");
        goto retnull;
    }
    void* data = malloc(ftell(f));
    d->data = data;
    fseek(f, 6, SEEK_SET);
    uint8_t flags = fgetc(f);
    if (flags & P3M_FILEFLAG_HASANIMATIONS) {
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "File has animations");
        #endif
    }
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG | LF_FUNC, "Reading data...");
    #endif
    if (!get16(f, &d->vertexcount)) {
        plog(LL_ERROR | LF_DEBUG | LF_FUNC, LE_EOF);
    }
    {
        struct p3m_vertex* vdata = data;
        fread(vdata, sizeof(*vdata), d->vertexcount, f);
        d->vertices = vdata;
        #if BYTEORDER == BO_BE
        for (int i = 0; i < d->vertexcount; ++i) {
            swapinplacele32((uint32_t*)&vdata->x);
            swapinplacele32((uint32_t*)&vdata->y);
            swapinplacele32((uint32_t*)&vdata->z);
            ++vdata;
        }
        #else
        vdata += d->vertexcount;
        data = vdata;
        #endif
    }
    {
        struct p3m_texturevertex* tvdata = data;
        fread(tvdata, sizeof(*tvdata), d->vertexcount, f);
        d->texturevertices = tvdata;
        #if BYTEORDER == BO_BE
        for (int i = 0; i < d->vertexcount; ++i) {
            swapinplacele32((uint32_t*)&tvdata->u);
            swapinplacele32((uint32_t*)&tvdata->v);
            ++tvdata;
        }
        #else
        tvdata += d->vertexcount;
        data = tvdata;
        #endif
    }
    if (!get16(f, &d->indexcount)) {
        plog(LL_ERROR | LF_DEBUG | LF_FUNC, LE_EOF);
    }
    {
        uint16_t* idata = data;
        fread(idata, sizeof(*idata), d->indexcount, f);
        d->indices = idata;
        #if BYTEORDER == BO_BE
        for (int i = 0; i < d->indexcount; ++i) {
            swapinplacele16((uint32_t*)&idata);
            ++idata;
        }
        #else
        idata += d->indexcount;
        data = idata;
        #endif
    }
    fclose(f);
    d->data = realloc(d->data, ((void*)data) - ((void*)d->data));
    return d;
    retnull:;
    fclose(f);
    free(d->data);
    free(d);
    return NULL;
}
