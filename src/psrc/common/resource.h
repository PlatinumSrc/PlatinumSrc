#ifndef PSRC_COMMON_RESOURCE_H
#define PSRC_COMMON_RESOURCE_H

// TODO: rewrite? (too much crust)

#include "config.h"
#include "pbasic.h"
#include "p3m.h"
#include "string.h"
#include "versioning.h"

#ifndef PSRC_MODULE_SERVER
    #include "../../schrift/schrift.h"
#endif

#include <stdint.h>
#include <stdbool.h>

#include "../attribs.h"

PACKEDENUM rctype {
    RC_CONFIG,
    RC_FONT,
    RC_MAP,
    RC_MODEL,
    RC_SCRIPT,
    RC_SOUND,
    RC_TEXTURE,
    RC_VALUES,
    RC__COUNT,
    RC__DIR = RC__COUNT
};

PACKEDENUM rcprefix {
    RCPREFIX_SELF = -1,
    RCPREFIX_INTERNAL,
    RCPREFIX_GAME,
    RCPREFIX_USER,
    RCPREFIX__COUNT
};

// RC_CONFIG
struct rc_config {
    struct cfg* config;
};

// RC_VALUES
struct rc_values {
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
struct rc_font {
    #ifndef PSRC_MODULE_SERVER
    SFT_Font* font;
    #endif
};

// RC_TEXTURE
enum rc_texture_frmt {
    RC_TEXTURE_FRMT_RGB = 3,
    RC_TEXTURE_FRMT_RGBA
};
struct rc_texture {
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
    RCOPT_TEXTURE_QLT_LOW // 0.25x size
};
#pragma pack(push, 1)
struct rcopt_texture {
    bool needsalpha;
    PACKEDENUM rcopt_texture_qlt quality;
};
#pragma pack(pop)

// RC_SOUND
PACKEDENUM rc_sound_frmt {
    RC_SOUND_FRMT_WAV,
    RC_SOUND_FRMT_VORBIS,
    #ifdef PSRC_USEMINIMP3
    RC_SOUND_FRMT_MP3
    #endif
};
struct rc_sound {
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
#pragma pack(push, 1)
struct rcopt_sound {
    bool decodewhole;
};
#pragma pack(pop)

// RC_MODEL
struct rc_model {
    struct p3m* model;
    struct rc_texture** textures;
};
#pragma pack(push, 1)
struct rcopt_model {
    uint8_t flags;
    PACKEDENUM rcopt_texture_qlt texture_quality;
};
#pragma pack(pop)

// RC_MAP
struct rc_map {
    int _placeholder;
};
PACKEDENUM rcopt_map_loadsect {
    RCOPT_MAP_LOADSECT_ALL,
    RCOPT_MAP_LOADSECT_CLIENT,
    RCOPT_MAP_LOADSECT_SERVER,
};
#pragma pack(push, 1)
struct rcopt_map {
    enum rcopt_map_loadsect loadsections;
    enum rcopt_texture_qlt texture_quality;
};
#pragma pack(pop)

struct rcheader {
    enum rctype type;
    enum rcprefix prefix;
    char* rcpath; // sanitized resource path without prefix (e.g. /textures/icon.png)
    char* path; // full file path (e.g. /usr/share/games/psrc/internal/resources/textures/icon.png)
    uint32_t rcpathcrc;
    uint32_t pathcrc;
    bool hasdatacrc;
    uint64_t datacrc;
    unsigned refs;
    unsigned index;
};

struct customfile {
    char* path;
    uint64_t crc;
};

struct modinfo {
    char* path;
    char* dir;
    char* name;
    char* author;
    char* desc;
    struct version version;
};

struct rcls_file {
    char* name;
    uint32_t namecrc;
};
struct rcls {
    char* names;
    unsigned nameslen;
    unsigned count[RC__DIR + 1];
    struct rcls_file* files[RC__DIR + 1];
};

bool initResource(void);
void quitResource(void);
void* loadResource(enum rctype type, const char* uri, void* opt, struct charbuf* err);
void freeResource(void*);
void grabResource(void*);
#define releaseResource freeResource
char* getRcPath(enum rctype type, const char* uri, enum rcprefix* p, char** rcpath, const char** ext);
bool lsRc(const char* uri, struct rcls*);
void freeRcls(struct rcls*);
void loadMods(const char* const* names, int count);
struct modinfo* queryMods(int* count);
void freeModList(struct modinfo*);
void setCustomFiles(struct customfile*, int count);

#endif
