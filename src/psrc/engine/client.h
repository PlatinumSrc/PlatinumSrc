#ifndef PSRC_ENGINE_CLIENT_H
#define PSRC_ENGINE_CLIENT_H

#include "../attribs.h"
#include "../threading.h"

#include "../common/world.h"
#include "../common/math_vec3.h"
#include "../common/config.h"

#include <stdbool.h>

PACKEDENUM playercameramode {
    PLAYERCAMERAMODE_FIRSTPERSON,
    PLAYERCAMERAMODE_THIRDPERSON
};

struct player {
    struct {
        char* username;
    } priv;
    struct {
        struct cfg config;
        struct {
            float poscorner[3];
            float negcorner[3];
            float mass;
            float drag;
        } box;
        struct {
            struct worldcoord pos;
            float sin[3];
            float cos[3];
            float isin[3];
            float icos[3];
        } cameracalc;
    } common;
    struct playercamera {
        struct {
            uint8_t mode : 1;
            uint8_t fov : 1;
        } changed;
        enum playercameramode mode;
        struct worldcoord pos;
        struct {
            struct {
                uint8_t entity : 1;
                uint8_t bone : 1;
                uint8_t followtail : 1;
                uint8_t inheritentrot : 1;
                uint8_t inheritbonerot : 1;
            } changed;
            char* entity;
            char* bone;
            uint8_t followtail : 1;
            uint8_t drawent : 1;
            uint8_t inheritentrot : 1;
            uint8_t inheritbonerot : 1;
        } follow;
        float rot[3];
        float fov;
        float eyewidth;
        float eyedivergence;
        union {
            struct {
                bool collide;
                float collideradius;
                float mindist;
                float maxdist;
            } thirdperson;
        };
    } camera;
    struct playerscreen {
        struct {
            uint8_t dim : 1;
            uint8_t uicontainer : 1;
        } changed;
        unsigned display;
        float x, y, w, h;
        char** uicontainer;
    } screen;
};

struct playerdata {
    #ifndef PSRC_NOMT
    struct accesslock lock;
    #endif
    struct player* data;
    unsigned len;
    unsigned size;
};

extern struct playerdata playerdata;

static inline void calcClientCameraAngles(struct player* p) {
    vec3_calctrig(p->camera.rot, p->common.cameracalc.sin, p->common.cameracalc.cos);
    vec3_calctrig((float[3]){-p->camera.rot[0], -p->camera.rot[1], -p->camera.rot[2]}, p->common.cameracalc.isin, p->common.cameracalc.icos);
}

bool initClient(void);
void updateClient(float framemult); // assumes playerdata has already been locked by the caller
void quitClient(void);

bool setPlayer(unsigned, const char* username);
struct player* getPlayerForReading(unsigned);
struct player* getPlayerForWriting(unsigned);
void rlsPlayerFromReading(struct player*);
void rlsPlayerFromWriting(struct player*);
void delPlayer(unsigned);

#endif
