#ifndef PSRC_COMMON_P3M_H
#define PSRC_COMMON_P3M_H

#include <stdint.h>

#define P3M_VER_MAJOR 1
#define P3M_VER_MINOR 0

struct __attribute__((packed)) p3m_vertex {
    float x;
    float y;
    float z;
};
struct __attribute__((packed)) p3m_texturevertex {
    uint8_t t;
    float u;
    float v;
};
struct __attribute__((packed)) p3m_texture {
    char* path;
};
struct __attribute__((packed)) p3m_bonevertex {
    uint16_t index;
    float weight;
};
struct __attribute__((packed)) p3m_bone {
    char* name;
    struct __attribute__((packed)) {
        float x;
        float y;
        float z;
    } head;
    struct __attribute__((packed)) {
        float x;
        float y;
        float z;
    } tail;
    uint16_t vertexcount;
    struct p3m_bonevertex* vertices;
    uint8_t childcount;
};
struct __attribute__((packed)) p3m_actiontranslation {
    float frame;
    float x;
    float y;
    float z;
};
struct __attribute__((packed)) p3m_actionrotation {
    float frame;
    float x;
    float y;
    float z;
};
struct __attribute__((packed)) p3m_actionscale {
    float frame;
    float x;
    float y;
    float z;
};
struct __attribute__((packed)) p3m_actionbone {
    char* name;
    uint8_t translationcount;
    struct p3m_actiontranslation* translations;
    uint8_t rotationcount;
    struct p3m_actionrotation* rotations;
    uint8_t scalecount;
    struct p3m_actionscale* scales;
};
struct __attribute__((packed)) p3m_action {
    float maxframe;
    uint8_t bonecount;
    struct p3m_actionbone* bones;
    uint8_t* boneindices;
};
struct __attribute__((packed)) p3m_animation {
    char* name;
    uint32_t frametime;
    uint8_t actioncount;
    struct p3m_action* actions;
};

struct p3m {
    uint16_t vertexcount;
    struct p3m_vertex* vertices;
    struct p3m_texturevertex* texturevertices;
    uint16_t indexcount;
    uint16_t* indices;
    uint8_t texturecount;
    struct p3m_texture* textures;
    uint8_t bonecount;
    struct p3m_bone* bones;
    uint8_t actioncount;
    struct p3m_action* actions;
    uint8_t animationcount;
    struct p3m_animation* animations;
    uint8_t* data;
};

enum __attribute__((packed)) p3m_animmode {
    ANIMMODE_SET,
    ANIMMODE_ADD,
};

#define P3M_ANIMFLAG_ACTIVE (1 << 0)
#define P3M_ANIMFLAG_ADVANCE (1 << 1)
#define P3M_ANIMFLAG_LOOP (1 << 2)

struct __attribute__((packed)) p3m_animstackitem {
    struct p3m* from;
    uint8_t* bonemap;
    uint8_t animation;
    enum p3m_animmode mode;
    uint8_t flags : 7;
    uint8_t valid : 1;
    uint64_t starttime;
    struct {
        uint8_t act_from;
        uint8_t frame_from;
        uint8_t act_to;
        uint8_t frame_to;
    } cache;
};
struct p3m_animstate {
    struct p3m* target;
    int stacklen;
    struct p3m_animstackitem* stack;
    uint16_t outsize;
    struct p3m_vertex* out;
};

struct p3m* p3m_loadfile(const char*);
void p3m_free(struct p3m*);

uint8_t* p3m_newbonemap(struct p3m* from, struct p3m* to);
void p3m_delbonemap(uint8_t*);

struct p3m_animstate* p3m_newanimstate(struct p3m*);
void p3m_delanimstate(struct p3m_animstate*);

int p3m_newanim(struct p3m_animstate*, struct p3m*, uint8_t* bm, char* name, uint64_t t, uint8_t f);
int p3m_swapanim(struct p3m_animstate*, int, struct p3m*, uint8_t* bm, char* name, uint64_t t, uint8_t f);
void p3m_changeanimflags(struct p3m_animstate*, int, uint8_t disable, uint8_t enable);
void p3m_delanim(struct p3m_animstate*, int);

struct p3m_vertex* p3m_animate(struct p3m_animstate*, uint64_t time);

#endif
