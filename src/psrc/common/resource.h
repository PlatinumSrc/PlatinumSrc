#ifndef PSRC_COMMON_RESOURCE_H
#define PSRC_COMMON_RESOURCE_H

// TODO: rewrite? (too much crust)

#include "config.h"
#include "pbasic.h"
#include "p3m.h"
#include "string.h"

#ifndef PSRC_MODULE_SERVER
    #include "../../schrift/schrift.h"
#endif

#include <stdint.h>
#include <stdbool.h>

enum __attribute__((packed)) rctype {
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

enum __attribute__((packed)) rcprefix {
    RCPREFIX_SELF = -1,
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

// RC_SCRIPT
struct rc_script {
    struct pb_script script;
};
struct rcopt_script {
    struct pbc_opt compileropt;
};

// RC_FONT
struct __attribute__((packed)) rc_font {
    #ifndef PSRC_MODULE_SERVER
    SFT_Font* font;
    #endif
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
enum __attribute__((packed)) rc_sound_frmt {
    RC_SOUND_FRMT_WAV,
    RC_SOUND_FRMT_VORBIS,
    #ifdef PSRC_USEMINIMP3
    RC_SOUND_FRMT_MP3
    #endif
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
    uint8_t sdlfree : 1; // use SDL_FreeWAV
};
struct __attribute__((packed)) rcopt_sound {
    bool decodewhole;
};

// RC_MODEL
struct __attribute__((packed)) rc_model {
    struct p3m* model;
    struct rc_texture** textures;
};
struct __attribute__((packed)) rcopt_model {
    uint8_t flags;
    enum rcopt_texture_qlt texture_quality;
};

// RC_MAP
struct __attribute__((packed)) rc_map {
    
};
enum __attribute__((packed)) rcopt_map_loadsect {
    RCOPT_MAP_LOADSECT_ALL,
    RCOPT_MAP_LOADSECT_CLIENT,
    RCOPT_MAP_LOADSECT_SERVER,
};
struct __attribute__((packed)) rcopt_map {
    enum rcopt_map_loadsect loadsections;
    enum rcopt_texture_qlt texture_quality;
};

struct rcheader {
    enum rctype type;
    char* path; // full file path
    uint32_t pathcrc;
    bool hasdatacrc;
    uint64_t datacrc;
    int refs;
    int index;
};

struct customfile {
    char* path;
    uint64_t crc;
};

bool initResource(void);
void quitResource(void);
void* loadResource(enum rctype type, const char* path, void* opt, struct charbuf* err);
void freeResource(void*);
void grabResource(void*);
#define releaseResource freeResource
char* getRcPath(const char* uri, enum rctype type, char** ext);
void loadMods(const char* const* names, int count);
char** queryMods(int* count);
void setCustomFiles(struct customfile*, int count);

#endif
