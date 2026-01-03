#ifndef PSRC_RESOURCE_H
#define PSRC_RESOURCE_H

#include "string.h"
#include "datastream.h"
#include "attribs.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

PACKEDENUM rsrc_type {
    RSRC_CONFIG,
    RSRC_FONT,
    RSRC_MAP,
    RSRC_MODEL,
    RSRC_SCRIPT,
    RSRC_SOUND,
    RSRC_TEXT,
    RSRC_TEXTURE,
    RSRC_VIDEO,
    RSRC__COUNT,
    RSRC__DIR = RSRC__COUNT
};
PACKEDENUM rsrc_subtype {
    RSRC_CONFIG_CFG = 0,
    RSRC_FONT_TTF = 0,
    RSRC_FONT_OTF,
    RSRC_MAP_PMF = 0,
    RSRC_MODEL_P3M = 0,
    RSRC_SCRIPT_BAS = 0,
    RSRC_SOUND_OGG = 0,
    RSRC_SOUND_MP3,
    RSRC_SOUND_WAV,
    RSRC_TEXT_TXT = 0,
    RSRC_TEXTURE_PTF = 0,
    RSRC_TEXTURE_PNG,
    RSRC_TEXTURE_JPG,
    RSRC_TEXTURE_TGA,
    RSRC_TEXTURE_BMP,
    RSRC_VIDEO_MPG = 0,
};

struct rsrc_data_config;
struct rsrc_data_font;
struct rsrc_data_map;
struct rsrc_data_model;
struct rsrc_data_script;
struct rsrc_data_sound;
struct rsrc_data_texture;
struct rsrc_data_values;
struct rsrc_data_video;

// TODO: rsrc_data and rsrc_opt structs

enum rsrc_drive_proto_type {
    RSRC_DRIVE_PROTO_NULL,
    RSRC_DRIVE_PROTO_FS,    // unsigned flags, const char* path
    //RSRC_DRIVE_PROTO_PAF,   // unsigned flags, struct paf*, const char* path
    RSRC_DRIVE_PROTO_REDIR, // unsigned flags, uint32_t drive, const char* path
    RSRC_DRIVE_PROTO_MAPPER
};
struct rsrc_drive_proto_params {
    enum rsrc_drive_proto_type type;
    struct {
        unsigned flags;
        const char* path;
    } fs;
    //struct {
    //    unsigned flags;
    //    struct paf* paf;
    //    const char* path;
    //} paf;
    struct {
        unsigned flags;
        uint32_t drive;
        const char* path;
    } redir;
    struct {
        char placeholder; // no options yet
    } mapper;
};
#define RSRC_DRIVE_PROTO_FS_NODUPPATH  (1U << 0)
#define RSRC_DRIVE_PROTO_FS_FREEPATH   (1U << 1)
#define RSRC_DRIVE_PROTO_FS_NATIVE     (1U << 2)
#define RSRC_DRIVE_PROTO_FS_WRITABLE   (1U << 3)
#define RSRC_DRIVE_PROTO_DLFS_NODUPPATH  (1U << 0)
#define RSRC_DRIVE_PROTO_DLFS_FREEPATH   (1U << 1)
#define RSRC_DRIVE_PROTO_PAF_NODUPPATH  (1U << 0)
#define RSRC_DRIVE_PROTO_PAF_FREEPATH   (1U << 1)
#define RSRC_DRIVE_PROTO_PAF_WRITABLE   (1U << 2)
#define RSRC_DRIVE_PROTO_PAF_CRCSTRUCT  (1U << 3)
#define RSRC_DRIVE_PROTO_REDIR_NODUPPATH  (1U << 0)
#define RSRC_DRIVE_PROTO_REDIR_FREEPATH   (1U << 1)

struct rsrc_overlay_params {
    uint32_t key;
    uint32_t srcdrivekey;
    uint32_t srcdrive;
    const char* srcpath;
    size_t srcpathlen;     // -1 for strlen()
    uint32_t destdrivekey;
    uint32_t destdrive;
    const char* destpath;
    size_t destpathlen;    // -1 for strlen()
};

PACKEDENUM rsrc_src_type {
    RSRC_SRC_MEM,
    //RSRC_SRC_PAF,
    RSRC_SRC_FS
};
struct rsrc_src {
    enum rsrc_src_type rsrctype;
    enum rsrc_subtype rsrcsubtype;
    union {
        struct {
            void* data;
            size_t size;
        } mem;
        //struct {
        //    struct paf* paf;
        //    const char* path;
        //} paf;
        const char* fs;
    };
};

PACKEDENUM rsrc_raw_type {
    RSRC_RAW_MEM,
    RSRC_RAW_DS,
    RSRC_RAW_F
};
struct rsrc_raw {
    enum rsrc_raw_type type;
    union {
        struct {
            void* data;
            size_t size;
            bool free;
        } mem;
        struct datastream ds;
        FILE* f;
    };
};

