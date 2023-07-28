#ifndef AUX_RESOURCE_H
#define AUX_RESOURCE_H

#include <stdint.h>
#include <stdbool.h>

enum restype {
    RES__INVAL,
    RES_ENT,    // entity
    RES_MAP,    // map
    RES_MTL,    // material
    RES_MDL,    // model
    RES_PRP,    // prop
    RES_SCR,    // script
    RES_SND,    // sound
    RES_TEX,    // texture
    RES__COUNT,
};

struct res_ent;
struct res_mtl;
struct res_mdl;
struct res_prp;
struct res_scr;
struct res_snd;
struct res_tex;

struct res_ent {
    struct res_mdl* mdl;    // model
    struct res_scr* scr;    // script
};

enum res_mtl_type {
    RES_MTL_TYPE_SOLID,
    RES_MTL_TYPE_FLUID,
    RES_MTL_TYPE_AIR,
};
struct res_mtl {
    enum res_mtl_type t;    // type
    struct res_tex* tex;    // texture
};

enum res_tex_format {
    RES_TEX_FRMT_RGB = 3,
    RES_TEX_FRMT_RGBA = 4,
};
struct res_tex {
    int w; // width
    int h; // height
    union {
        enum res_tex_format f; // format
        int c; // channels
    };
    const uint8_t* d; // data
};
struct resopt_tex {
    bool needsaplha;
};

#endif
