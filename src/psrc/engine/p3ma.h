#ifndef PSRC_ENGINE_P3MA_H
#define PSRC_ENGINE_P3MA_H

#include "../common/p3m.h"
#include "../common/resource.h"

enum __attribute__((packed)) p3m_animmode {
    P3MA_ANIMMODE_SET,
    P3MA_ANIMMODE_ADD,
};

#define P3MA_FLAG_ACTIVE (1 << 0)
#define P3MA_FLAG_ADVANCE (1 << 1)
#define P3MA_FLAG_LOOP (1 << 2)

struct __attribute__((packed)) p3m_animstackitem {
    struct rc_model* from;
    uint8_t* bonemap;
    uint8_t animation;
    enum p3m_animmode mode;
    uint8_t flags : 7;
    uint8_t valid : 1;
    uint64_t starttime;
    struct {
        uint8_t act_from;
        uint16_t frame_from;
        uint8_t act_to;
        uint16_t frame_to;
    } cache;
};
struct p3m_animstate {
    struct rc_model* target;
    struct {
        struct p3m_animstackitem* data;
        int len;
        int size;
    } stack;
    uint16_t outsize;
    struct p3m_vertex* out;
};

struct p3ma_animstate* p3m_newanimstate(struct rc_model* target);
void p3ma_delanimstate(struct p3m_animstate*);

int p3ma_newanim(struct p3m_animstate*, int replace, struct rc_model*, uint8_t* bm, char* name, uint64_t t, uint8_t flags);
void p3ma_changeanimflags(struct p3m_animstate*, int, uint8_t disable, uint8_t enable);
void p3ma_delanim(struct p3m_animstate*, int);

struct p3m_vertex* p3ma_animate(struct p3m_animstate*, uint64_t time);

#endif
