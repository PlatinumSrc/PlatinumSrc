#ifndef PSRC_COMMON_RESOURCE_H
#define PSRC_COMMON_RESOURCE_H

#include "config.h"
#include "scripting.h"
#include "p3m.h"

#ifndef MODULE_SERVER
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

// RC_SCRIPT
struct __attribute__((packed)) rc_script {
    struct script script;
};
struct __attribute__((packed)) rcopt_script {
    scriptfunc_t (*findcmd)(char*);
};

// RC_FONT
struct __attribute__((packed)) rc_font {
    #ifndef MODULE_SERVER
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
    RC_SOUND_FRMT_VORBIS,
    #ifdef PSRC_USEMINIMP3
    RC_SOUND_FRMT_MP3,
    #endif
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

struct __attribute__((packed)) rcatt {
    int8_t key;
    void* data;
    void (*cb)(void*);
};

struct __attribute__((packed)) rcheader {
    enum rctype type;
    char* path; // full file path
    uint32_t pathcrc;
    int refs;
    int index;
    struct {
        struct rcatt* data;
        int8_t len;
        int8_t size;
    } att;
};

bool initResource(void);
void quitResource(void);
void* loadResource(enum rctype type, const char* path, void* opt);
void freeResource(void*);
void grabResource(void*);
char* getRcPath(const char* uri, enum rctype type, char** ext);
int8_t genRcAttKey(void);
void setRcAtt(void*, int8_t key, void* data, void (*cb)(void*));
void setRcAttData(void*, int8_t key, void*);
void setRcAttCallback(void*, int8_t key, void (*)(void*));
bool getRcAtt(void*, int8_t key, void** out);
void delRcAtt(void*, int8_t key);

#define releaseResource freeResource

#endif
