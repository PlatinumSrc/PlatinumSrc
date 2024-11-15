#include "../rcmgralloc.h"

#include "p3m.h"

#include "../debug.h"
#include "../attribs.h"

#include "filesystem.h"
#include "logging.h"
#include "byteorder.h"
#include "vlb.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define _STR(x) #x
#define STR(x) _STR(x)
#define TOPTR(x) ((void*)(uintptr_t)(x))

#define get8 ds_bin_getc_noerr
static ALWAYSINLINE uint16_t get16(struct datastream* ds) {
    uint16_t v;
    v = ds_bin_getc_noerr(ds);
    v |= ds_bin_getc_noerr(ds) << 8;
    return v;
}
static inline uint32_t get32(struct datastream* ds) {
    uint32_t v;
    v = ds_bin_getc_noerr(ds);
    v |= ds_bin_getc_noerr(ds) << 8;
    v |= ds_bin_getc_noerr(ds) << 16;
    v |= ds_bin_getc_noerr(ds) << 24;
    return v;
}

void p3m_free(struct p3m* m) {
    if (m->partcount) {
        int i = 0;
        do {
            struct p3m_part* p = &m->parts[i++];
            free(p->vertices);
            free(p->normals);
            free(p->indices);
            if (p->weightgroupcount) {
                int j = 0;
                do {
                    struct p3m_weightgroup* wg = &p->weightgroups[j++];
                    if (!wg->ranges) continue;
                    free(wg->ranges->weights);
                    free(wg->ranges);
                } while (j < p->weightgroupcount);
            }
        } while (i < m->partcount);
        free(m->parts->weightgroups);
    }
    free(m->parts);
    free(m->materials);
    for (int i = 0; i < m->texturecount; ++i) {
        struct p3m_texture* t = &m->textures[i];
        switch (t->type) {
            case P3M_TEXTYPE_EMBEDDED:
                free(t->embedded.data);
                break;
            default:
                break;
        }
    }
    free(m->textures);
    if (m->animationcount) {
        free(m->animations->actions);
    }
    free(m->animations);
    if (m->actioncount) {
        free(m->actions->partlist);
        int i = 0;
        do {
            struct p3m_action* a = &m->actions[i++];
            for (int j = 0; j < a->bonecount; ++j) {
                struct p3m_actionbone* b = &a->bones[j];
                free(b->translskips);
                free(b->translinterps);
                free(b->transldata);
            }
        } while (i < m->actioncount);
        free(m->actions->bones);
    }
    free(m->actions);
    free(m->strings);
}

