#ifndef GAME_RESOURCE_H
#define GAME_RESOURCE_H

#include "../psrc_aux/config.h"

#include <stdint.h>
#include <stdbool.h>

enum rctype {
    RC_CONFIG,
    RC_CONSOLESCRIPT,
    RC_ENTITY,
    RC_GAMESCRIPT,
    RC_MAP,
    RC_MATERIAL,
    RC_MODEL,
    RC_PLAYERMODEL,
    RC_PROP,
    RC_SOUND,
    RC_TEXTURE,
    RC__COUNT,
};

enum rcprefix {
    RCPREFIX_SELF = -1,
    RCPREFIX_COMMON,
    RCPREFIX_ENGINE,
    RCPREFIX_GAME,
    RCPREFIX_MOD,
    RCPREFIX_USER,
    RCPREFIX__COUNT,
};

struct __attribute__((packed)) rc_config {
    struct cfg* config; // not const because it can be modified (the api is mt-safe so it's fine)
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
    //const struct rc_gamescript* gamescript;
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
    long size; // size of data
    const uint8_t* data; // file data for FRMT_VORBIS, audio data converted to AUDIO_S16SYS or AUDIO_S8 for FRMT_WAV
    int len; // length in samples
    int freq;
    uint8_t is8bit : 1; // data is AUDIO_S8 instead of AUDIO_S16SYS for FRMT_WAV
    uint8_t stereo : 1;
};

struct __attribute__((packed)) rcheader {
    enum rctype type;
    char* path; // full file path
    uint32_t pathcrc;
    int refs;
    int index;
};

union __attribute__((packed)) resource {
    const void* ptr;
    //const struct rc_config* config;
    //const struct rc_consolescript* consolescript;
    const struct rc_entity* entity;
    //const struct rc_gamescript* gamescript;
    const struct rc_map* map;
    const struct rc_material* material;
    const struct rc_model* model;
    const struct rc_playermodel* playermodel;
    //const struct rc_prop* prop;
    const struct rc_sound* sound;
    const struct rc_texture* texture;
};

union __attribute__((packed)) rcopt {
    const void* ptr;
    //const struct rcopt_config config;
    //const struct rcopt_consolescript consolescript;
    //const struct rcopt_entity entity;
    //const struct rcopt_gamescript gamescript;
    const struct rcopt_map map;
    const struct rcopt_material material;
    const struct rcopt_model model;
    //const struct rcopt_playermodel playermodel;
    //const struct rcopt_prop prop;
    //const struct rcopt_sound sonud;
    const struct rcopt_texture texture;
};

bool initResource(void);
union resource loadResource(enum rctype type, const char* path, union rcopt* opt);
void freeResource(union resource);

#define loadResource(t, p, o) loadResource((t), (p), (union rcopt){.ptr = (void*)(o)})
#define freeResource(r) freeResource((union resource){.ptr = (void*)(r)})

extern char* maindir;
extern char* userdir;

extern char* gamedir; // relative to maindir

#endif
