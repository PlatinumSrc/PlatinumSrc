#include "../rcmgralloc.h"

#include "p3m.h"

#ifndef PSRC_MODULE_SERVER
    #include "../engine/ptf.h"
#endif

#include "../filesystem.h"
#include "../logging.h"
#include "../byteorder.h"
#include "../vlb.h"

#include "../debug.h"
#include "../attribs.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define _STR(x) #x
#define STR(x) _STR(x)
#define TOPTR(x) ((void*)(uintptr_t)(x))

#define get8 ds_getc_noerr
static ALWAYSINLINE uint16_t get16(struct datastream* ds) {
    uint16_t v;
    v = ds_getc_noerr(ds);
    v |= ds_getc_noerr(ds) << 8;
    return v;
}
static inline uint32_t get32(struct datastream* ds) {
    uint32_t v;
    v = ds_getc_noerr(ds);
    v |= ds_getc_noerr(ds) << 8;
    v |= ds_getc_noerr(ds) << 16;
    v |= ds_getc_noerr(ds) << 24;
    return v;
}
static inline float getf32(struct datastream* ds) {
    float v;
    ds_read(ds, 4, &v);
    return swaplefloat(v);
}

void p3m_free(struct p3m* m) {
    if (m->partcount) {
        unsigned i = 0;
        do {
            struct p3m_part* p = &m->parts[i++];
            free(p->vertices);
            free(p->normals);
            free(p->indices);
            if (p->weightgroupcount) {
                unsigned j = 0;
                do {
                    free(p->weightgroups[j].ranges->weights);
                    free(p->weightgroups[j].ranges);
                    ++j;
                } while (j < p->weightgroupcount);
            }
        } while (i < m->partcount);
        free(m->parts->weightgroups);
    }
    free(m->parts);
    free(m->materials);
    for (unsigned i = 0; i < m->texturecount; ++i) {
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
    free(m->bones);
    if (m->animationcount) {
        free(m->animations->actions);
    }
    free(m->animations);
    if (m->actioncount) {
        free(m->actions->partlist);
        unsigned i = 0;
        do {
            struct p3m_action* a = &m->actions[i++];
            for (unsigned j = 0; j < a->bonecount; ++j) {
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

#define P3M_LOAD_INTERR(lbl) do {plog(LL_ERROR | LF_FUNCLN, "Internal error"); goto lbl;} while (0)
#define P3M_LOAD_EOSERR(lbl) do {plog(LL_ERROR | LF_FUNCLN, "Unexpected end of stream"); goto lbl;} while (0)
#define P3M_LOAD_OOMERR(lbl) do {plog(LL_ERROR | LF_FUNCLN, LE_MEMALLOC); goto lbl;} while (0)
bool p3m_load(struct datastream* ds, uint8_t lf, struct p3m* m) {
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG | LF_FUNC, "Checking header...");
    #endif
    {
        char h[3];
        if (ds_read(ds, 3, h) != 3) {
            plog(LL_ERROR | LF_FUNCLN, "Unexpected end of stream");
            return false;
        }
        if (h[0] != 'P' || h[1] != '3' || h[2] != 'M') {
            plog(
                LL_ERROR,
                "P3M header magic wrong (expected 50 33 4D, got %02X %02X %02X)",
                h[0], h[1], h[2]
            );
            return false;
        }
    }
    {
        int v = ds_getc(ds);
        if (v == DS_END) {
            plog(LL_ERROR | LF_FUNCLN, "Unexpected end of stream");
            return false;
        }
        if (v != P3M_VER) {
            plog(LL_ERROR, "P3M version wrong (expected " STR(P3M_VER) ", got %d)", v);
            return false;
        }
    }
    {
        int f = ds_getc(ds);
        if (f == DS_END) {
            plog(LL_ERROR | LF_FUNCLN, "Unexpected end of stream");
            return false;
        }
    }
    memset(m, 0, sizeof(*m));
    if (!(lf & P3M_LOADFLAG_IGNOREGEOM)) {
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Reading parts...");
        #endif
        uint8_t partct = get8(ds);
        if (partct) {
            if (!(m->parts = malloc(partct * sizeof(*m->parts)))) P3M_LOAD_OOMERR(retfalse);
            if (ds_read(ds, (partct + 7) / 8, m->vismask) != (partct + 7) / 8U) P3M_LOAD_EOSERR(retfalse);
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
                p->vertexcount = vertct;
                if (!(p->vertices = malloc(vertct * sizeof(*p->vertices)))) P3M_LOAD_OOMERR(retfalse);
                if (ds_read(ds, vertct * 4 * 5, p->vertices) != vertct * 4 * 5) P3M_LOAD_EOSERR(retfalse);
                #if BYTEORDER == BO_BE
                for (unsigned i = 0; i < vertct; ++i) {
                    struct p3m_vertex* v = &p->vertices[i];
                    v->x = swaplefloat(v->x);
                    v->y = swaplefloat(v->y);
                    v->z = swaplefloat(v->z);
                    v->u = swaplefloat(v->u);
                    v->v = swaplefloat(v->v);
                }
                #endif
                if (f & P3M_FILEFLAG_PART_HASNORMS) {
                    if (!(lf & P3M_LOADFLAG_IGNORENORMS)) {
                        if (!(p->normals = malloc(vertct * sizeof(*p->normals)))) P3M_LOAD_OOMERR(retfalse);
                        if (ds_read(ds, vertct * 4 * 3, p->normals) != vertct * 4 * 3) P3M_LOAD_EOSERR(retfalse);
                        #if BYTEORDER == BO_BE
                        for (unsigned i = 0; i < vertct; ++i) {
                            struct p3m_normal* n = &p->normals[i];
                            n->x = swaplefloat(n->x);
                            n->y = swaplefloat(n->y);
                            n->z = swaplefloat(n->z);
                        }
                        #endif
                    } else {
                        if (ds_skip(ds, vertct * 4 * 3) != vertct * 4 * 3) P3M_LOAD_EOSERR(retfalse);
                    }
                }
                uint16_t indct = get16(ds);
                p->indexcount = indct;
                if (!(p->indices = malloc(indct * sizeof(*p->indices)))) P3M_LOAD_OOMERR(retfalse);
                if (ds_read(ds, indct * 2, p->indices) != indct * 2) P3M_LOAD_EOSERR(retfalse);
                for (unsigned i = 0; i < indct; ++i) {
                    #if BYTEORDER == BO_BE
                    p->indices[i] = swaple16(p->indices[i]);
                    #endif
                    if (p->indices[i] >= vertct) {
                        plog(LL_ERROR, "Index %u of part %u is out of range (got %u, expected less than %u)", i, parti - 1, p->indices[i], vertct);
                        goto retfalse;
                    }
                }
                uint8_t wgct = get8(ds);
                if (wgct) {
                    unsigned wgi = 0;
                    if (!(lf & P3M_LOADFLAG_IGNORESKEL)) {
                        if (!(m->parts->weightgroups = realloc(m->parts->weightgroups, (totalwgct + wgct) * sizeof(*m->parts->weightgroups)))) P3M_LOAD_OOMERR(retfalse);
                        do {
                            struct p3m_weightgroup* wg = &m->parts->weightgroups[totalwgct + wgi++];
                            struct VLB(struct p3m_weightrange) wrvlb;
                            VLB_INIT(wrvlb, 4, P3M_LOAD_OOMERR(retfalse););
                            wrvlb.data->weights = NULL;
                            struct VLB(uint8_t) wvlb;
                            VLB_INIT(wvlb, 64, VLB_FREE(wrvlb); P3M_LOAD_OOMERR(retfalse););
                            wg->name = TOPTR(get16(ds));
                            unsigned totalwct = 0;
                            while (1) {
                                struct p3m_weightrange* wr;
                                VLB_NEXTPTR(wrvlb, wr, 3, 2, VLB_FREE(wvlb); VLB_FREE(wrvlb); P3M_LOAD_OOMERR(retfalse););
                                totalwct += (wr->skip = get16(ds));
                                totalwct += (wr->weightcount = get16(ds));
                                if (totalwct > vertct) {
                                    VLB_FREE(wvlb);
                                    VLB_FREE(wrvlb);
                                    plog(
                                        LL_ERROR,
                                        "Weight group %u of part %u has too many weights (got %u, expected less than or equal to %u)",
                                        wgi - 1, parti - 1, totalwct, vertct
                                    );
                                    goto retfalse;
                                }
                                if (!wr->weightcount) break;
                                wr->weights = TOPTR(wvlb.len);
                                uintptr_t oldlen = wvlb.len;
                                VLB_EXP(wvlb, wr->weightcount, 3, 2, VLB_FREE(wvlb); VLB_FREE(wrvlb); P3M_LOAD_OOMERR(retfalse););
                                if (ds_read(ds, wr->weightcount, wvlb.data + oldlen) != wr->weightcount) {
                                    VLB_FREE(wvlb);
                                    VLB_FREE(wrvlb);
                                    P3M_LOAD_EOSERR(retfalse);
                                }
                            }
                            VLB_SHRINK(wvlb, VLB_FREE(wvlb); VLB_FREE(wrvlb); P3M_LOAD_OOMERR(retfalse););
                            VLB_SHRINK(wrvlb, VLB_FREE(wvlb); VLB_FREE(wrvlb); P3M_LOAD_OOMERR(retfalse););
                            p->weightgroupcount = wgi;
                            wg->ranges = wrvlb.data;
                            wrvlb.data->weights = wvlb.data;
                            ++wrvlb.data;
                            while (wrvlb.data->weightcount) {
                                wrvlb.data->weights = wvlb.data + (uintptr_t)wrvlb.data->weights;
                                ++wrvlb.data;
                            }
                        } while (wgi < wgct);
                        totalwgct += wgct;
                    } else {
                        do {
                            get16(ds);
                            while (1) {
                                get16(ds);
                                uint16_t wct = get16(ds);
                                if (!wct) break;
                                if (ds_skip(ds, wct) != wct) P3M_LOAD_EOSERR(retfalse);
                            }
                        } while (wgi < wgct);
                    }
                }
            } while (parti < partct);
            for (parti = 1; parti < partct; ++parti) {
                struct p3m_part* p = &m->parts[parti];
                p->weightgroups = m->parts->weightgroups + (uintptr_t)p->weightgroups;
            }
        }
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Reading materials...");
        #endif
        uint8_t matct = get8(ds);
        if (matct) {
            if (!(m->materials = malloc(matct * sizeof(*m->materials)))) P3M_LOAD_OOMERR(retfalse);
            m->materialcount = matct;
            unsigned mati = 0;
            do {
                struct p3m_material* mat = &m->materials[mati];
                if ((mat->rendmode = get8(ds)) >= P3M_MATRENDMODE__COUNT) {
                    plog(LL_WARN, "Render mode of material %u is invalid (got %u, expected less than %u)", mati, mat->rendmode, P3M_MATRENDMODE__COUNT);
                    mat->rendmode = P3M_MATRENDMODE_NORMAL;
                }
                mat->texture = TOPTR(get8(ds));
                if (ds_read(ds, 4, &mat->color) != 4) P3M_LOAD_EOSERR(retfalse);
                if (ds_read(ds, 3, &mat->emission) != 3) P3M_LOAD_EOSERR(retfalse);
                mat->shading = get8(ds);
            } while (++mati < matct);
        }
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Validating part material indices...");
        #endif
        for (unsigned parti = 0; parti < partct; ++parti) {
            struct p3m_part* p = &m->parts[parti];
            if ((uintptr_t)p->material < matct) {
                p->material = m->materials + (uintptr_t)p->material;
            } else {
                plog(LL_ERROR, "Material of part %u is out of bounds (got %u, expected less than %u)", parti, (unsigned)(uintptr_t)p->material, matct);
                goto retfalse;
            }
        }
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Reading textures...");
        #endif
        uint8_t texct = get8(ds);
        if (texct) {
            if (!(m->textures = malloc(texct * sizeof(*m->textures)))) P3M_LOAD_OOMERR(retfalse);
            unsigned texi = 0;
            do {
                struct p3m_texture* t = &m->textures[texi++];
                m->texturecount = texi;
                t->type = get8(ds);
                switch (t->type) {
                    case P3M_TEXTYPE_EMBEDDED: {
                        uint32_t sz = get32(ds);
                        #ifndef PSRC_MODULE_SERVER
                        if (!(lf & P3M_LOADFLAG_IGNOREEMBTEXS)) {
                            size_t tmp = ds_tell(ds);
                            unsigned r, c;
                            {
                                struct datastream tmpds;
                                if (!ds_opensect(ds, sz, 0, &tmpds)) P3M_LOAD_INTERR(retfalse);
                                t->embedded.data = ptf_load(&tmpds, &r, &c);
                                ds_close(&tmpds);
                            }
                            tmp = ds_tell(ds) - tmp;
                            if (t->embedded.data) {
                                t->embedded.res = r;
                                t->embedded.ch = c;
                            } else {
                                plog(LL_WARN, "Failed to decode texture %u", texi - 1);
                            }
                            if (tmp < sz) {
                                if (t->embedded.data) plog(
                                    LL_WARN, "Data of texture %u is smaller than expected (got %lu, expected %u)",
                                    texi - 1, (long unsigned)tmp, (unsigned)sz
                                );
                                sz -= tmp;
                                if (ds_skip(ds, sz) != sz) P3M_LOAD_EOSERR(retfalse);
                            } else if (tmp > sz) {
                                plog(LL_ERROR, "Data of texture %u is larger than expected (got %lu, expected %u)", texi - 1, (long unsigned)tmp, (unsigned)sz);
                                goto retfalse;
                            }
                        } else {
                        #endif
                            if (ds_skip(ds, sz) != sz) P3M_LOAD_EOSERR(retfalse);
                            t->embedded.data = NULL;
                        #ifndef PSRC_MODULE_SERVER
                        }
                        #endif
                    } break;
                    case P3M_TEXTYPE_EXTERNAL: {
                        t->external.rcpath = TOPTR(get16(ds));
                    } break;
                    default: {
                        plog(LL_WARN, "Type of texture %u is invalid (got %u, expected less than %u)", texi - 1, t->type, P3M_TEXTYPE__COUNT);
                        t->type = P3M_TEXTYPE_EMBEDDED;
                        t->embedded.data = NULL;
                        uint32_t sz = get32(ds);
                        if (ds_skip(ds, sz) != sz) P3M_LOAD_EOSERR(retfalse);
                    } break;
                }
            } while (texi < texct);
        }
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Validating material texture indices...");
        #endif
        for (unsigned mati = 0; mati < matct; ++mati) {
            struct p3m_material* mat = &m->materials[mati];
            if ((uintptr_t)mat->texture == 255) {
                mat->texture = NULL;
            } else if ((uintptr_t)mat->texture < texct) {
                mat->texture = m->textures + (uintptr_t)mat->texture;
            } else {
                plog(LL_ERROR, "Texture of material %u is out of bounds (got %u, expected less than %u)", mati, (unsigned)(uintptr_t)mat->texture, texct);
                goto retfalse;
            }
        }
    } else {
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Skipping parts...");
        #endif
        uint8_t partct = get8(ds);
        if (partct) {
            if (ds_skip(ds, (partct + 7) / 8) != (partct + 7) / 8U) P3M_LOAD_EOSERR(retfalse);
            unsigned parti = 0;
            do {
                uint8_t f = get8(ds);
                get16(ds);
                get8(ds);
                uint16_t vertct = get16(ds);
                if (ds_skip(ds, vertct * 4 * 5) != vertct * 4 * 5) P3M_LOAD_EOSERR(retfalse);
                if (f & P3M_FILEFLAG_PART_HASNORMS && ds_skip(ds, vertct * 4 * 3) != vertct * 4 * 3) P3M_LOAD_EOSERR(retfalse);
                uint16_t indct = get16(ds);
                if (ds_skip(ds, indct * 2) != indct * 2) P3M_LOAD_EOSERR(retfalse);
                uint8_t wgct = get8(ds);
                if (wgct) {
                    unsigned wgi = 0;
                    do {
                        get16(ds);
                        while (1) {
                            get16(ds);
                            uint16_t wct = get16(ds);
                            if (!wct) break;
                            if (ds_skip(ds, wct) != wct) P3M_LOAD_EOSERR(retfalse);
                        }
                    } while (wgi < wgct);
                }
            } while (parti < partct);
        }
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Skipping materials...");
        #endif
        uint8_t matct = get8(ds);
        for (unsigned mati = 0; mati < matct; ++mati) {
            if (ds_skip(ds, 10) != 10) P3M_LOAD_EOSERR(retfalse);
        }
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Skipping textures...");
        #endif
        uint8_t texct = get8(ds);
        for (unsigned texi = 0; texi < texct; ++texi) {
            enum p3m_textype textype = get8(ds);
            switch (textype) {
                case P3M_TEXTYPE_EMBEDDED: {
                    uint32_t sz = get32(ds);
                    if (ds_skip(ds, sz) != sz) P3M_LOAD_EOSERR(retfalse);
                } break;
                case P3M_TEXTYPE_EXTERNAL: {
                    get16(ds);
                } break;
                default: {
                    plog(LL_WARN, "Type of texture %u is invalid (got %u, expected less than %u)", texi, textype, P3M_TEXTYPE__COUNT);
                    uint32_t sz = get32(ds);
                    if (ds_skip(ds, sz) != sz) P3M_LOAD_EOSERR(retfalse);
                } break;
            }
        }
    }
    if (!(lf & P3M_LOADFLAG_IGNORESKEL)) {
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Reading bones...");
        #endif
        uint8_t bonect = get8(ds);
        if (bonect) {
            if (!(m->bones = malloc(bonect * sizeof(*m->bones)))) P3M_LOAD_OOMERR(retfalse);
            m->bonecount = bonect;
            unsigned bonei = 0;
            do {
                struct p3m_bone* b = &m->bones[bonei];
                b->name = TOPTR(get16(ds));
                if (ds_read(ds, 3 * 4, &b->head) != 3 * 4) P3M_LOAD_EOSERR(retfalse);
                #if BYTEORDER == BO_BE
                b->head.x = swaplefloat(b->head.x);
                b->head.y = swaplefloat(b->head.y);
                b->head.z = swaplefloat(b->head.z);
                #endif
                if (ds_read(ds, 3 * 4, &b->tail) != 3 * 4) P3M_LOAD_EOSERR(retfalse);
                #if BYTEORDER == BO_BE
                b->tail.x = swaplefloat(b->tail.x);
                b->tail.y = swaplefloat(b->tail.y);
                b->tail.z = swaplefloat(b->tail.z);
                #endif
                b->childcount = get8(ds);
                if (b->childcount >= bonect - bonei) {
                    plog(LL_ERROR, "Child count of bone %u is too large (got %u, expected less than %u)", bonei, b->childcount, bonect - bonei);
                    goto retfalse;
                }
            } while (++bonei < bonect);
        }
    } else {
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Skipping bones...");
        #endif
        uint8_t bonect = get8(ds);
        for (unsigned bonei = 0; bonei < bonect; ++bonei) {
            get16(ds);
            if (ds_skip(ds, 6 * 4) != 6 * 4) P3M_LOAD_EOSERR(retfalse);
            get8(ds);
        }
    }
    if (!(lf & P3M_LOADFLAG_IGNOREANIMS)) {
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Reading animations...");
        #endif
        uint8_t animct = get8(ds);
        if (animct) {
            if (!(m->animations = malloc(animct * sizeof(*m->animations)))) P3M_LOAD_OOMERR(retfalse);
            struct VLB(struct p3m_animationactref) actrefvlb;
            VLB_INIT(actrefvlb, 16, P3M_LOAD_OOMERR(retfalse););
            m->animationcount = animct;
            unsigned animi = 0;
            do {
                struct p3m_animation* a = &m->animations[animi];
                a->name = TOPTR(get16(ds));
                uint8_t actct = get8(ds);
                a->actioncount = actct;
                uintptr_t oldlen = actrefvlb.len;
                a->actions = TOPTR(oldlen);
                VLB_EXP(actrefvlb, actct, 3, 2, VLB_FREE(actrefvlb); P3M_LOAD_OOMERR(retfalse););
                for (unsigned acti = 0; acti < actct; ++acti) {
                    struct p3m_animationactref* r = &actrefvlb.data[oldlen + acti];
                    r->action = TOPTR(get8(ds));
                    r->speed = getf32(ds);
                    r->start = get16(ds);
                    r->end = get16(ds);
                }
            } while (++animi < animct);
            VLB_SHRINK(actrefvlb, VLB_FREE(actrefvlb); P3M_LOAD_OOMERR(retfalse););
            m->animations->actions = actrefvlb.data;
            for (animi = 1; animi < animct; ++animi) {
                struct p3m_animation* a = &m->animations[animi];
                a->actions = actrefvlb.data + (uintptr_t)a->actions;
            }
        }
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Reading actions...");
        #endif
        uint8_t actct = get8(ds);
        if (actct) {
            if (!(m->actions = malloc(actct * sizeof(*m->actions)))) P3M_LOAD_OOMERR(retfalse);
            m->actions->partlist = NULL;
            m->actions->bones = NULL;
            struct VLB(char*) plvlb;
            VLB_INIT(plvlb, 16, P3M_LOAD_OOMERR(retfalse););
            m->actioncount = actct;
            unsigned acti = 0;
            do {
                struct p3m_action* a = &m->actions[acti];
                a->frametime = get32(ds);
                a->partlistmode = get8(ds);
                if (a->partlistmode >= P3M_ACTPARTLISTMODE__COUNT) {
                    plog(
                        LL_WARN,
                        "Part list mode of action %u is invalid (got %u, expected less than %u)",
                        acti - 1, a->partlistmode, P3M_ACTPARTLISTMODE__COUNT
                    );
                    a->partlistmode = P3M_ACTPARTLISTMODE_DEFAULTWHITE;
                }
                uint8_t pllen = get8(ds);
                a->partlistlen = pllen;
                uintptr_t ploldlen = plvlb.len;
                a->partlist = TOPTR(ploldlen);
                VLB_EXP(plvlb, pllen, 3, 2, VLB_FREE(plvlb); P3M_LOAD_OOMERR(retfalse););
                for (unsigned parti = 0; parti < pllen; ++parti) {
                    plvlb.data[ploldlen + parti] = TOPTR(get16(ds));
                }
                uint8_t bonect = get8(ds);
                a->bonecount = 0;
                if (!(a->bones = malloc(bonect * sizeof(*a->bones)))) {
                    VLB_FREE(plvlb);
                    P3M_LOAD_OOMERR(retfalse);
                }
                unsigned bonei = 0;
                do {
                    struct p3m_actionbone* b = &a->bones[bonei++];
                    b->name = TOPTR(get16(ds));
                    uint8_t translct = get8(ds);
                    uint8_t rotct = get8(ds);
                    uint8_t scalect = get8(ds);
                    size_t sz = translct + rotct + scalect;
                    b->translcount = translct;
                    b->rotcount = rotct;
                    b->scalecount = scalect;
                    if (!(b->translskips = malloc(sz * sizeof(*b->translskips)))) {
                        VLB_FREE(plvlb);
                        P3M_LOAD_OOMERR(retfalse);
                    }
                    a->bonecount = bonei;
                    b->rotskips = b->translskips + translct;
                    b->scaleskips = b->rotskips + rotct;
                    if (!(b->translinterps = malloc(sz * sizeof(*b->translinterps)))) {
                        VLB_FREE(plvlb);
                        b->transldata = NULL;
                        P3M_LOAD_OOMERR(retfalse);
                    }
                    b->rotinterps = b->translinterps + translct;
                    b->scaleinterps = b->rotinterps + rotct;
                    if (!(b->transldata = malloc(sz * sizeof(*b->transldata)))) {
                        VLB_FREE(plvlb);
                        P3M_LOAD_OOMERR(retfalse);
                    }
                    b->rotdata = b->transldata + translct;
                    b->scaledata = b->rotdata + rotct;
                    if (ds_read(ds, sz, b->translskips) != sz) {
                        VLB_FREE(plvlb);
                        P3M_LOAD_OOMERR(retfalse);
                    }
                    for (size_t i = 0; i < translct; ++i) {
                        uint8_t tmp = get8(ds);
                        if (tmp >= P3M_ACTINTERP__COUNT) {
                            plog(
                                LL_WARN,
                                "Translation interpolation mode %u of bone action data %u under action %u is invalid (got %u, expected less than %u)",
                                i, bonei - 1, acti, tmp, P3M_ACTINTERP__COUNT
                            );
                            tmp = P3M_ACTINTERP_LINEAR;
                        }
                        b->translinterps[i] = tmp;
                    }
                    for (size_t i = 0; i < rotct; ++i) {
                        uint8_t tmp = get8(ds);
                        if (tmp >= P3M_ACTINTERP__COUNT) {
                            plog(
                                LL_WARN,
                                "Rotation interpolation mode %u of bone action data %u under action %u is invalid (got %u, expected less than %u)",
                                i, bonei - 1, acti, tmp, P3M_ACTINTERP__COUNT
                            );
                            tmp = P3M_ACTINTERP_LINEAR;
                        }
                        b->rotinterps[i] = tmp;
                    }
                    for (size_t i = 0; i < scalect; ++i) {
                        uint8_t tmp = get8(ds);
                        if (tmp >= P3M_ACTINTERP__COUNT) {
                            plog(
                                LL_WARN,
                                "Scale interpolation mode %u of bone action data %u under action %u is invalid (got %u, expected less than %u)",
                                i, bonei - 1, acti, tmp, P3M_ACTINTERP__COUNT
                            );
                            tmp = P3M_ACTINTERP_LINEAR;
                        }
                        b->scaleinterps[i] = tmp;
                    }
                    if (ds_read(ds, sz * 4 * 3, b->transldata) != sz * 4 * 3) {
                        VLB_FREE(plvlb);
                        P3M_LOAD_OOMERR(retfalse);
                    }
                    #if BYTEORDER == BO_BE
                    for (size_t i = 0; i < sz; ++i) {
                        b->transldata[i][0] = swaplefloat(b->transldata[i][0]);
                        b->transldata[i][1] = swaplefloat(b->transldata[i][1]);
                        b->transldata[i][2] = swaplefloat(b->transldata[i][2]);
                    }
                    #endif
                } while (bonei < bonect);
            } while (++acti < actct);
            VLB_SHRINK(plvlb, VLB_FREE(plvlb); P3M_LOAD_OOMERR(retfalse););
            for (acti = 0; acti < actct; ++acti) {
                struct p3m_action* a = &m->actions[acti];
                a->partlist = plvlb.data + (uintptr_t)a->partlist;
            }
        }
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Validating animation action indices...");
        #endif
        for (unsigned animi = 0; animi < animct; ++animi) {
            struct p3m_animation* a = &m->animations[animi];
            for (unsigned acti = 0; acti < a->actioncount; ++acti) {
                struct p3m_animationactref* r = &a->actions[acti];
                if ((uintptr_t)r->action < actct) {
                    r->action = m->actions + (uintptr_t)r->action;
                } else {
                    plog(
                        LL_ERROR,
                        "Action reference %u of animation %u is out of range (got %u, expected less than %u)",
                        acti, animi, (unsigned)(uintptr_t)r->action, actct
                    );
                    goto retfalse;
                }
            }
        }
    } else {
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Skipping animations...");
        #endif
        uint8_t animct = get8(ds);
        for (unsigned animi = 0; animi < animct; ++animi) {
            get16(ds);
            uint8_t actrefct = get8(ds);
            for (unsigned actrefi = 0; actrefi < actrefct; ++actrefi) {
                if (ds_skip(ds, 9) != 9) P3M_LOAD_EOSERR(retfalse);
            }
        }
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG | LF_FUNC, "Skipping actions...");
        #endif
        uint8_t actct = get8(ds);
        for (unsigned acti = 0; acti < actct; ++acti) {
            get32(ds);
            get8(ds);
            uint8_t partct = get8(ds);
            if (ds_skip(ds, partct * 2) != partct * 2) P3M_LOAD_EOSERR(retfalse);
            uint8_t bonect = get8(ds);
            for (unsigned bonei = 0; bonei < bonect; ++bonei) {
                get16(ds);
                uint8_t translct = get8(ds);
                uint8_t rotct = get8(ds);
                uint8_t scalect = get8(ds);
                size_t skipsz = (translct + rotct + scalect) * 14;
                if (ds_skip(ds, skipsz) != skipsz) P3M_LOAD_EOSERR(retfalse);
            }
        } 
    }
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG | LF_FUNC, "Reading string table...");
    #endif
    struct charbuf strtbl;
    cb_init(&strtbl, 256);
    while (!ds_atend(ds)) {
        unsigned long oldlen = strtbl.len;
        cb_addmultifake(&strtbl, 256);
        strtbl.len = ds_read(ds, 256, strtbl.data + oldlen) + oldlen;
        if (strtbl.len > 65535) {
            cb_dump(&strtbl);
            plog(LL_ERROR, "String table is too large (got %u, expected less than or equal to 65535)", strtbl.len);
            goto retfalse;
        }
    }
    if (strtbl.len && !strtbl.data[strtbl.len - 1]) --strtbl.len;
    m->strings = cb_finalize(&strtbl);
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG | LF_FUNC, "Validating strings...");
    #endif
    for (unsigned parti = 0; parti < m->partcount; ++parti) {
        struct p3m_part* p = &m->parts[parti];
        if ((uintptr_t)p->name > strtbl.len) {
            plog(
                LL_ERROR,
                "Name string for part %u is out of bounds (got %u, expected less than or equal to %u)",
                parti, (unsigned)(uintptr_t)p->name, strtbl.len
            );
            goto retfalse;
        }
        p->name = strtbl.data + (uintptr_t)p->name;
        for (unsigned wgi = 0; wgi < p->weightgroupcount; ++wgi) {
            struct p3m_weightgroup* wg = &p->weightgroups[wgi];
            if ((uintptr_t)wg->name > strtbl.len) {
                plog(
                    LL_ERROR,
                    "Name string for weight group %u under part %u is out of bounds (got %u, expected less than or equal to %u)",
                    wgi, parti, (unsigned)(uintptr_t)wg->name, strtbl.len
                );
                goto retfalse;
            }
            wg->name = strtbl.data + (uintptr_t)wg->name;
        }
    }
    for (unsigned texi = 0; texi < m->texturecount; ++texi) {
        struct p3m_texture* t = &m->textures[texi];
        switch (t->type) {
            case P3M_TEXTYPE_EXTERNAL:
                if ((uintptr_t)t->external.rcpath > strtbl.len) {
                    plog(
                        LL_ERROR,
                        "Resource path string for texture %u is out of bounds (got %u, expected less than or equal to %u)",
                        texi, (unsigned)(uintptr_t)t->external.rcpath, strtbl.len
                    );
                    goto retfalse;
                }
                t->external.rcpath = strtbl.data + (uintptr_t)t->external.rcpath;
                break;
            default:
                break;
        }
    }
    for (unsigned bonei = 0; bonei < m->bonecount; ++bonei) {
        struct p3m_bone* b = &m->bones[bonei];
        if ((uintptr_t)b->name > strtbl.len) {
            plog(
                LL_ERROR,
                "Name string for bone %u is out of bounds (got %u, expected less than or equal to %u)",
                bonei, (unsigned)(uintptr_t)b->name, strtbl.len
            );
            goto retfalse;
        }
        b->name = strtbl.data + (uintptr_t)b->name;
    }
    for (unsigned animi = 0; animi < m->animationcount; ++animi) {
        struct p3m_animation* a = &m->animations[animi];
        if ((uintptr_t)a->name > strtbl.len) {
            plog(
                LL_ERROR,
                "Name string for animation %u is out of bounds (got %u, expected less than or equal to %u)",
                animi, (unsigned)(uintptr_t)a->name, strtbl.len
            );
            goto retfalse;
        }
        a->name = strtbl.data + (uintptr_t)a->name;
    }
    for (unsigned acti = 0; acti < m->actioncount; ++acti) {
        struct p3m_action* a = &m->actions[acti];
        for (unsigned parti = 0; parti < a->partlistlen; ++parti) {
            if ((uintptr_t)a->partlist[parti] > strtbl.len) {
                plog(
                    LL_ERROR,
                    "Part name string %u under action %u is out of bounds (got %u, expected less than or equal to %u)",
                    parti, acti, (unsigned)(uintptr_t)a->partlist[parti], strtbl.len
                );
                goto retfalse;
            }
            a->partlist[parti] = strtbl.data + (uintptr_t)a->partlist[parti];
        }
        for (unsigned bonei = 0; bonei < a->bonecount; ++bonei) {
            struct p3m_actionbone* b = &a->bones[bonei];
            if ((uintptr_t)b->name > strtbl.len) {
                plog(
                    LL_ERROR,
                    "Name string for bone action data %u under action %u is out of bounds (got %u, expected less than or equal to %u)",
                    bonei, acti, (unsigned)(uintptr_t)b->name, strtbl.len
                );
                goto retfalse;
            }
            b->name = strtbl.data + (uintptr_t)b->name;
        }
    }

    #if DEBUG(2)
    plog(LL_INFO | LF_DEBUG | LF_FUNC, "Model info:");
    plog(LL_INFO | LF_DEBUG, "  Parts (%u):", m->partcount);
    for (unsigned i = 0; i < m->partcount; ++i) {
        struct p3m_part* p = &m->parts[i];
        plog(LL_INFO | LF_DEBUG, "    Part %u (%s):", i, p->name);
        plog(LL_INFO | LF_DEBUG, "      Material: %u", (unsigned)(((uintptr_t)p->material - (uintptr_t)m->materials) / sizeof(*p->material)));
        plog(
            LL_INFO | LF_DEBUG,
            "      Vertices (%u): (%.3f, %.3f, %.3f) ... (%.3f, %.3f, %.3f)",
            p->vertexcount,
            (double)p->vertices->x, (double)p->vertices->y, (double)p->vertices->z,
            (double)p->vertices[p->vertexcount - 1].x, (double)p->vertices[p->vertexcount - 1].y, (double)p->vertices[p->vertexcount - 1].z
        );
        if (p->normals) {
            plog(
                LL_INFO | LF_DEBUG,
                "      Normals: (%.3f, %.3f, %.3f) ... (%.3f, %.3f, %.3f)",
                (double)p->normals->x, (double)p->normals->y, (double)p->normals->z,
                (double)p->normals[p->vertexcount - 1].x, (double)p->normals[p->vertexcount - 1].y, (double)p->normals[p->vertexcount - 1].z
            );
        } else {
            plog(LL_INFO | LF_DEBUG, "      Normals: (None)");
        }
        plog(LL_INFO | LF_DEBUG, "      Indices (%u): %u ... %u", p->indexcount, *p->indices, p->indices[p->indexcount - 1]);
        plog(LL_INFO | LF_DEBUG, "      Weight groups (%u):", p->weightgroupcount);
        for (unsigned j = 0; j < p->weightgroupcount; ++j) {
            struct p3m_weightgroup* wg = &p->weightgroups[j];
            plog(LL_INFO | LF_DEBUG, "        Weight group %u (%s):", j, wg->name);
            struct p3m_weightrange* wr = wg->ranges;
            while (wr->weightcount) {
                plog(
                    LL_INFO | LF_DEBUG,
                    "          Skip: %u; Weights (%u): %u ... %u",
                    wr->skip, wr->weightcount, *wr->weights + 1, wr->weights[wr->weightcount - 1] + 1
                );
                ++wr;
            }
            plog(LL_INFO | LF_DEBUG, "          Skip: %u", wr->skip);
        }
    }
    plog(LL_INFO | LF_DEBUG, "  Materials (%u):", m->materialcount);
    for (unsigned i = 0; i < m->materialcount; ++i) {
        struct p3m_material* mat = &m->materials[i];
        plog(LL_INFO | LF_DEBUG, "    Material %u:", i);
        plog(LL_INFO | LF_DEBUG, "      Render mode: %u (%s)", mat->rendmode, ((const char* const[]){"normal", "additive"})[mat->rendmode]);
        if (mat->texture) {
            plog(LL_INFO | LF_DEBUG, "      Texture: %u", (unsigned)(((uintptr_t)mat->texture - (uintptr_t)m->textures) / sizeof(*mat->texture)));
        } else {
            plog(LL_INFO | LF_DEBUG, "      Texture: (None)");
        }
        plog(LL_INFO | LF_DEBUG, "      Color: #%02x%02x%02x%02x", mat->color[0], mat->color[1], mat->color[2], mat->color[3]);
        plog(LL_INFO | LF_DEBUG, "      Emission: #%02x%02x%02x", mat->emission[0], mat->emission[1], mat->emission[2]);
        plog(LL_INFO | LF_DEBUG, "      Shading: %u", mat->shading);
    }
    plog(LL_INFO | LF_DEBUG, "  Texture (%u):", m->texturecount);
    for (unsigned i = 0; i < m->texturecount; ++i) {
        struct p3m_texture* t = &m->textures[i];
        switch ((uint8_t)t->type) {
            case P3M_TEXTYPE_EMBEDDED: {
                if (t->embedded.data) {
                    plog(LL_INFO | LF_DEBUG, "    Texture %u (embedded): %ux%u@%ubpp", i, t->embedded.res, t->embedded.res, t->embedded.ch * 8);
                } else {
                    plog(LL_INFO | LF_DEBUG, "    Texture %u (embedded): (Failed to load)", i);
                }
            } break;
            case P3M_TEXTYPE_EXTERNAL: {
                plog(LL_INFO | LF_DEBUG, "    Texture %u (external): %s", i, t->external.rcpath);
            } break;
        }
    }
    {
        plog(LL_INFO | LF_DEBUG, "  Bones (%u):", m->bonecount);
        struct charbuf cb;
        cb_init(&cb, 16);
        cb_nullterm(&cb);
        struct VLB(uint8_t) chctvlb;
        VLB_INIT(chctvlb, 4, VLB_OOM_NOP);
        for (unsigned i = 0; i < m->bonecount; ++i) {
            struct p3m_bone* b = &m->bones[i];
            plog(LL_INFO | LF_DEBUG, "%s    Bone %u (%s):", cb.data, i, b->name);
            plog(LL_INFO | LF_DEBUG, "%s      Head: (%.3f, %.3f, %.3f)", cb.data, (double)b->head.x, (double)b->head.y, (double)b->head.z);
            plog(LL_INFO | LF_DEBUG, "%s      Tail: (%.3f, %.3f, %.3f)", cb.data, (double)b->tail.x, (double)b->tail.y, (double)b->tail.z);
            if (b->childcount) {
                plog(LL_INFO | LF_DEBUG, "%s      Children (%u):", cb.data, b->childcount);
                VLB_ADD(chctvlb, b->childcount, 3, 2, VLB_OOM_NOP);
                cb_add(&cb, ' ');
                cb_add(&cb, ' ');
                cb_add(&cb, ' ');
                cb_add(&cb, ' ');
                cb_nullterm(&cb);
            }
            if (chctvlb.len && !chctvlb.data[chctvlb.len - 1]--) {
                do {
                    --chctvlb.len;
                    cb.len -= 4;
                } while (chctvlb.len && !chctvlb.data[chctvlb.len - 1]--);
                cb_nullterm(&cb);
            }
        }
        cb_dump(&cb);
        VLB_FREE(chctvlb);
    }
    plog(LL_INFO | LF_DEBUG, "  Animations (%u):", m->animationcount);
    for (unsigned i = 0; i < m->animationcount; ++i) {
        struct p3m_animation* a = &m->animations[i];
        plog(LL_INFO | LF_DEBUG, "    Animation %u (%s):", i, a->name);
        plog(LL_INFO | LF_DEBUG, "      Action references (%u):", a->actioncount);
        for (unsigned j = 0; j < a->actioncount; ++j) {
            struct p3m_animationactref* r = &a->actions[j];
            plog(
                LL_INFO | LF_DEBUG, 
                "        Reference %u: Action: %u; Speed: %.2fx; Range: %u ... %u",
                j, (unsigned)(((uintptr_t)r->action - (uintptr_t)m->actions) / sizeof(*r->action)), (double)r->speed, r->start, r->end
            );
        }
    }
    plog(LL_INFO | LF_DEBUG, "  Actions (%u):", m->actioncount);
    for (unsigned i = 0; i < m->actioncount; ++i) {
        struct p3m_action* a = &m->actions[i];
        plog(LL_INFO | LF_DEBUG, "    Action %u:", i);
        plog(LL_INFO | LF_DEBUG, "      Frame time: %uus", (unsigned)a->frametime);
        plog(
            LL_INFO | LF_DEBUG,
            "      Part list mode: %u (%s)",
            a->partlistmode,
            ((const char* const[]){"defaults + whitelist", "defaults - blacklist", "none + whitelist", "all - blacklist"})[a->partlistmode]
        );
        if (a->partlistlen) {
            plog(LL_INFO | LF_DEBUG, "      Part list: %s ... %s", a->partlist);
        } else {
            plog(LL_INFO | LF_DEBUG, "      Part list: (Empty)");
        }
        plog(LL_INFO | LF_DEBUG, "      Bones (%u):", a->bonecount);
        for (unsigned j = 0; j < a->bonecount; ++j) {
            struct p3m_actionbone* b = &a->bones[j];
            plog(LL_INFO | LF_DEBUG, "        Bone %u (%s):", j, b->name);
            static const char* const interpnames[] = {"none", "linear"};
            plog(LL_INFO | LF_DEBUG, "          Translation keyframes (%u):", b->translcount);
            if (b->translcount) {
                plog(LL_INFO | LF_DEBUG, "            Skips: %u ... %u", *b->translskips, b->translskips[b->translcount - 1]);
                plog(
                    LL_INFO | LF_DEBUG,
                    "            Interpolation modes: %u (%s) ... %u (%s)",
                    *b->translinterps, interpnames[*b->translinterps],
                    b->translinterps[b->translcount - 1], interpnames[b->translinterps[b->translcount - 1]]
                );
                plog(
                    LL_INFO | LF_DEBUG,
                    "            Data: (%.3f, %.3f, %.3f) ... (%.3f, %.3f, %.3f)",
                    (double)b->transldata[0][0],
                    (double)b->transldata[0][1],
                    (double)b->transldata[0][2],
                    (double)b->transldata[b->translcount - 1][0],
                    (double)b->transldata[b->translcount - 1][1],
                    (double)b->transldata[b->translcount - 1][2]
                );
            }
            plog(LL_INFO | LF_DEBUG, "          Rotation keyframes (%u):", b->rotcount);
            if (b->rotcount) {
                plog(LL_INFO | LF_DEBUG, "            Skips: %u ... %u", *b->rotskips, b->rotskips[b->rotcount - 1]);
                plog(
                    LL_INFO | LF_DEBUG,
                    "            Interpolation modes: %u (%s) ... %u (%s)",
                    *b->rotinterps, interpnames[*b->rotinterps],
                    b->rotinterps[b->rotcount - 1], interpnames[b->rotinterps[b->rotcount - 1]]
                );
                plog(
                    LL_INFO | LF_DEBUG,
                    "            Data: (%.3f, %.3f, %.3f) ... (%.3f, %.3f, %.3f)",
                    (double)b->rotdata[0][0],
                    (double)b->rotdata[0][1],
                    (double)b->rotdata[0][2],
                    (double)b->rotdata[b->rotcount - 1][0],
                    (double)b->rotdata[b->rotcount - 1][1],
                    (double)b->rotdata[b->rotcount - 1][2]
                );
            }
            plog(LL_INFO | LF_DEBUG, "          Scale keyframes (%u):", b->scalecount);
            if (b->scalecount) {
                plog(LL_INFO | LF_DEBUG, "            Skips: %u ... %u", *b->rotskips, b->rotskips[b->scalecount - 1]);
                plog(
                    LL_INFO | LF_DEBUG,
                    "            Interpolation modes: %u (%s) ... %u (%s)",
                    *b->rotinterps, interpnames[*b->rotinterps],
                    b->rotinterps[b->scalecount - 1], interpnames[b->rotinterps[b->scalecount - 1]]
                );
                plog(
                    LL_INFO | LF_DEBUG,
                    "            Data: (%.3f, %.3f, %.3f) ... (%.3f, %.3f, %.3f)",
                    (double)b->rotdata[0][0],
                    (double)b->rotdata[0][1],
                    (double)b->rotdata[0][2],
                    (double)b->rotdata[b->scalecount - 1][0],
                    (double)b->rotdata[b->scalecount - 1][1],
                    (double)b->rotdata[b->scalecount - 1][2]
                );
            }
        }
    }
    plog(LL_INFO | LF_DEBUG, "  String table length: %u", (unsigned)strtbl.len);
    #endif

    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG | LF_FUNC, "Done");
    #endif
    return true;

    retfalse:;
    p3m_free(m);
    return false;
}