#define P3M_LOAD_EOSERR(lbl) do {plog(LL_ERROR | LF_FUNCLN, "Unexpected end of stream"); goto lbl;} while (0)
#define P3M_LOAD_OOMERR(lbl) do {plog(LL_ERROR | LF_FUNCLN, LE_MEMALLOC); goto lbl;} while (0)
bool p3m_load(struct datastream* ds, uint8_t lf, struct p3m* m) {
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG | LF_FUNC, "Checking header...");
    #endif
    {
        char h[3];
        if (ds_bin_read(ds, 3, h) != 3) {
            plog(LL_ERROR | LF_FUNCLN, "Unexpected end of stream");
            return false;
        }
        if (h[0] != 'P' || h[1] != '3' || h[2] != 'M') {
            plog(
                LL_ERROR | LF_FUNC,
                "P3M header magic wrong (expected 50 33 4D, got %02X %02X %02X)",
                h[0], h[1], h[2]
            );
            return false;
        }
    }
    {
        int v = ds_bin_getc(ds);
        if (v == DS_END) {
            plog(LL_ERROR | LF_FUNCLN, "Unexpected end of stream");
            return false;
        }
        if (v != P3M_VER) {
            plog(LL_ERROR | LF_FUNC, "P3M version wrong (expected " STR(P3M_VER) ", got %d)", v);
            return false;
        }
    }
    {
        int f = ds_bin_getc(ds);
        if (f == DS_END) {
            plog(LL_ERROR | LF_FUNCLN, "Unexpected end of stream");
            return false;
        }
    }
    memset(m, 0, sizeof(*m));
    if (!(lf & P3M_LOADFLAG_IGNOREGEOM)) {
        uint8_t partct = get8(ds);
        if (partct) {
            if (ds_bin_read(ds, (partct + 7) / 8, m->vismask) != (partct + 7) / 8U) P3M_LOAD_EOSERR(retfalse);
            if (!(m->parts = malloc(partct * sizeof(*m->parts)))) P3M_LOAD_OOMERR(retfalse);
            unsigned totalwgct = 0;
            unsigned parti = 0;
            do {
                struct p3m_part* p = &m->parts[parti++];
                m->partcount = parti;
                p->normals = NULL;
                p->indices = NULL;
                p->weightgroups = TOPTR(totalwgct);
                p->weightgroupcount = 0;
                uint8_t f = get8(ds);
                p->name = TOPTR(get16(ds));
                p->material = TOPTR(get8(ds));
                uint16_t vertct = get16(ds);
                if (!(p->vertices = malloc(vertct * sizeof(*p->vertices)))) P3M_LOAD_OOMERR(retfalse);
                if (ds_bin_read(ds, vertct * 4 * 5, p->vertices) != vertct * 4 * 5) P3M_LOAD_EOSERR(retfalse);
                #if BYTEORDER == BO_BE
                for (unsigned i = 0; i < vertct; ++i) {
                    struct p3m_vertex* v = &p->vertices[i];
                    *(uint32_t*)v->x = swaple32(*(uint32_t*)v->x);
                    *(uint32_t*)v->y = swaple32(*(uint32_t*)v->y);
                    *(uint32_t*)v->z = swaple32(*(uint32_t*)v->z);
                    *(uint32_t*)v->u = swaple32(*(uint32_t*)v->u);
                    *(uint32_t*)v->v = swaple32(*(uint32_t*)v->v);
                }
                #endif
                if (f & P3M_FILEFLAG_PART_HASNORMS) {
                    if (!(p->normals = malloc(vertct * sizeof(*p->vertices)))) P3M_LOAD_OOMERR(retfalse);
                    if (ds_bin_read(ds, vertct * 4 * 3, p->normals) != vertct * 4 * 3) P3M_LOAD_EOSERR(retfalse);
                    #if BYTEORDER == BO_BE
                    for (unsigned i = 0; i < vertct; ++i) {
                        struct p3m_normal* n = &p->normals[i];
                        *(uint32_t*)n->x = swaple32(*(uint32_t*)n->x);
                        *(uint32_t*)n->y = swaple32(*(uint32_t*)n->y);
                        *(uint32_t*)n->z = swaple32(*(uint32_t*)n->z);
                    }
                    #endif
                }
                uint16_t indct = get16(ds);
                p->indexcount = indct;
                if (!(p->indices = malloc(indct * sizeof(*p->indices)))) P3M_LOAD_OOMERR(retfalse);
                if (ds_bin_read(ds, indct * 2, p->indices) != indct * 2) P3M_LOAD_EOSERR(retfalse);
                for (unsigned i = 0; i < indct; ++i) {
                    #if BYTEORDER == BO_BE
                    p->indices[i] = swaple16(p->indices[i]);
                    #endif
                    if (p->indices[i] >= vertct) {
                        plog(LL_ERROR | LF_FUNCLN, "Index %u of part %u is out of range (%u >= %u)", i, parti - 1, p->indices[i], vertct);
                        goto retfalse;
                    }
                }
                uint8_t wgct = get8(ds);
                if (wgct) {
                    if (!(m->parts->weightgroups = realloc(m->parts->weightgroups, (totalwgct + wgct) * sizeof(*m->parts->weightgroups)))) P3M_LOAD_OOMERR(retfalse);
                    unsigned wgi = 0;
                    do {
                        struct p3m_weightgroup* wg = &m->parts->weightgroups[totalwgct + wgi++];
                        struct VLB(struct p3m_weightrange) wrvlb;
                        VLB_INIT(wrvlb, 4, P3M_LOAD_OOMERR(retfalse););
                        wrvlb.data->weights = NULL;
                        struct VLB(uint8_t) wvlb;
                        VLB_INIT(wvlb, 64, VLB_FREE(wrvlb); P3M_LOAD_OOMERR(retfalse););
                        p->weightgroupcount = wgi;
                        wg->name = TOPTR(get16(ds));
                        wg->ranges = NULL;
                        unsigned totalwct = 0;
                        while (1) {
                            struct p3m_weightrange* wr;
                            VLB_NEXTPTR(wrvlb, wr, 3, 2, VLB_FREE(wvlb); VLB_FREE(wrvlb); P3M_LOAD_OOMERR(retfalse););
                            totalwct += (wr->skip = get16(ds));
                            totalwct += (wr->weightcount = get16(ds));
                            if (totalwct > vertct) {
                                VLB_FREE(wvlb);
                                VLB_FREE(wrvlb);
                                plog(LL_ERROR | LF_FUNCLN, "Weight group %u of part %u has too many weights (%u > %u)", wgi - 1, parti - 1, totalwct, vertct);
                                goto retfalse;
                            }
                            if (!wr->weightcount) break;
                            wr->weights = TOPTR(wvlb.len);
                            VLB_EXP(wvlb, wr->weightcount, 3, 2, VLB_FREE(wvlb); VLB_FREE(wrvlb); P3M_LOAD_OOMERR(retfalse););
                            if (ds_bin_read(ds, wr->weightcount, wvlb.data + wvlb.len) != wr->weightcount) {
                                VLB_FREE(wvlb);
                                VLB_FREE(wrvlb);
                                P3M_LOAD_EOSERR(retfalse);
                            }
                        }
                        VLB_SHRINK(wvlb, VLB_FREE(wvlb); VLB_FREE(wrvlb); P3M_LOAD_OOMERR(retfalse););
                        VLB_SHRINK(wrvlb, VLB_FREE(wvlb); VLB_FREE(wrvlb); P3M_LOAD_OOMERR(retfalse););
                        wg->ranges = wrvlb.data;
                        wrvlb.data->weights = wvlb.data;
                        ++wrvlb.data;
                        while (wrvlb.data->weightcount) {
                            wrvlb.data->weights = wvlb.data + (uintptr_t)wrvlb.data->weights;
                            ++wrvlb.data;
                        }
                    } while (wgi < wgct);
                    totalwgct += wgct;
                }
            } while (parti < partct);
            for (parti = 1; parti < partct; ++parti) {
                struct p3m_part* p = &m->parts[parti];
                p->weightgroups = m->parts->weightgroups + (uintptr_t)p->weightgroups;
            }
        }
    }
    return true;
    retfalse:;
    p3m_free(m);
    return false;
}
