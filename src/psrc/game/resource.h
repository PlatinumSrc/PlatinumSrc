#ifndef PSRC_GAME_RESOURCE_H
#define PSRC_GAME_RESOURCE_H

#include "../aux/config.h"

#include "../../schrift/schrift.h"

#include <stdint.h>
#include <stdbool.h>

enum rctype {
    RC_CONFIG,
    RC_FONT,
    RC_MAP,
    RC_MATERIAL,
    RC_MODEL,
    RC_PLAYERMODEL,
    RC_SCRIPT,
    RC_SOUND,
    RC_TEXTURE,
    RC_VALUES,
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

// RC_CONFIG
struct __attribute__((packed)) rc_config {
    struct cfg* config;
};

// RC_VALUES
struct __attribute__((packed)) rc_values {
    struct cfg* values;
};

// RC_FONT
struct __attribute__((packed)) rc_font {
    SFT_Font* font;
};

// RC_TEXTURE
enum rc_texture_frmt {
    RC_TEXTURE_FRMT_RGB = 3,
    RC_TEXTURE_FRMT_RGBA,
};
struct __attribute__((packed)) rc_texture {
    int width;
    int height;
    union {
        enum rc_texture_frmt format;
        int channels;
    };
    uint8_t* data;
};
enum rcopt_texture_qlt {
    RCOPT_TEXTURE_QLT_HIGH, // 1x size
    RCOPT_TEXTURE_QLT_MED, // 0.5x size
    RCOPT_TEXTURE_QLT_LOW, // 0.25x size
};
struct __attribute__((packed)) rcopt_texture {
    bool needsalpha;
    enum rcopt_texture_qlt quality;
};

// RC_MATERIAL
struct __attribute__((packed)) rc_material {
    float color[4]; // RGBA
    struct rc_texture* texture;
    //struct rc_texture* bumpmap;
};
struct __attribute__((packed)) rcopt_material {
    enum rcopt_texture_qlt quality;
};

// RC_SOUND
enum rc_sound_frmt {
    RC_SOUND_FRMT_VORBIS,
    RC_SOUND_FRMT_MP3,
    RC_SOUND_FRMT_WAV,
};
struct __attribute__((packed)) rc_sound {
    enum rc_sound_frmt format;
    long size; // size of data in bytes
    uint8_t* data; // file data for FRMT_VORBIS, audio data converted to AUDIO_S16SYS or AUDIO_S8 for FRMT_WAV
    int len; // length in samples
    int freq;
    int channels;
    uint8_t is8bit : 1; // data is AUDIO_S8 instead of AUDIO_S16SYS for FRMT_WAV
    uint8_t stereo : 1;
};
struct __attribute__((packed)) rcopt_sound {
    bool decodewhole;
};

// RC_MODEL
struct __attribute__((packed)) rc_model_part {
    struct rc_material* material;
};
struct __attribute__((packed)) rc_model {
    unsigned parts;
    struct rc_model_part* partdata;
};
struct __attribute__((packed)) rcopt_model {
    enum rcopt_texture_qlt texture_quality;
};

// RC_MAP
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
    char* path; // full file path
    uint32_t pathcrc;
    int refs;
    int index;
};

union __attribute__((packed)) resource {
    void* ptr;
    struct rc_config* config;
    struct rc_font* font;
    struct rc_map* map;
    struct rc_material* material;
    struct rc_model* model;
    //struct rc_playermodel* playermodel;
    //struct rc_script* script;
    struct rc_sound* sound;
    struct rc_texture* texture;
    struct rc_values* values;
};

union __attribute__((packed)) rcopt {
    void* ptr;
    //struct rcopt_config* config;
    //struct rcopt_font* font;
    struct rcopt_map* map;
    struct rcopt_material* material;
    struct rcopt_model* model;
    //struct rcopt_playermodel* playermodel;
    //struct rcopt_script* script;
    struct rcopt_sound* sound;
    struct rcopt_texture* texture;
    //struct rcopt_values* values;
};

bool initResource(void);
void termResource(void);
union resource loadResource(enum rctype type, const char* path, union rcopt opt);
void freeResource(union resource);
void grabResource(union resource);
char* getRcPath(const char* uri, enum rctype type, char** ext);

#define loadResource(t, p, o) loadResource((t), (p), (union rcopt){.ptr = (void*)(o)})
#define freeResource(r) freeResource((union resource){.ptr = (void*)(r)})
#define grabResource(r) grabResource((union resource){.ptr = (void*)(r)})
#define releaseResource freeResource

#endif
