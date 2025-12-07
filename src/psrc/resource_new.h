#ifndef PSRC_RESOURCE_H
#define PSRC_RESOURCE_H

#include "string.h"
#include "datastream.h"
#include "attribs.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

PACKEDENUM rctype {
    RC_CONFIG,
    RC_FONT,
    RC_MAP,
    RC_MODEL,
    RC_SCRIPT,
    RC_SOUND,
    RC_TEXT,
    RC_TEXTURE,
    RC_VIDEO,
    RC__COUNT,
    RC__DIR = RC__COUNT
};

struct rcdata_config;
struct rcdata_font;
struct rcdata_map;
struct rcdata_model;
struct rcdata_script;
struct rcdata_sound;
struct rcdata_texture;
struct rcdata_values;
struct rcdata_video;

// TODO: rcdata and rcopt structs

enum rcdrive_proto {
    RCDRIVE_PROTO_NULL,
    RCDRIVE_PROTO_FS,    // unsigned flags, const char* path
    RCDRIVE_PROTO_DLFS,  // unsigned flags, const char* path
    RCDRIVE_PROTO_PAF,   // unsigned flags, struct paf*, const char* path
    RCDRIVE_PROTO_REDIR, // unsigned flags, uint32_t drive, const char* path
    RCDRIVE_PROTO_MAPPER
};
#define RCDRIVE_PROTO_FS_FLAG_NODUPPATH (1U << 0)
#define RCDRIVE_PROTO_FS_FLAG_NATIVE    (1U << 1)
#define RCDRIVE_PROTO_FS_FLAG_WRITABLE  (1U << 2)
#define RCDRIVE_PROTO_DLFS_FLAG_NODUPPATH (1U << 0)
#define RCDRIVE_PROTO_PAF_FLAG_NODUPPATH (1U << 0)
#define RCDRIVE_PROTO_REDIR_FLAG_NODUPPATH (1U << 0)

PACKEDENUM rcsrc_type {
    RCSRC_TYPE_DS,
    RCSRC_TYPE_MEM,
    RCSRC_TYPE_FH,
    RCSRC_TYPE_FS
};
struct rcsrc {
    enum rcsrc_type type;
    union {
        struct datastream ds;
        struct {
            void* data;
            size_t size;
            bool free;
        } mem;
        FILE* fh;
        const char* fs;
    };
};

typedef void (*getrc_async_cb)(void* ctx, void* rc);
struct getrc_opt {
    unsigned nocache : 1;
    unsigned havecrc : 1;
    unsigned async : 1;
    unsigned async_usemainthrd : 1;
    unsigned async_nocbifcached : 1;
    uint64_t crc;
    getrc_async_cb async_cb;
    void* async_cb_ctx;
};

struct rcls_ent {
    enum rctype type;
    uint8_t havecrc : 1;
    const char* name;
    const char* ext;
    uint32_t namecrc;
    size_t size;
    uint64_t crc;
};
struct rcls {
    struct rcls_ent* ents;
    uint32_t entct;
    char* names;
};
struct rcinfo {
    uint8_t havecrc : 1;
    const char* ext;
    size_t size;
    uint64_t crc;
};

#define LSRC_FLAG_NAMECRC (1U << 0)
#define LSRC_FLAG_SIZE    (1U << 1)
#define LSRC_FLAG_DLFSCRC (1U << 2)
#define LSRC_FLAG_CRC     (1U << 3)
#define GETRCINFO_FLAG_HAVECRC    (1U << 0)
#define GETRCINFO_FLAG_GETSIZE    (1U << 1)
#define GETRCINFO_FLAG_GETDLFSCRC (1U << 2)
#define GETRCINFO_FLAG_GETCRC     (1U << 3)

#define DELRC_FLAG_HAVECRC (1U << 0)

bool initRcMgr(void);
void runRcMgr(uint64_t t);
void* rcmgr_malloc(size_t);
void* rcmgr_calloc(size_t, size_t);
void* rcmgr_realloc(void*, size_t);
void clRcCache(void);
void quitRcMgr(bool quick);

uint32_t newRcDrive(uint32_t key, const char* name, enum rcdrive_proto, ...);
bool replRcDrive(uint32_t, const char* name, enum rcdrive_proto, ...); // takes newRcDrive flags
void delRcDrive(uint32_t);

bool evalRcPath(uint32_t key, const char* path, uint32_t* outdrive, struct charbuf* outpath);

uint32_t newRcOverlay(uint32_t key, uint32_t srcdrive, const char* srcpath, uint32_t destdrive, const char* destpath);
void delRcOverlay(uint32_t);

bool getRcSrc(enum rctype type, uint32_t key, uint32_t drive, const char* path, enum rcsrc_type srctype, struct rcsrc*);
void freeRcSrc(struct rcsrc*);

void* getRc(enum rctype type, uint32_t key, uint32_t drive, const char* path, struct getrc_opt* opt, const void* rcopt, struct charbuf* err);
void rlsRc(void*);
void lockRc(void*);
#define unlockRc rlsRc

bool lsRc(unsigned typemask, unsigned flags, uint32_t key, uint32_t drive, const char* path, struct rcls*);
void freeRcls(struct rcls*);
bool getRcInfo(enum rctype type, unsigned flags, uint32_t key, uint32_t drive, const char* path, uint64_t crc, struct rcinfo*);

bool delRc(enum rctype type, unsigned flags, uint32_t key, uint32_t drive, const char* path, uint64_t crc);
bool copyRc(enum rctype type, uint32_t key, uint32_t srcdrive, const char* srcpath, uint32_t destdrive, const char* destpath);

uint32_t mapRcFile(uint32_t mapperdrive, const char* path);
uint32_t mapRcDir(uint32_t mapperdrive, const char* path);
//uint32_t mapRcPAF(uint32_t mapperdrive, struct paf*);

#endif
