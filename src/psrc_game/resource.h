#ifndef GAME_RESOURCE_H
#define GAME_RESOURCE_H

#include <stdint.h>
#include <stdbool.h>

enum rctype {
    RC_ENTITY,
    RC_MAP,
    RC_MATERIAL,
    RC_MODEL,
    RC_PROP,
    RC_SCRIPT,
    RC_SOUND,
    RC_TEXTURE,
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

enum rc_sound_frmt {
    RC_SOUND_FRMT_WAV = 1,
    RC_SOUND_FRMT_VORBIS,
};
struct __attribute__((packed)) rc_sound {
    enum rc_sound_frmt format;
    int len; // would be better as unsigned long but stb_vorbis uses int
    const uint8_t* data; // file data for FRMT_VORBIS, audio data converted to AUDIO_S16 for FRMT_WAV
    int rate;
    int channels;
};

struct __attribute__((packed)) rcheader {
    enum rctype type;
    char* path;
    int refs;
    int index;
};

union __attribute__((packed)) resource {
    const void* ptr;
    const struct rc_entity* entity;
    const struct rc_map* map;
    const struct rc_material* material;
    const struct rc_model* model;
    //const struct rc_prop* prop;
    //const struct rc_script* script;
    const struct rc_sound* sound;
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

bool initResource(void);
union resource loadResource(enum rctype type, char* path, union rcopt* opt);
void freeResource(union resource);

#define freeResource(r) freeResource((union resource){.ptr = (void*)(r)})

#endif
