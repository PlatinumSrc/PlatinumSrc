#ifndef AUX_RESOURCE_H
#define AUX_RESOURCE_H

#include <stdint.h>
#include <stdbool.h>

enum restype {
    RES__INVAL,
    RES_ENT, // entity
    RES_MAP, // map
    RES_MAT, // material
    RES_MDL, // model
    RES_PRP, // prop
    RES_SCR, // script
    RES_SND, // sound
    RES_TEX, // texture
    RES__COUNT,
};

struct resource;

enum res_tex_format {
    RES_TEX_FRMT_W = 1,
    RES_TEX_FRMT_WA,
    RES_TEX_FRMT_RGB,
    RES_TEX_FRMT_RGBA,
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
enum resopt_tex_quality {
    RESOPT_TEX_QLT_HIGH, // 1x size
    RESOPT_TEX_QLT_MED, // 0.5x size
    RESOPT_TEX_QLT_LOW, // 0.25x size
};
struct resopt_tex {
    bool needsaplha;
    enum resopt_tex_quality quality;
};

struct res_mat {
    float color[3];
    struct resource* tex;
    //struct resource* bumpmap;
};
struct resopt_mat {
    bool needsaplha;
    enum resopt_tex_quality quality;
};

struct res_mdl_part {
    struct resource* mat;
};
struct res_mdl {
    unsigned parts;
    struct res_mdl_part* partdata;
};
struct resopt_mdl {
    enum resopt_tex_quality tex_quality;
};

struct res_ent {
    struct resource* mdl;
    struct resource* scr;
};

struct res_map {
    
};
struct resopt_map {
    enum resopt_tex_quality tex_quality;
};

struct resource {
    enum restype type;
    int refs;
    void* data;
    void* opt;
};

#endif
