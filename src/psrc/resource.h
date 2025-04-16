#ifndef PSRC_RESOURCE_H
#define PSRC_RESOURCE_H

#include "string.h"
#include "versioning.h"

#include "common/config.h"
#include "common/pbasic.h"
#include "common/p3m.h"

#ifndef PSRC_MODULE_SERVER
    #include "../schrift/schrift.h"
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "attribs.h"

PACKEDENUM rcprefix {
    RCPREFIX_INTERNAL,
    RCPREFIX_GAME,
    RCPREFIX_USER,
    RCPREFIX_NATIVE,
    RCPREFIX__COUNT
};

PACKEDENUM rctype {
    RC_CONFIG,
    RC_FONT,
    RC_MAP,
    RC_MODEL,
    RC_SCRIPT,
    RC_SOUND,
    RC_TEXTURE,
    RC_VALUES,
    RC_VIDEO,
    RC__COUNT,
    RC__DIR = RC__COUNT
};

struct rc_config;
struct rc_font;
struct rc_map;
struct rc_model;
struct rc_script;
struct rc_sound;
struct rc_texture;
struct rc_values;
struct rc_video;

//struct rcopt_config;
//struct rcopt_font;
struct rcopt_map;
    PACKEDENUM rcopt_map_loadsect {
        RCOPT_MAP_LOADSECT_ALL,
        RCOPT_MAP_LOADSECT_CLIENT,
        RCOPT_MAP_LOADSECT_SERVER,
    };
struct rcopt_model;
struct rcopt_script;
struct rcopt_sound;
struct rcopt_texture;
    enum rcopt_texture_qlt {
        RCOPT_TEXTURE_QLT_HIGH, // 1x size
        RCOPT_TEXTURE_QLT_MED, // 0.5x size
        RCOPT_TEXTURE_QLT_LOW // 0.25x size
    };
//struct rcopt_values;
//struct rcopt_video;

PACKEDENUM rcsourcetype {
    RCSRCTYPE_FS
};
struct rcsource {
    enum rcsourcetype type;
    union {
        struct {
            char* path;
        } fs;
    };
};

// RC_CONFIG
struct rc_config {
    struct cfg config;
};

// RC_FONT
struct rc_font {
    #ifndef PSRC_MODULE_SERVER
    SFT_Font* font;
    #endif
};

// RC_MAP
struct rc_map {
    int _placeholder;
};
#pragma pack(push, 1)
struct rcopt_map {
    enum rcopt_map_loadsect loadsections;
    enum rcopt_texture_qlt texture_quality;
};
#pragma pack(pop)

// RC_MODEL
struct rc_model {
    struct p3m model;
};
#pragma pack(push, 1)
struct rcopt_model {
    uint8_t flags;
};
#pragma pack(pop)

// RC_SCRIPT
struct rc_script {
    struct pbscript script;
};
struct rcopt_script {
    struct pbc_opt compileropt;
};

// RC_SOUND
PACKEDENUM rc_sound_frmt {
    RC_SOUND_FRMT_WAV,
    #ifdef PSRC_USESTBVORBIS
    RC_SOUND_FRMT_VORBIS,
    #endif
    #ifdef PSRC_USEMINIMP3
    RC_SOUND_FRMT_MP3
    #endif
};
struct rc_sound {
    enum rc_sound_frmt format;
    long size; // size of data in bytes
    uint8_t* data; // file data for FRMT_VORBIS and FRMT_MP3, audio data converted to I16 or U8 for FRMT_WAV
    long len; // length in samples
    unsigned freq;
    uint8_t channels;
    uint8_t stereo : 1;
    uint8_t is8bit : 1; // data is U8 instead of I16 for FRMT_WAV
};
#pragma pack(push, 1)
struct rcopt_sound {
    bool decodewhole;
};
#pragma pack(pop)

// RC_TEXTURE
enum rc_texture_frmt {
    RC_TEXTURE_FRMT_RGB = 3,
    RC_TEXTURE_FRMT_RGBA
};
struct rc_texture {
    unsigned width;
    unsigned height;
    union {
        PACKEDENUM rc_texture_frmt format;
        uint8_t channels;
    };
    uint8_t* data;
};
#pragma pack(push, 1)
struct rcopt_texture {
    bool needsalpha;
    PACKEDENUM rcopt_texture_qlt quality;
};
#pragma pack(pop)

// RC_VALUES
struct rc_values {
    struct cfg values;
};

// RC_VIDEO
struct rc_video {
    struct rcsource src;
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
    const char* name;
    uint32_t namecrc;
    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    const uint8_t* dirbits;
    #endif
};
struct rcls {
    char* names;
    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    uint8_t* dirbmp;
    #endif
    size_t nameslen;
    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    size_t dirbmplen;
    #endif
    size_t count[RC__DIR + 1];
    struct rcls_file* files[RC__DIR + 1];
};

bool initRcMgr(void);
void runRcMgr(uint64_t t);
void* rcmgr_malloc(size_t);
void* rcmgr_calloc(size_t, size_t);
void* rcmgr_realloc(void*, size_t);
void clRcCache(void);
void quitRcMgr(bool quick);

#define LOADRC_FLAG_ALLOWNATIVE (1 << 0)

void* getRc(enum rctype type, const char* id, const void* opt, unsigned flags, struct charbuf* err);
void rlsRc(void*, bool force);
void lockRc(void*);
#define unlockRc(r) rlsRc(r, false)

bool lsRc(const char* id, bool allownative, struct rcls*);
bool lsCacheRc(const char* id, bool allownative, struct rcls* l);
void freeRcls(struct rcls*);

void loadMods(const char* const* names, int count);
struct modinfo* queryMods(int* count);
void freeModList(struct modinfo*);
void setCustomFiles(struct customfile*, int count);

#endif
