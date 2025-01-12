#ifndef PSRC_COMMON_P3M_H
#define PSRC_COMMON_P3M_H

#include <stdint.h>

#include "../datastream.h"

#include "../attribs.h"

#define P3M_VER 0

//#pragma pack(push, 1)
//#pragma pack(pop)

// struct member order is sorted by type size

struct p3m;
struct p3m_part;
struct p3m_vertex;
struct p3m_normal;
struct p3m_weightgroup;
struct p3m_weightrange;
struct p3m_material;
    PACKEDENUM p3m_matrendmode {
        P3M_MATRENDMODE_NORMAL,
        P3M_MATRENDMODE_ADD,
        P3M_MATRENDMODE__COUNT
    };
struct p3m_texture;
    PACKEDENUM p3m_textype {
        P3M_TEXTYPE_EMBEDDED,
        P3M_TEXTYPE_EXTERNAL,
        P3M_TEXTYPE__COUNT
    };
struct p3m_bone;
struct p3m_animation;
struct p3m_animationactref;
struct p3m_action;
    PACKEDENUM p3m_actpartlistmode {
        P3M_ACTPARTLISTMODE_DEFAULTWHITE,
        P3M_ACTPARTLISTMODE_DEFAULTBLACK,
        P3M_ACTPARTLISTMODE_WHITE,
        P3M_ACTPARTLISTMODE_BLACK,
        P3M_ACTPARTLISTMODE__COUNT
    };
struct p3m_actionbone;
    PACKEDENUM p3m_actinterp {
        P3M_ACTINTERP_NONE,
        P3M_ACTINTERP_LINEAR,
        P3M_ACTINTERP__COUNT
    };

struct p3m {
    uint8_t vismask[32];
    struct p3m_part* parts;
    struct p3m_material* materials;
    struct p3m_texture* textures;
    struct p3m_bone* bones;
    struct p3m_animation* animations;
    struct p3m_action* actions;
    char* strings;
    uint8_t partcount;
    uint8_t materialcount;
    uint8_t texturecount;
    uint8_t bonecount;
    uint8_t animationcount;
    uint8_t actioncount;
};

struct p3m_part {
    char* name; // pointer in 'strings' of 'struct p3m'
    struct p3m_material* material; // pointer in 'materials' of 'struct p3m'
    struct p3m_vertex* vertices;
    struct p3m_normal* normals;
    uint16_t* indices;
    struct p3m_weightgroup* weightgroups; // owned by first
    uint16_t vertexcount;
    uint16_t indexcount;
    uint8_t weightgroupcount;
};
#pragma pack(push, 1)
struct p3m_vertex {
    float x, y, z;
    float u, v;
};
struct p3m_normal {
    float x, y, z;
};
#pragma pack(pop)
struct p3m_weightgroup {
    char* name; // pointer in 'strings' of 'struct p3m'
    struct p3m_weightrange* ranges; // owned by first of each p3m_part
};
struct p3m_weightrange {
    uint8_t* weights; // owned by first
    uint16_t skip;
    uint16_t weightcount;
};

struct p3m_material {
    struct p3m_texture* texture; // pointer in 'textures' of 'struct p3m'
    enum p3m_matrendmode rendmode;
    uint8_t color[4];
    uint8_t emission[3];
    uint8_t shading;
};

struct p3m_texture {
    union {
        struct {
            uint8_t* data;
            uint16_t res;
            uint8_t ch;
        } embedded;
        struct {
            char* rcpath;
        } external;
    };
    enum p3m_textype type; 
};

struct p3m_bone {
    char* name; // pointer in 'strings' of 'struct p3m'
    struct {
        float x, y, z;
    } head;
    struct {
        float x, y, z;
    } tail;
    uint8_t childcount;
};

struct p3m_animation {
    char* name; // pointer in 'strings' of 'struct p3m'
    struct p3m_animationactref* actions; // owned by first
    uint8_t actioncount;
};
struct p3m_animationactref {
    struct p3m_action* action; // pointer in 'actions' of 'struct p3m'
    float speed;
    uint16_t start;
    uint16_t end;
};

struct p3m_action {
    char** partlist; // owned by first, pointers in 'strings' of 'struct p3m'
    struct p3m_actionbone* bones; // owned by first
    uint32_t frametime;
    enum p3m_actpartlistmode partlistmode;
    uint8_t partlistlen;
    uint8_t bonecount;
};
struct p3m_actionbone {
    char* name; // pointer in 'strings' of 'struct p3m'
    uint8_t* translskips;
    uint8_t* rotskips; // pointer in 'translskips'
    uint8_t* scaleskips; // pointer in 'translskips'
    enum p3m_actinterp* translinterps;
    enum p3m_actinterp* rotinterps; // pointer in 'translinterps'
    enum p3m_actinterp* scaleinterps; // pointer in 'translinterps'
    float (*transldata)[3];
    float (*rotdata)[3]; // pointer in 'transldata'
    float (*scaledata)[3]; // pointer in 'transldata'
    uint8_t translcount;
    uint8_t rotcount;
    uint8_t scalecount;
};

#define P3M_LOADFLAG_IGNOREGEOM    (1 << 0) // ignore verts, norms, inds, weights, mats, and textures
#define P3M_LOADFLAG_IGNORENORMS   (1 << 1) // ignore norms
#define P3M_LOADFLAG_IGNOREEMBTEXS (1 << 2) // ignore embedded textures
#define P3M_LOADFLAG_IGNORESKEL    (1 << 3) // ignore weights and bones
#define P3M_LOADFLAG_IGNOREANIMS   (1 << 4) // ignore anims and acts

#define P3M_FILEFLAG_PART_HASNORMS (1 << 0) 

bool p3m_load(struct datastream*, uint8_t flags, struct p3m*);
void p3m_free(struct p3m*);

uint8_t* p3m_newbonemap(struct p3m* from, struct p3m* to);
void p3m_delbonemap(uint8_t*);

#endif