typedef void (*getrc_async_cb)(void* ctx, void* rsrc, struct charbuf* err);
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

struct rsrc_ls_ent {
    enum rsrc_type type;
    uint8_t havecrc : 1;
    const char* name;
    const char* ext;
    uint32_t namecrc;
    size_t size;
    uint64_t crc;
};
struct rsrc_ls {
    struct rsrc_ls_ent* ents;
    uint32_t entct;
    char* names;
};
struct rsrc_info {
    uint8_t havecrc : 1;
    const char* ext;
    size_t size;
    uint64_t crc;
};

#define RSRCDRIVE_NODUPNAME  (1U << 0)
#define RSRCDRIVE_FREENAME   (1U << 1)
#define REPLRSRCDRIVE_NODELONFAIL (1U << 8)

#define RSRCOVERLAY_NODUPSRCPATH   (1U << 0)
#define RSRCOVERLAY_FREESRCPATH    (1U << 1)
#define RSRCOVERLAY_NODUPDESTPATH  (1U << 2)
#define RSRCOVERLAY_FREEDESTPATH   (1U << 3)
#define RSRCOVERLAY_UNSAFE         (RSRCOVERLAY_NODUPSRCPATH | RSRCOVERLAY_FREESRCPATH | \
                                   RSRCOVERLAY_NODUPDESTPATH | RSRCOVERLAY_FREEDESTPATH)

#define LSRC_NAMECRC (1U << 0)
#define LSRC_SIZE    (1U << 1)
#define LSRC_DLFSCRC (1U << 2)
#define LSRC_CRC     (1U << 3)

#define GETRCINFO_HAVECRC    (1U << 0)
#define GETRCINFO_GETSIZE    (1U << 1)
#define GETRCINFO_GETDLFSCRC (1U << 2)
#define GETRCINFO_GETCRC     (1U << 3)

#define DELRC_HAVECRC (1U << 0)

#define MAPRC_NODUPPATH  (1U << 0)
#define MAPRC_FREEPATH   (1U << 1)
#define MAPRC_UNSAFE     (MAPRC_NODUPPATH | MAPRC_FREEPATH)

bool initRsrcMgr(void);
void runRsrcMgr(uint64_t t);
//void* rsrcmgr_malloc(size_t);
//void* rsrcmgr_calloc(size_t, size_t);
//void* rsrcmgr_realloc(void*, size_t);
void clrRsrcCache(void);
void quitRsrcMgr(bool quick);

uint32_t newRsrcDrive(unsigned flags, uint32_t key, const char* name, struct rsrc_drive_proto_params*);
bool editRsrcDrive(uint32_t, unsigned flags, const char* name, struct rsrc_drive_proto_params*);
void delRsrcDrive(uint32_t);

bool evalRsrcPath(uint32_t key, const char* path, uint32_t* outdrive, struct charbuf* outpath);

uint32_t newRsrcOverlay(unsigned flags, uint32_t after, uint32_t key, const struct rsrc_overlay_params*);
void delRsrcOverlay(uint32_t);

bool getRsrcSrc(enum rsrc_type type, uint32_t key, uint32_t drive, const char* path, size_t pathlen, struct rsrc_src*);
void freeRsrcSrc(struct rsrc_src*);

bool getRsrcRaw(const struct rsrc_src*, enum rsrc_raw_type typepref, bool forcepref, struct rsrc_raw*);
bool cvtRsrcRaw(struct rsrc_raw*, enum rsrc_raw_type newtype);
void freeRsrcRaw(struct rsrc_raw*);

void* getRsrc(enum rsrc_type type, uint32_t key, uint32_t drive, const char* path, struct getrc_opt* opt, const void* rsrc_opt, struct charbuf* err);
void rlsRsrc(void*);
void lockRsrc(void*);
#define unLockRsrc rlsRsrc

bool lsRsrc(unsigned typemask, unsigned flags, uint32_t key, uint32_t drive, const char* path, struct rsrc_ls*);
void freeRsrcls(struct rsrc_ls*);
bool getRsrcInfo(enum rsrc_type type, unsigned flags, uint32_t key, uint32_t drive, const char* path, uint64_t crc, struct rsrc_info*);

bool delRsrc(enum rsrc_type type, unsigned flags, uint32_t key, uint32_t drive, const char* path, uint64_t crc);
bool copyRsrc(enum rsrc_type type, uint32_t key, uint32_t srcdrive, const char* srcpath, uint32_t destdrive, const char* destpath);

uint32_t mapRsrcFile(uint32_t mapperdrive, unsigned flags, const char* path);
uint32_t mapRsrcDir(uint32_t mapperdrive, unsigned flags, const char* path);
//uint32_t mapRsrcPAF(uint32_t mapperdrive, unsigned flags, struct paf*);
void unMapRsrc(uint32_t mapperdrive, uint32_t id);

// TODO: inteface for reading and writing raw rsrc data

#endif
