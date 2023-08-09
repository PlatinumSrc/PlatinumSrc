#ifndef AUX_RESOURCE_H
#define AUX_RESOURCE_H

#include <stdint.h>
#include <stdbool.h>

enum rctype {
    RC__INVAL,
    RC_ENTITY,
    RC_MAP,
    RC_MATERIAL,
    RC_MODEL,
    RC_PROP,
    RC_SCRIPT,
    RC_SOUND,
    RC_TEXTURE,
    RC__COUNT,
};

enum rc_texture_frmt {
    RC_TEXTURE_FRMT_W = 1, // monochrome
    RC_TEXTURE_FRMT_WA, // monochrome with alpha
    RC_TEXTURE_FRMT_RGB,
    RC_TEXTURE_FRMT_RGBA,
};
struct __attribute__((packed)) rc_texture {
    int width;
    int height;
    union {
        enum rc_texture_frmt format;
        int channels;
    };
    const uint8_t* data;
};
enum rcopt_texture_qlt {
    RCOPT_TEXTURE_QLT_HIGH, // 1x size
    RCOPT_TEXTURE_QLT_MED, // 0.5x size
    RCOPT_TEXTURE_QLT_LOW, // 0.25x size
};
struct __attribute__((packed)) rcopt_texture {
    bool needsaplha;
    enum rcopt_texture_qlt quality;
};

struct __attribute__((packed)) rc_material {
    float color[3];
    const struct rc_texture* texture;
    //const struct rc_texture* bumpmap;
};
struct __attribute__((packed)) rcopt_material {
    bool needsaplha;
    enum rcopt_texture_qlt quality;
};

struct __attribute__((packed)) rc_model_part {
    const struct rc_material* material;
};
struct __attribute__((packed)) rc_model {
    unsigned parts;
    const struct rc_model_part* partdata;
};
struct __attribute__((packed)) rcopt_model {
    enum rcopt_texture_qlt texture_quality;
};

struct __attribute__((packed)) rc_entity {
    const struct rc_model* model;
    const struct rc_script* script;
};

struct __attribute__((packed)) rc_map {
    
};
enum rcopt_map_loadsect {
    RCOPT_MAP_LOADSECT_ALL,
    RCOPT_MAP_LOADSECT_CLIENT,
    RCOPT_MAP_LOADSECT_SERVER,
};
struct __attribute__((packed)) rcopt_map {
    enum rcopt_map_loadsect loadsections;
    enum rcopt_texture_qlt texture_quality;
};

struct __attribute__((packed)) rcheader {
    enum rctype type;
    char* path;
    int refs;
    uint64_t lastuse; // last time a load or free was performed
    int index;
    uint64_t id;
};

union __attribute__((packed)) resource {
    const void* ptr;
    const struct rc_entity* entity;
    const struct rc_map* map;
    const struct rc_material* material;
    const struct rc_model* model;
    //const struct rc_prop* prop;
    //const struct rc_script* script;
    //const struct rc_sound* sound;
    const struct rc_texture* texture;
};

union __attribute__((packed)) rcopt {
    const void* ptr;
    //const struct rcopt_entity entity;
    const struct rcopt_map map;
    const struct rcopt_material material;
    const struct rcopt_model model;
    //const struct rcopt_prop prop;
    //const struct rcopt_script script;
    //const struct rcopt_sound sonud;
    const struct rcopt_texture texture;
};

union resource loadResource(enum rctype type, char* path, union rcopt* opt);
void freeResource_internal(union resource);

#define freeResource(r) freeResource_internal((union resource){.ptr = (r)})

#include "resource_reap.h"

#endif
