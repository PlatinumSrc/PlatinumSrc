#include "../rcmgralloc.h"

#include "p3m.h"

#include "../debug.h"

#include "filesystem.h"
#include "logging.h"
#include "byteorder.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define P3M_FILEFLAG_HASANIMATIONS (1 << 0)

#define _STR(x) #x
#define STR(x) _STR(x)

static inline bool get8(FILE* f, uint8_t* d) {
    int tmp = fgetc(f);
    if (tmp == EOF) return false;
    *d = tmp;
    return true;
}
static inline bool get16(FILE* f, uint16_t* d) {
    size_t tmp = fread(d, sizeof(*d), 1, f);
    if (tmp < 1) return false;
    swapinplacele16(d);
    return true;
}
static inline bool get32(FILE* f, uint32_t* d) {
    size_t tmp = fread(d, sizeof(*d), 1, f);
    if (tmp < 1) return false;
    swapinplacele32(d);
    return true;
}

void p3m_free(struct p3m* m) {
    free(m->vertices);
    free(m->indices);
    free(m->indexgroups);
    free(m->boneverts);
    free(m->bones);
    free(m->actiontranslations);
    free(m->actionrotations);
    free(m->actionscales);
    free(m->actioninterps);
    free(m->actionbones);
    free(m->actions);
    free(m->animactions);
    free(m->animations);
    free(m->strings);
    free(m);
}

struct p3m* p3m_loadfile(const char* p, uint8_t loadflags) {
    FILE* f;
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
            return NULL;
        }
        f = fopen(p, "rb");
        if (!f) {
            plog(LL_WARN | LF_FUNC, LE_CANTOPEN(p, errno));
            return NULL;
        }
    }
    struct p3m* m = calloc(1, sizeof(*m));
    if (!m) {
        plog(LL_ERROR | LF_FUNC, LE_MEMALLOC);
        fclose(f);
        return NULL;
    }
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG | LF_FUNC, "Checking header...");
    #endif
    {
        char h[4];
        fread(h, 1, 4, f);
        if (h[0] != 'P' || h[1] != '3' || h[2] != 'M' || h[3] != 0) {
            plog(
                LL_ERROR | LF_FUNC,
                "P3M header magic wrong (%02X %02X %02X %02X != 50 33 4D 00)",
                h[0], h[1], h[2], h[3]
            );
            goto retnull;
        }
    }
    {
        uint8_t v[2] = {fgetc(f), fgetc(f)};
        if (v[0] != P3M_VER_MAJOR || v[1] != P3M_VER_MINOR) {
            plog(LL_ERROR | LF_FUNC, "P3M version wrong (%d.%d != " STR(P3M_VER_MAJOR) "." STR(P3M_VER_MINOR) ")", v[0], v[1]);
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
    fseek(f, 6, SEEK_SET);
    uint8_t flags = fgetc(f);
    #if DEBUG(1)
    if (flags & P3M_FILEFLAG_HASANIMATIONS) {
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "File has animations");
    }
    plog(LL_INFO | LF_DEBUG | LF_FUNC, "Reading data...");
    #endif
    if (loadflags & P3M_LOADFLAG_IGNOREVERTS) {
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "  Ignoring vertex data");
        #endif
        {
            uint16_t vertexcount;
            if (!get16(f, &vertexcount)) {
                plog(LL_ERROR | LF_FUNC, LE_EOF);
                goto retnull;
            }
            fseek(f, vertexcount * sizeof(struct p3m_vertex), SEEK_CUR);
        }
        {
            uint8_t indexgroupcount;
            if (!get8(f, &indexgroupcount)) {
                plog(LL_ERROR | LF_FUNC, LE_EOF);
                goto retnull;
            }
            for (int i = 0; i < indexgroupcount; ++i) {
                fseek(f, sizeof(uint16_t), SEEK_CUR);
                uint16_t indexcount;
                if (!get16(f, &indexcount)) {
                    plog(LL_ERROR | LF_FUNC, LE_EOF);
                    goto retnull;
                }
                fseek(f, indexcount * sizeof(uint16_t), SEEK_CUR);
            }
        }
    } else {
        if (!get16(f, &m->vertexcount)) {
            plog(LL_ERROR | LF_FUNC, LE_EOF);
            goto retnull;
        }
        {
            struct p3m_vertex* data = malloc(m->vertexcount * sizeof(*data));
            if (!data) {
                plog(LL_ERROR | LF_FUNC, LE_MEMALLOC);
                goto retnull;
            }
            m->vertices = data;
            fread(data, sizeof(*data), m->vertexcount, f);
            #if BYTEORDER == BO_BE
            for (int i = 0; i < m->vertexcount; ++i) {
                swapinplacele32((uint32_t*)&data->x);
                swapinplacele32((uint32_t*)&data->y);
                swapinplacele32((uint32_t*)&data->z);
                swapinplacele32((uint32_t*)&data->u);
                swapinplacele32((uint32_t*)&data->v);
                ++data;
            }
            #endif
        }
        if (!get8(f, &m->indexgroupcount)) {
            plog(LL_ERROR | LF_FUNC, LE_EOF);
            goto retnull;
        }
        {
            struct p3m_indexgroup* data = malloc(m->indexgroupcount * sizeof(*data));
            if (!data) {
                plog(LL_ERROR | LF_FUNC, LE_MEMALLOC);
                goto retnull;
            }
            m->indexgroups = data;
            long fpos = ftell(f);
            int indcount = 0;
            for (int i = 0; i < m->indexgroupcount; ++i) {
                uint16_t tmp;
                if (!get16(f, &tmp)) {
                    plog(LL_ERROR | LF_FUNC, LE_EOF);
                    goto retnull;
                }
                data->texture = (char*)(uintptr_t)tmp;
                if (!get16(f, &tmp)) {
                    plog(LL_ERROR | LF_FUNC, LE_EOF);
                    goto retnull;
                }
                data->indexcount = tmp;
                fseek(f, tmp * sizeof(uint16_t), SEEK_CUR);
                data->indices = (uint16_t*)(uintptr_t)indcount;
                indcount += tmp;
                ++data;
            }
            m->indices = malloc(indcount * sizeof(*m->indices));
            if (!m->indices) {
                plog(LL_ERROR | LF_FUNC, LE_MEMALLOC);
                goto retnull;
            }
            data = m->indexgroups;
            fseek(f, fpos, SEEK_SET);
            for (int g = 0; g < m->indexgroupcount; ++g) {
                fseek(f, 2 * sizeof(uint16_t), SEEK_CUR);
                data->indices = &m->indices[(uintptr_t)data->indices];
                uint16_t* data2 = data->indices;
                fread(data2, sizeof(*data2), data->indexcount, f);
                #if BYTEORDER == BO_BE
                for (int i = 0; i < data->indexcount; ++i) {
                    swapinplacele16(data2);
                    ++data2;
                }
                data2 = data->indices;
                #endif
                for (int i = 0; i < data->indexcount; ++i) {
                    if (*data2 >= m->vertexcount) {
                        plog(LL_ERROR | LF_FUNC, "Index %d of group %d is out of bounds (%d >= %d)", i, g, *data2, m->vertexcount);
                        goto retnull;
                    }
                    ++data2;
                }
                ++data;
            }
        }
    }
    if (flags & P3M_FILEFLAG_HASANIMATIONS) {
        
    }
    fclose(f);
    return m;
    retnull:;
    fclose(f);
    p3m_free(m);
    return NULL;
}
