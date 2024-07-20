#ifndef PSRC_COMMON_P3M_H
#define PSRC_COMMON_P3M_H

#include <stdint.h>

#include "../attribs.h"

#define P3M_VER_MAJOR 1
#define P3M_VER_MINOR 1

#pragma pack(push, 1)
struct p3m_vertex {
    float x;
    float y;
    float z;
    float u;
    float v;
};
struct p3m_indexgroup {
    char* texture;
    uint16_t indexcount;
    uint16_t* indices;
};
struct p3m_bonevertex {
    uint16_t index;
    uint16_t weight;
};
struct p3m_bone {
    char* name;
    struct {
        float x;
        float y;
        float z;
    } head;
    struct {
        float x;
        float y;
        float z;
    } tail;
    uint16_t vertexcount;
    struct p3m_bonevertex* vertices;
    uint8_t childcount;
};
struct p3m_actiontranslation {
    float x;
    float y;
    float z;
};
struct p3m_actionrotation {
    float x;
    float y;
    float z;
};
struct p3m_actionscale {
    float x;
    float y;
    float z;
};
PACKEDENUM p3m_actioninterp {
    P3M_ACTINTERP_NONE,
    P3M_ACTINTERP_LINEAR
};
struct p3m_actionbone {
    char* name;
    uint8_t index;
    uint8_t translationcount;
    uint8_t rotationcount;
    uint8_t scalecount;
    uint16_t* frames;
    enum p3m_actioninterp* interps;
    struct p3m_actiontranslation* translations;
    struct p3m_actionrotation* rotations;
    struct p3m_actionscale* scales;
};
struct p3m_action {
    uint16_t maxframe;
    uint8_t bonecount;
    struct p3m_actionbone* bones;
};
#pragma pack(pop)
struct p3m_animation {
    char* name;
    uint32_t frametime;
    uint8_t actioncount;
    uint8_t* actions;
    float* actionspeeds;
    uint16_t* actionstarts;
    uint16_t* actionends;
};

struct p3m {
    uint16_t vertexcount;
    uint8_t indexgroupcount;
    uint8_t bonecount;
    uint8_t actioncount;
    uint8_t animationcount;

    struct p3m_vertex* vertices;

    uint16_t* indices;
    struct p3m_indexgroup* indexgroups;

    struct p3m_bonevertex* boneverts;
    struct p3m_bone* bones;

    uint16_t* actionframes;
    enum p3m_actioninterp* actioninterps;
    struct p3m_actiontranslation* actiontranslations;
    struct p3m_actionrotation* actionrotations;
    struct p3m_actionscale* actionscales;
    struct p3m_actionbone* actionbones;
    struct p3m_action* actions;

    uint8_t* animactions;
    float* animactionspeeds;
    uint16_t* animactionbounds;
    struct p3m_animation* animations;

    char* strings;
};

#define P3M_LOADFLAG_IGNOREVERTS (1 << 0)
#define P3M_LOADFLAG_IGNOREBONES (1 << 1)
#define P3M_LOADFLAG_IGNOREANIMS (1 << 2)

struct p3m* p3m_loadfile(const char*, uint8_t flags);
void p3m_free(struct p3m*);

uint8_t* p3m_newbonemap(struct p3m* from, struct p3m* to);
void p3m_delbonemap(uint8_t*);

#endif
