#include "resource_new.h"
#include "threading.h"
#include "vlb.h"
#include "crc.h"
#include "util.h"
#include "filesystem.h"

#ifndef PSRC_MODULE_SERVER
    //#include "engine/renderer.h"
#endif

static const enum rsrc_subtype rsrc_subtypect[RSRC__COUNT] = {
    1,
    RSRC_CONFIG__COUNT,
    RSRC_FONT__COUNT,
    RSRC_MAP__COUNT,
    RSRC_MODEL__COUNT,
    RSRC_SCRIPT__COUNT,
    RSRC_SOUND__COUNT,
    RSRC_TEXT__COUNT,
    RSRC_TEXTURE__COUNT,
    RSRC_VIDEO__COUNT
};
static const char** rsrc_exts[RSRC__COUNT] = {
    (const char* [1])                   {NULL},
    (const char* [RSRC_CONFIG__COUNT])  {".cfg"},
    (const char* [RSRC_FONT__COUNT])    {".ttf", ".otf"},
    (const char* [RSRC_MAP__COUNT])     {".pmf"},
    (const char* [RSRC_MODEL__COUNT])   {".p3m"},
    (const char* [RSRC_SCRIPT__COUNT])  {".bas"},
    (const char* [RSRC_SOUND__COUNT])   {".ogg", ".mp3", ".wav"},
    (const char* [RSRC_TEXT__COUNT])    {".txt", ".md"},
    (const char* [RSRC_TEXTURE__COUNT]) {".ptf", ".png", ".jpg", ".tga", ".bmp"},
    (const char* [RSRC_VIDEO__COUNT])   {".mpg"}
};
static const size_t* rsrc_extlens[RSRC__COUNT] = {
    (const size_t [1])                   {0},
    (const size_t [RSRC_CONFIG__COUNT])  {4},
    (const size_t [RSRC_FONT__COUNT])    {4, 4},
    (const size_t [RSRC_MAP__COUNT])     {4},
    (const size_t [RSRC_MODEL__COUNT])   {4},
    (const size_t [RSRC_SCRIPT__COUNT])  {4},
    (const size_t [RSRC_SOUND__COUNT])   {4, 4, 4},
    (const size_t [RSRC_TEXT__COUNT])    {4, 3},
    (const size_t [RSRC_TEXTURE__COUNT]) {4, 4, 4, 4, 4},
    (const size_t [RSRC_VIDEO__COUNT])   {4}
};

static const void* const defaultrcopts[RSRC__COUNT] = {
    NULL,
    NULL,
    NULL,
    NULL,
    &(struct rsrc_opt_model){0},
    &(struct rsrc_opt_script){0},
    NULL,
    NULL,
    &(struct rsrc_opt_texture){0},
    NULL
};

struct rsrc_header {
    enum rsrc_type type;
    enum rsrc_subtype subtype;
    uint32_t drive;
    uint64_t driveid;
    char* path;
    size_t pathlen;
    uint32_t pathcrc;
    uint64_t datacrc;
    size_t refs;
    size_t index;
    unsigned nocache : 1;
    unsigned havecrc : 1;
    unsigned hasdatacrc : 1;
};

struct resource {
    struct rsrc_header header;
};

struct rsrc_page {
    void* data;
    uint16_t occ;
    uint16_t zref;
};
struct rsrc_table {
    struct rsrc_page* pages;
    size_t pagect;
} rsrc_tables[RSRC__COUNT];

PACKEDENUM rsrc_drive_mapperitem_type {
    RSRC_DRIVE_MAPPERITEMTYPE_FILE,
    RSRC_DRIVE_MAPPERITEMTYPE_DIR
    //RSRC_DRIVE_MAPPERITEMTYPE_PAF
};
struct rsrc_drive_mapperitem {
    uint32_t id;
    uint8_t flags : 7;
    uint8_t valid : 1;
    enum rsrc_type rsrctype;
    enum rsrc_subtype rsrcsubtype;
    enum rsrc_drive_mapperitem_type type;
    union {
        struct {
            char* path;
        } file;
        struct {
            char* path;
            size_t pathlen;
        } dir;
        //struct {
        //    struct paf* paf;
        //    const char* path;
        //    size_t pathlen;
        //} paf;
    };
};
struct rsrc_drive_proto {
    enum rsrc_drive_proto_type type;
    union {
        struct {
            uint8_t flags;
            uint32_t drive;
            const char* path;
            size_t pathlen;
        } redir;
        struct {
            uint8_t flags;
            const char* path;
            size_t pathlen;
        } fs;
        #if 0
        struct {
            uint8_t flags;
            struct paf* paf;
            const char* path;
        } paf;
        #endif
        struct {
            struct {
                struct rsrc_drive_mapperitem* data;
                uint32_t len;
                size_t size;
            } items;
            uint32_t curid;
        } mapper;
    };
};

struct rsrc_drive_overlay {
    uint32_t index;
    uint32_t next;
};
struct rsrc_drive {
    uint8_t flags : 7;
    uint8_t valid : 1;
    const char* name;
    uint32_t namecrc;
    uint32_t key;
    uint64_t id;
    struct rsrc_drive_proto proto;
    struct {
        struct rsrc_drive_overlay* data;
        uint32_t len;
        size_t size;
        uint32_t head;
    } overlays;
};

struct rsrc_overlay {
    uint8_t flags : 7;
    uint8_t valid : 1;
    uint32_t key;
    uint32_t srcdrive;
    const char* srcpath;
    size_t srcpathlen;
    uint32_t destdrive;
    const char* destpath;
    size_t destpathlen;
};

struct {
    #if PSRC_MTLVL >= 1
    mutex_t lock;
    #endif
    struct {
        struct rsrc_drive* data;
        uint32_t len;
        size_t size;
        uint64_t curid;
    } drives;
    struct {
        struct rsrc_overlay* data;
        uint32_t len;
        size_t size;
    } overlays;
} rsrcmgr;

static uint32_t findRsrcDrive(uint32_t key, const char* name) {
    if (name && *name) {
        uint32_t namecrc = strcrc32(name);
        for (uint32_t i = 0; i < rsrcmgr.drives.len; ++i) {
            struct rsrc_drive* d = &rsrcmgr.drives.data[i];
            if (d->valid && d->key == key && d->name && d->namecrc == namecrc && !strcmp(d->name, name)) return i;
        }
    } else {
        for (uint32_t i = 0; i < rsrcmgr.drives.len; ++i) {
            struct rsrc_drive* d = &rsrcmgr.drives.data[i];
            if (d->valid && d->key == key && !d->name) return i;
        }
    }
    return -1;
}
#if 0
static uint32_t findRsrcDrive_givencrc(uint32_t key, const char* name, uint32_t namecrc) {
    if (name && *name) {
        for (uint32_t i = 0; i < rsrcmgr.drives.len; ++i) {
            struct rsrc_drive* d = &rsrcmgr.drives.data[i];
            if (d->valid && d->key == key && d->name && d->namecrc == namecrc && !strcmp(d->name, name)) return i;
        }
    } else {
        for (uint32_t i = 0; i < rsrcmgr.drives.len; ++i) {
            struct rsrc_drive* d = &rsrcmgr.drives.data[i];
            if (d->valid && d->key == key && !d->name) return i;
        }
    }
    return -1;
}
#endif
static inline uint32_t findDefaultRsrcDrive(uint32_t key) {
    for (uint32_t i = 0; i < rsrcmgr.drives.len; ++i) {
        struct rsrc_drive* d = &rsrcmgr.drives.data[i];
        if (d->valid && d->key == key && !d->name) return i;
    }
    return -1;
}

// 'isdir' output is true if the path is a dir, false if the path can be interpreted as either a file or dir
static bool canonRsrcPath(const char* in, size_t inlen, bool* isdir, struct charbuf* out) {
    size_t baselen = out->len;
    register size_t pos = 0;
    while (1) {
        while (1) {
            if (pos == inlen) {
                if (isdir) *isdir = true;
                goto ret;
            }
            if (in[pos] != '/') break;
            slash:;
            ++pos;
        }
        if (in[pos] == '.') {
            ++pos;
            if (pos == inlen) {
                if (isdir) *isdir = true;
                goto ret;
            }
            if (in[pos] == '.') {
                ++pos;
                if (pos == inlen || in[pos] == '/') {
                    while (out->len > baselen) {
                        --out->len;
                        if (out->data[out->len] == '/') break;
                    }
                    if (pos == inlen) {
                        if (isdir) *isdir = true;
                        goto ret;
                    }
                    goto slash;
                }
                //if (!cb_add(out, '/') || !cb_add(out, '.') || !cb_add(out, '.')) goto retfalse;
                //if (!cb_addpartstr(out, "/..", 3)) goto retfalse;
                {
                    register size_t ol = out->len;
                    if (!cb_addmultifake(out, 3)) goto retfalse;
                    out->data[ol++] = '/';
                    out->data[ol++] = '.';
                    out->data[ol] = '.';
                }
            } else if (in[pos] == '/') {
                goto slash;
            } else {
                if (!cb_add(out, '/') || !cb_add(out, '.')) goto retfalse;
            }
        } else if (!cb_add(out, '/')) {
            goto retfalse;
        }
        while (1) {
            if (!cb_add(out, in[pos])) goto retfalse;
            ++pos;
            if (pos == inlen) {
                if (isdir) *isdir = false;
                goto ret;
            }
            if (in[pos] == '/') goto slash;
        }
    }

    ret:;
    //if (out->len == baselen && !cb_add(out, '/')) goto retfalse;
    return true;

    retfalse:;
    out->len = baselen;
    return false;
}

static bool evalRsrcPath_internal(uint32_t key, const char* path, size_t pathlen, bool* isdir, uint32_t* outdrive, struct charbuf* outpath) {
    size_t off;
    if (pathlen && path[0] == ':') {
        uint32_t di = findDefaultRsrcDrive(key);
        if (di == -1U) return false;
        off = 1;
        *outdrive = di;
    } else {
        size_t oldoutlen = outpath->len;
        register size_t pos = 0;
        while (1) {
            if (pos == pathlen) {
                uint32_t di = findDefaultRsrcDrive(key);
                if (di == -1U) return false;
                off = 0;
                *outdrive = di;
                outpath->len = oldoutlen;
                break;
            }
            char c = path[pos++];
            if (c == ':') {
                uint32_t di;
                if (!cb_nullterm(outpath) || (di = findRsrcDrive(key, outpath->data + oldoutlen)) == -1U) {
                    outpath->len = oldoutlen;
                    return false;
                }
                off = pos;
                *outdrive = di;
                outpath->len = oldoutlen;
                break;
            } else if (!c) {
                outpath->len = oldoutlen;
                return false;
            }
            cb_add(outpath, c);
        }
    }
    if (!canonRsrcPath(path + off, pathlen - off, isdir, outpath)) return false;
    return true;
}
bool evalRsrcPath(uint32_t key, const char* path, size_t pathlen, bool* isdir, uint32_t* outdrive, struct charbuf* outpath) {
    #if PSRC_MTLVL >= 1
    lockMutex(&rsrcmgr.lock);
    #endif
    if (pathlen == (size_t)-1) pathlen = strlen(path);
    bool ret = evalRsrcPath_internal(key, path, pathlen, isdir, outdrive, outpath);
    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
    return ret;
}

static inline void delRsrcOverlay_internal(struct rsrc_overlay* o) {
    o->valid = 0;
    if (o->flags & RSRCOVERLAY_FREESRCPATH) free((char*)o->srcpath);
    if (o->flags & RSRCOVERLAY_FREEDESTPATH) free((char*)o->destpath);
}

static inline void unlinkRsrcDriveOverlays(struct rsrc_drive* d, uint32_t oi) {
    uint32_t i = d->overlays.head;
    uint32_t prev = -1;
    while (/*i != -1*/ 1) {
        struct rsrc_drive_overlay* o = &d->overlays.data[i];
        if (o->index == oi) {
            if (prev != -1U) d->overlays.data[prev].next = o->next;
            else d->overlays.head = o->next;
            o->index = -1;
            break;
        }
        prev = i;
        i = o->next;
    }
}

static bool initRsrcDrive(struct rsrc_drive* d, struct rsrc_drive_proto_opt* opt) {
    switch (opt->type) {
        DEFAULTCASE(RSRC_DRIVE_PROTO_NULL): break;
        case RSRC_DRIVE_PROTO_REDIR: {
            d->proto.redir.flags = opt->redir.flags;
            d->proto.redir.drive = opt->redir.drive;
            if (opt->redir.pathlen == (size_t)-1) d->proto.redir.pathlen = strlen(opt->redir.path);
            else d->proto.redir.pathlen = opt->redir.pathlen;
            if (!(d->proto.redir.flags & RSRC_DRIVE_PROTO_REDIR_NODUPPATH)) {
                char* tmp = malloc(d->proto.redir.pathlen);
                if (!tmp) {
                    if (d->proto.redir.flags & RSRC_DRIVE_PROTO_REDIR_FREEPATH) free((char*)opt->redir.path);
                    return false;
                }
                memcpy(tmp, opt->redir.path, d->proto.redir.pathlen);
                if (d->proto.redir.flags & RSRC_DRIVE_PROTO_REDIR_FREEPATH) free((char*)opt->redir.path);
                d->proto.redir.path = tmp;
                d->proto.redir.flags |= RSRC_DRIVE_PROTO_REDIR_FREEPATH;
            } else {
                d->proto.redir.path = opt->redir.path;
            }
        } break;
        case RSRC_DRIVE_PROTO_FS: {
            d->proto.fs.flags = opt->fs.flags;
            if (opt->fs.pathlen == (size_t)-1) d->proto.fs.pathlen = strlen(opt->fs.path);
            else d->proto.fs.pathlen = opt->fs.pathlen;
            if (!(d->proto.fs.flags & RSRC_DRIVE_PROTO_FS_NODUPPATH)) {
                char* tmp = malloc(d->proto.fs.pathlen);
                if (!tmp) {
                    if (d->proto.fs.flags & RSRC_DRIVE_PROTO_FS_FREEPATH) free((char*)opt->fs.path);
                    return false;
                }
                memcpy(tmp, opt->fs.path, d->proto.fs.pathlen);
                if (d->proto.fs.flags & RSRC_DRIVE_PROTO_FS_FREEPATH) free((char*)opt->fs.path);
                d->proto.fs.path = tmp;
                d->proto.fs.flags |= RSRC_DRIVE_PROTO_FS_FREEPATH;
            } else {
                d->proto.fs.path = opt->fs.path;
            }
        } break;
        case RSRC_DRIVE_PROTO_MAPPER: {
            VLB_ZINIT(d->proto.mapper.items);
        } break;
    }
    d->proto.type = opt->type;
    VLB_ZINIT(d->overlays);
    d->overlays.head = -1;
    return true;
}
static void freeRsrcDrive(struct rsrc_drive* d, uint32_t di) {
    switch (d->proto.type) {
        DEFAULTCASE(RSRC_DRIVE_PROTO_NULL): break;
        case RSRC_DRIVE_PROTO_REDIR:
            if (d->proto.redir.flags & RSRC_DRIVE_PROTO_REDIR_FREEPATH) free((char*)d->proto.redir.path);
            break;
        case RSRC_DRIVE_PROTO_FS:
            if (d->proto.fs.flags & RSRC_DRIVE_PROTO_FS_FREEPATH) free((char*)d->proto.fs.path);
            break;
        case RSRC_DRIVE_PROTO_MAPPER:
            for (size_t i = 0; i < d->proto.mapper.items.len; ++i) {
                struct rsrc_drive_mapperitem* item = &d->proto.mapper.items.data[i];
                if (!item->valid) continue;
                switch (item->type) {
                    case RSRC_DRIVE_MAPPERITEMTYPE_FILE:
                        if (item->flags & MAPRSRC_FREEPATH) free(item->file.path);
                        break;
                    case RSRC_DRIVE_MAPPERITEMTYPE_DIR:
                        if (item->flags & MAPRSRC_FREEPATH) free(item->dir.path);
                        break;
                }
            }
            VLB_FREE(d->proto.mapper.items);
            break;
    }
    for (uint32_t i = 0; i < rsrcmgr.overlays.len; ++i) {
        struct rsrc_overlay* o = &rsrcmgr.overlays.data[i];
        if (o->valid && o->destdrive == di) {
            unlinkRsrcDriveOverlays(&rsrcmgr.drives.data[o->srcdrive], i);
            delRsrcOverlay_internal(o);
        }
    }
    {
        uint32_t i = d->overlays.head;
        while (i != -1U) {
            struct rsrc_drive_overlay* o = &d->overlays.data[i];
            delRsrcOverlay_internal(&rsrcmgr.overlays.data[o->index]);
            i = o->next;
        }
    }
    VLB_FREE(d->overlays);
}

uint32_t newRsrcDrive(unsigned flags, uint32_t key, const char* name, struct rsrc_drive_proto_opt* opt) {
    #if PSRC_MTLVL >= 1
    lockMutex(&rsrcmgr.lock);
    #endif
    if (rsrcmgr.drives.len == -1U) goto retbad;
    uint32_t namecrc;
    if (name) {
        if (*name) {
            if (!(flags & RSRCDRIVE_NODUPNAME)) {
                char* tmp = strdup(name);
                if (!tmp) {
                    if (flags & RSRCDRIVE_FREENAME) free((char*)name);
                    goto retbad;
                }
                if (flags & RSRCDRIVE_FREENAME) free((char*)name);
                name = tmp;
                flags |= RSRCDRIVE_FREENAME;
            }
            namecrc = strcrc32(name);
        } else {
            if (flags & RSRCDRIVE_FREENAME) free((char*)name);
            name = NULL;
            namecrc = 0;
        }
    } else {
        namecrc = 0;
    }
    struct rsrc_drive* d;
    uint32_t di;
    {
        uint32_t i = 0;
        while (1) {
            if (i == rsrcmgr.drives.len) {
                VLB_NEXTPTR(rsrcmgr.drives, d, 2, 1, goto retbad;);
                if (!initRsrcDrive(d, opt)) {
                    --rsrcmgr.drives.len;
                    goto retbad;
                }
                di = i;
                break;
            }
            struct rsrc_drive* tmpd = &rsrcmgr.drives.data[i];
            if (!tmpd->valid) {
                if (!initRsrcDrive(tmpd, opt)) goto retbad;
                d = tmpd;
                di = i;
                break;
            }
            ++i;
        }
    }
    d->valid = 1;
    d->flags = flags;
    d->name = name;
    d->namecrc = namecrc;
    d->key = key;
    d->id = rsrcmgr.drives.curid++;
    ret:;
    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
    return di;

    retbad:;
    di = -1;
    if (flags & RSRCDRIVE_FREENAME) free((char*)name);
    goto ret;
}
bool editRsrcDrive(uint32_t di, unsigned flags, const char* name, struct rsrc_drive_proto_opt* opt) {
    #if PSRC_MTLVL >= 1
    lockMutex(&rsrcmgr.lock);
    #endif
    struct rsrc_drive* d = &rsrcmgr.drives.data[di];
    if ((int)opt->type >= 0) {
        if (!(flags & REPLRSRCDRIVE_NODELONFAIL)) {
            freeRsrcDrive(d, di);
            if (!initRsrcDrive(d, opt)) {
                d->valid = 0;
                goto retfalse;
            }
        } else {
            struct rsrc_drive tmpd;
            if (!initRsrcDrive(&tmpd, opt)) goto retfalse;
            freeRsrcDrive(d, di);
            *d = tmpd;
        }
        d->valid = 1;
        d->flags &= (RSRCDRIVE_FREENAME | RSRCDRIVE_NODUPNAME);
        d->flags |= flags & ~(RSRCDRIVE_FREENAME | RSRCDRIVE_NODUPNAME);
        d->id = rsrcmgr.drives.curid++;
    }
    if (name) {
        if (*name) {
            if (!(flags & RSRCDRIVE_NODUPNAME)) {
                char* tmp = strdup(name);
                if (!tmp) {
                    if (flags & RSRCDRIVE_FREENAME) free((char*)name);
                    goto retfalse;
                }
                if (flags & RSRCDRIVE_FREENAME) free((char*)name);
                name = tmp;
                flags |= RSRCDRIVE_FREENAME;
            }
            d->namecrc = strcrc32(name);
        } else {
            if (flags & RSRCDRIVE_FREENAME) free((char*)name);
            name = NULL;
            d->namecrc = 0;
        }
        if (d->flags & RSRCDRIVE_FREENAME) free((char*)d->name);
        d->name = name;
        d->flags &= ~(RSRCDRIVE_FREENAME | RSRCDRIVE_NODUPNAME);
        d->flags |= flags & (RSRCDRIVE_FREENAME | RSRCDRIVE_NODUPNAME);
    }

    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
    return true;

    retfalse:;
    if (flags & RSRCDRIVE_FREENAME) free((char*)name);
    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
    return false;
}
void delRsrcDrive(uint32_t di) {
    #if PSRC_MTLVL >= 1
    lockMutex(&rsrcmgr.lock);
    #endif
    struct rsrc_drive* d = &rsrcmgr.drives.data[di];
    freeRsrcDrive(d, di);
    if (d->flags & RSRCDRIVE_FREENAME) free((char*)d->name);
    d->valid = 0;
    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
}

uint32_t newRsrcOverlay(unsigned flags, uint32_t after, uint32_t key, const struct rsrc_overlay_opt* opt) {
    #if PSRC_MTLVL >= 1
    lockMutex(&rsrcmgr.lock);
    #endif

    uint32_t srcdrive, destdrive;
    const char* srcpath = opt->srcpath;
    const char* destpath = opt->destpath;
    size_t srcpathlen = (opt->srcpathlen != (size_t)-1) ? opt->srcpathlen : strlen(srcpath);
    size_t destpathlen = (opt->destpathlen != (size_t)-1) ? opt->destpathlen : strlen(destpath);
    if (rsrcmgr.overlays.len == -1U) goto retbad_freepaths;
    if (opt->srcdrive == -1U) {
        struct charbuf cb;
        if (!cb_init(&cb, 128)) goto retbad_freepaths;
        char* tmp;
        if (!evalRsrcPath_internal(opt->srcdrivekey, srcpath, srcpathlen, NULL, &srcdrive, &cb)) {
            cb_dump(&cb);
            goto retbad_freepaths;
        }
        if (cb.len) {
            if (!(tmp = cb_finalize(&cb))) {
                cb_dump(&cb);
                goto retbad_freepaths;
            }
            if (flags & RSRCOVERLAY_FREESRCPATH) free((char*)srcpath);
            flags |= RSRCOVERLAY_FREESRCPATH;
            srcpath = tmp;
            srcpathlen = cb.len;
        } else {
            cb_dump(&cb);
            srcpath = NULL;
            srcpathlen = 0;
        }
    } else {
        srcdrive = opt->srcdrive;
        if (!(flags & RSRCOVERLAY_NODUPSRCPATH)) {
            char* tmp = malloc(srcpathlen);
            if (!tmp) {
                if (flags & RSRCOVERLAY_FREESRCPATH) free((char*)srcpath);
                goto retbad_freepaths;
            }
            memcpy(tmp, srcpath, srcpathlen);
            if (flags & RSRCOVERLAY_FREESRCPATH) free((char*)srcpath);
            srcpath = tmp;
            flags |= RSRCOVERLAY_FREESRCPATH;
        }
    }
    if (opt->destdrive == -1U) {
        struct charbuf cb;
        if (!cb_init(&cb, 128)) goto retbad_freepaths;
        char* tmp;
        if (!evalRsrcPath_internal(opt->destdrivekey, destpath, destpathlen, NULL, &destdrive, &cb)) {
            cb_dump(&cb);
            goto retbad_freepaths;
        }
        if (cb.len) {
            if (!(tmp = cb_finalize(&cb))) {
                cb_dump(&cb);
                goto retbad_freepaths;
            }
            if (flags & RSRCOVERLAY_FREEDESTPATH) free((char*)destpath);
            flags |= RSRCOVERLAY_FREEDESTPATH;
            destpath = tmp;
            destpathlen = cb.len;
        } else {
            cb_dump(&cb);
            destpath = NULL;
            destpathlen = 0;
        }
    } else {
        destdrive = opt->destdrive;
        if (!(flags & RSRCOVERLAY_NODUPDESTPATH)) {
            char* tmp = malloc(destpathlen);
            if (!tmp) {
                if (flags & RSRCOVERLAY_FREEDESTPATH) free((char*)destpath);
                goto retbad_freepaths;
            }
            memcpy(tmp, destpath, destpathlen);
            if (flags & RSRCOVERLAY_FREEDESTPATH) free((char*)destpath);
            destpath = tmp;
            flags |= RSRCOVERLAY_FREEDESTPATH;
        }
    }

    uint32_t oi = 0;
    struct rsrc_overlay* o;
    while (1) {
        if (oi == rsrcmgr.overlays.len) {
            VLB_NEXTPTR(rsrcmgr.overlays, o, 2, 1, goto retbad_freepaths;);
            break;
        }
        o = &rsrcmgr.overlays.data[oi];
        if (!o->valid) break;
        ++oi;
    }
    *o = (struct rsrc_overlay){
        .flags = flags,
        .key = key,
        .srcdrive = srcdrive,
        .srcpath = srcpath,
        .srcpathlen = srcpathlen,
        .destdrive = destdrive,
        .destpath = destpath,
        .destpathlen = destpathlen
    };
    struct rsrc_drive* d = &rsrcmgr.drives.data[srcdrive];
    uint32_t drvoi = 0;
    struct rsrc_drive_overlay* drvo;
    while (1) {
        if (drvoi == d->overlays.len) {
            VLB_NEXTPTR(d->overlays, drvo, 2, 1, goto retbad_freeoverlay;);
            break;
        }
        drvo = &d->overlays.data[drvoi];
        if (drvo->index == -1U) break;
        ++drvoi;
    }
    drvo->index = oi;
    if (after == -1U) {
        drvo->next = d->overlays.head;
        d->overlays.head = drvoi;
    } else {
        if (after >= rsrcmgr.overlays.len) goto retbad_freeoverlay;
        struct rsrc_overlay* o2 = &rsrcmgr.overlays.data[after];
        if (!o2->valid) goto retbad_freeoverlay;
        struct rsrc_drive* d2 = &rsrcmgr.drives.data[o2->srcdrive];
        uint32_t i = d2->overlays.head;
        while (/*i != -1*/ 1) {
            struct rsrc_drive_overlay* drvo2 = &d2->overlays.data[i];
            if (drvo2->index == after) {
                drvo->next = drvo2->next;
                drvo2->next = drvoi;
                break;
            }
            i = drvo2->next;
        }
    }
    o->valid = 1;

    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
    return oi;

    retbad:;
    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
    return -1;

    retbad_freepaths:;
    if (flags & RSRCOVERLAY_FREESRCPATH) free((char*)srcpath);
    if (flags & RSRCOVERLAY_FREEDESTPATH) free((char*)destpath);
    goto retbad;

    retbad_freeoverlay:;
    delRsrcOverlay_internal(o);
    goto retbad;
}
void delRsrcOverlay(uint32_t oi) {
    #if PSRC_MTLVL >= 1
    lockMutex(&rsrcmgr.lock);
    #endif
    if (oi >= rsrcmgr.overlays.len) goto ret;
    struct rsrc_overlay* o = &rsrcmgr.overlays.data[oi];
    if (!o->valid) goto ret;
    unlinkRsrcDriveOverlays(&rsrcmgr.drives.data[o->srcdrive], oi);
    delRsrcOverlay_internal(o);
    ret:;
    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
}

struct fro_state_elem {
    uint32_t key;
    struct rsrc_drive* d;
    struct charbuf path;
    uint32_t oi;
};
struct fro_state {
    struct fro_state_elem cur;
    struct VLB(struct fro_state_elem) prev;
    struct charbuf* inpath;
    struct charbuf tmpcb;
    size_t redirct;
    uint8_t flags;
};
#define FRO_ISDIR    (1U << 0)
#define FRO_ADOPTKEY (1U << 1)
static bool followRsrcOverlay_start(struct fro_state* fro, uint32_t key, struct rsrc_drive* d, struct charbuf* pathcb, uint8_t flags) {
    //printf("FRO BEGIN: {%s} {%s}\n", d->name, cb_peek(pathcb));
    //if (!cb_init(&fro->cur.path, 128)) return false;
    fro->cur.key = key;
    fro->cur.d = d;
    fro->cur.path.data = NULL;
    fro->cur.oi = d->overlays.head;
    VLB_ZINIT(fro->prev);
    fro->inpath = pathcb;
    fro->tmpcb.data = NULL;
    fro->redirct = 0;
    fro->flags = flags;
    return true;
}
static inline void followRsrcOverlay_end(struct fro_state* fro) {
    cb_dump(&fro->cur.path);
    for (size_t i = 0; i < fro->prev.len; ++i) {
        cb_dump(&fro->prev.data[i].path);
    }
    VLB_FREE(fro->prev);
    cb_dump(&fro->tmpcb);
}
static int followRsrcOverlay_next(struct fro_state* fro, struct rsrc_drive** d, struct charbuf** path) {
    struct rsrc_drive* nd;
    next:;
    if (fro->cur.oi != -1U) {
        next_nocheck:;
        struct rsrc_drive_overlay* drvo = &fro->cur.d->overlays.data[fro->cur.oi];
        fro->cur.oi = drvo->next;
        struct rsrc_overlay* o = &rsrcmgr.overlays.data[drvo->index];
        if (o->key != fro->cur.key) goto next;
        struct charbuf* inpath = (fro->prev.len) ? &fro->prev.data[fro->prev.len - 1].path : fro->inpath;
        if (o->srcpathlen) {
            if (inpath->len < o->srcpathlen) goto next;
            if (!(fro->flags & FRO_ISDIR)) {
                if (inpath->len == o->srcpathlen || inpath->data[o->srcpathlen] != '/') goto next;
            } else {
                if (inpath->len > o->srcpathlen && inpath->data[o->srcpathlen] != '/') goto next;
            }
            if (strncmp(inpath->data, o->srcpath, o->srcpathlen)) goto next;
        }
        if (!fro->cur.path.data) {
            if (!cb_init(&fro->cur.path, 128)) return -1;
        } else {
            fro->cur.path.len = 0;
        }
        if (!cb_addpartstr(&fro->cur.path, o->destpath, o->destpathlen)) return -1;
        if (!cb_addpartstr(&fro->cur.path, inpath->data + o->srcpathlen, inpath->len - o->srcpathlen)) return -1;
        nd = &rsrcmgr.drives.data[o->destdrive];
        if (nd->overlays.head == -1U && nd->proto.type != RSRC_DRIVE_PROTO_REDIR) {
            *d = nd;
            *path = &fro->cur.path;
            return 1;
        }
        if (fro->prev.len + fro->redirct == 31) return -1;
        struct fro_state_elem* e;
        VLB_NEXTPTR(fro->prev, e, 2, 1, return -1;);
        *e = fro->cur;
        if (fro->flags & FRO_ADOPTKEY) fro->cur.key = nd->key;
        fro->cur.d = nd;
        fro->cur.path.data = NULL;
        fro->cur.oi = nd->overlays.head;
        if (nd->proto.type != RSRC_DRIVE_PROTO_REDIR) goto next_nocheck;
        if (nd->proto.redir.pathlen) goto isredir_addpath_prevpath;
        goto next;
    } else if (fro->cur.d->proto.type == RSRC_DRIVE_PROTO_REDIR) {
        if (fro->prev.len + fro->redirct++ == 31) return -1;
        nd = &rsrcmgr.drives.data[fro->cur.d->proto.redir.drive];
        //fro->cur.path.len = 0;
        if (nd->proto.redir.pathlen) {
            struct charbuf* inpath;
            if (fro->prev.len) {
                isredir_addpath_prevpath:;
                inpath = &fro->prev.data[fro->prev.len - 1].path;
            } else {
                if (!fro->inpath->size) {
                    if (fro->tmpcb.data) {
                        fro->tmpcb.len = 0;
                    } else {
                        if (!cb_init(&fro->tmpcb, 128)) return -1;
                    }
                    if (!cb_addpartstr(&fro->tmpcb, fro->cur.d->proto.redir.path, fro->cur.d->proto.redir.pathlen)) return -1;
                    if (!cb_addpartstr(&fro->tmpcb, fro->inpath->data, fro->inpath->len)) return -1;
                    fro->inpath = &fro->tmpcb;
                    goto setnd;
                }
                inpath = fro->inpath;
            }
            size_t inpathlen = inpath->len;
            if (!cb_addmultifake(inpath, fro->cur.d->proto.redir.pathlen)) return -1;
            memmove(inpath->data + fro->cur.d->proto.redir.pathlen, inpath->data, inpathlen);
            memcpy(inpath->data, fro->cur.d->proto.redir.path, fro->cur.d->proto.redir.pathlen);
        }
        setnd:;
        fro->cur.d = nd;
        fro->cur.oi = nd->overlays.head;
        goto next;
    }
    *d = fro->cur.d;
    if (fro->prev.len) {
        cb_dump(&fro->cur.path);
        fro->cur = fro->prev.data[--fro->prev.len];
        *path = &fro->cur.path;
        return 1;
    }
    *path = fro->inpath;
    return 0;
}

static int getRsrcSrc_try_fs(enum rsrc_type rt, const char* dir, size_t dirlen, const char* path, size_t pathlen, struct charbuf* tmpcb, struct rsrc_src* src) {
    if (tmpcb->data) {
        tmpcb->len = 0;
    } else {
        if (!cb_init(tmpcb, 128)) return -1;
    }
    if (!cb_addpartstr(tmpcb, dir, dirlen)) return -1;
    cb_add(tmpcb, PATHSEP);

    {
        #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        WIN32_FIND_DATA find;
        HANDLE h;
        size_t outbase = tmpcb->len;
        #endif
        for (size_t i = 1; i < pathlen; ++i) {
            char c = path[i];
            if (!c) return 0;
            #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
            else if (c == '/') {
                c = '\\';
                if (tmpcb->len) {
                    if (!cb_nullterm(tmpcb)) return -1;
                    h = FindFirstFile(tmpcb->data, &find);
                    if (h == INVALID_HANDLE_VALUE) return 0;
                    FindClose(h);
                    //printf("STRCMP 1: {%s} {%s}\n", tmpcb->data + outbase, find.cFileName);
                    if (strcmp(tmpcb->data + outbase, find.cFileName)) return 0;
                    outbase = tmpcb->len + 1;
                }
            }
            else if (c == '\\') return 0;
            else if (c == '<' || c == '>' || c == ':' || c == '"' || c == '|' || c == '?' || c == '*') return 0;
            else if (c < 32) return 0;
            #endif
            if (!cb_add(tmpcb, c)) return -1;
        }

        size_t oldlen = tmpcb->len;
        enum rsrc_subtype i = 0;
        while (1) {
            cb_addpartstr(tmpcb, rsrc_exts[rt][i], rsrc_extlens[rt][i]);
            if (!cb_nullterm(tmpcb)) return -1;
            //printf("TRY FS: {%s} [%d]\n", tmpcb->data, isFile(tmpcb->data));
            if (isFile(tmpcb->data) == 1) {
                #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
                    src->rsrcsubtype = i;
                    break;
                #else
                    h = FindFirstFile(tmpcb->data, &find);
                    if (h != INVALID_HANDLE_VALUE) {
                        FindClose(h);
                        //printf("STRCMP 2: {%s} {%s}\n", tmpcb->data + outbase, find.cFileName);
                        if (!strcmp(tmpcb->data + outbase, find.cFileName)) {
                            src->rsrcsubtype = i;
                            break;
                        }
                    }
                #endif
            }
            tmpcb->len = oldlen;
            ++i;
            if (i == rsrc_subtypect[rt]) return 0;
        }
    }

    if (!cb_finalize(tmpcb)) return -1;
    src->type = RSRC_SRC_FS;
    src->fs.path = tmpcb->data;
    src->fs.freepath = true;
    tmpcb->data = NULL;
    return 1;
}
static ALWAYSINLINE int getRsrcSrc_try_proto_fs(enum rsrc_type rt, struct rsrc_drive* d, struct charbuf* pathcb, struct charbuf* tmpcb, struct rsrc_src* src) {
    return getRsrcSrc_try_fs(rt, d->proto.fs.path, d->proto.fs.pathlen, pathcb->data, pathcb->len, tmpcb, src);
}
static int getRsrcSrc_try_proto_mapper(enum rsrc_type rt, struct rsrc_drive* d, struct charbuf* pathcb, struct charbuf* tmpcb, bool dup, struct rsrc_src* src) {
    char c = pathcb->data[1];
    if (c < '0' || c > '9') return 0;
    uint32_t id = c - '0';
    size_t pathi;
    for (pathi = 2; pathi < pathcb->len; ++pathi) {
        char c = pathcb->data[pathi];
        if (c >= '0' || c <= '9') {
            uint32_t newid = id * 10 + (c - '0');
            if (newid < id) return 0;
            id = newid;
        } else if (c == '/') {
            break;
        } else {
            return 0;
        }
    }
    struct rsrc_drive_mapperitem* item;
    {
        size_t i = 0;
        while (1) {
            if (i == d->proto.mapper.items.len) return 0;
            item = &d->proto.mapper.items.data[i];
            if (item->valid && item->id == id) {
                if (item->rsrctype != rt) return 0;
                if (item->type == RSRC_DRIVE_MAPPERITEMTYPE_FILE) {
                    if (pathi != pathcb->len) return 0;
                    src->type = RSRC_SRC_FS;
                    src->rsrcsubtype = item->rsrcsubtype;
                    if (dup || (item->flags & MAPRSRC_UNTERMEDPATH)) {
                        src->fs.path = strdup(item->file.path);
                        if (!src->fs.path) return -1;
                        src->fs.freepath = true;
                    } else {
                        src->fs.path = item->file.path;
                        src->fs.freepath = false;
                    }
                    return 1;
                } else if (pathi == pathcb->len) {
                    return 0;
                }
                break;
            }
            ++i;
        }
    }
    switch (item->type) {
        DEFAULTCASE(RSRC_DRIVE_MAPPERITEMTYPE_DIR): {
            return getRsrcSrc_try_fs(rt, item->dir.path, item->dir.pathlen, pathcb->data + pathi, pathcb->len - pathi, tmpcb, src);
        }
        //case RSRC_DRIVE_MAPPERITEMTYPE_PAF: {
        //} return 1;
    }
}
static int getRsrcSrc_internal(enum rsrc_type rt, uint32_t key, struct rsrc_drive* d, struct charbuf* pathcb, bool dup, struct rsrc_src* src) {
    int retval;
    struct charbuf tmpcb;
    tmpcb.data = NULL;
    struct fro_state fro;
    int froret;
    again:;
    //printf("DRIVE: {%s}\n", d->name);
    if (d->overlays.head == -1U) {
        if (d->proto.type == RSRC_DRIVE_PROTO_REDIR) {
            if (d->proto.redir.pathlen) goto usefro;
            d = &rsrcmgr.drives.data[d->proto.redir.drive];
            goto again;
        }
        froret = -1;
    } else {
        usefro:;
        if (!followRsrcOverlay_start(&fro, key, d, pathcb, 0)) {
            retval = -1;
            goto ret;
        }
        next:;
        froret = followRsrcOverlay_next(&fro, &d, &pathcb);
        if (froret == -1) {
            retval = -1;
            goto ret_endfro;
        }
    }

    switch (d->proto.type) {
        DEFAULTCASE(RSRC_DRIVE_PROTO_NULL):
            //puts("PROTO NULL !");
            retval = 0;
            break;
        case RSRC_DRIVE_PROTO_FS:
            //printf("PROTO FS: {%s} {%s}\n", d->name, cb_peek(pathcb));
            retval = getRsrcSrc_try_proto_fs(rt, d, pathcb, &tmpcb, src);
            break;
        case RSRC_DRIVE_PROTO_MAPPER: 
            //puts("PROTO MAPPER !");
            retval = getRsrcSrc_try_proto_mapper(rt, d, pathcb, &tmpcb, dup, src);
            break;
    }

    if (froret != -1) {
        if (froret && !retval) goto next;
        ret_endfro:;
        followRsrcOverlay_end(&fro);
    }

    ret:;
    cb_dump(&tmpcb);
    return retval;
}
bool getRsrcSrc(enum rsrc_type t, uint32_t k, uint32_t d, const char* p, size_t pl, struct rsrc_src* s) {
    #if PSRC_MTLVL >= 1
    lockMutex(&rsrcmgr.lock);
    #endif
    int retval;
    if (pl == (size_t)-1) pl = strlen(p);
    if (d == -1U) {
        //if (t < RSRC__FILE || t >= RSRC__COUNT) { // TODO: move out
        //    retval = -1;
        //    goto ret;
        //}
        struct charbuf cb;
        if (!cb_init(&cb, 128)) {retval = -1; goto ret;}
        bool isdir;
        if (!evalRsrcPath_internal(k, p, pl, &isdir, &d, &cb) || isdir) {
            cb_dump(&cb);
            retval = -1;
            goto ret;
        }
        retval = getRsrcSrc_internal(t, k, &rsrcmgr.drives.data[d], &cb, true, s);
        cb_dump(&cb);
    } else {
        retval = getRsrcSrc_internal(t, k, &rsrcmgr.drives.data[d], &(struct charbuf){.data = (char*)p, .len = pl}, true, s);
    }
    ret:;
    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
    return (retval == 1);
}
void freeRsrcSrc(struct rsrc_src* src) {
    switch (src->type) {
        default: break;
        case RSRC_SRC_FS:
            if (src->fs.freepath) free((char*)src->fs.path);
            break;
    }
}

int getRsrcRaw(const struct rsrc_src* src, unsigned flags, enum rsrc_raw_type typepref, struct rsrc_raw* raw) {
    switch (src->type) {
        case RSRC_SRC_MEM:
            switch (typepref) {
                case RSRC_RAW_MEM: {
                    raw->type = RSRC_RAW_MEM;
                    raw->mem.data = (void*)src->mem.data;
                    raw->mem.size = src->mem.size;
                    raw->mem.free = false;
                } break;
                case RSRC_RAW_DS: {
                    raw->type = RSRC_RAW_DS;
                    ds_openmem(src->mem.data, src->mem.size, NULL, false, NULL, NULL, &raw->ds);
                } break;
                case RSRC_RAW_FILE: {
                    if (!(flags & GETRSRCRAW_FORCE)) return 0;
                    #if defined(__GLIBC__) && ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 22) || __GLIBC__ > 2)
                        FILE* f = fmemopen((void*)src->mem.data, src->mem.size, "rb");
                        if (!f) return -1;
                        raw->type = RSRC_RAW_FILE;
                        raw->file = f;
                    #else
                        return -1;
                    #endif
                } break;
            }
        break;
        case RSRC_SRC_FS:
            switch (typepref) {
                case RSRC_RAW_MEM: {
                    if (!(flags & GETRSRCRAW_FORCE)) return 0;
                    // TODO: maybe replace with open()/fopen() solution?
                    struct datastream ds;
                    if (!ds_openfile(src->fs.path, NULL, false, 0, &ds)) return -1;
                    size_t sz = ds_getsz(&ds);
                    if (sz == (size_t)-1) {
                        ds_close(&ds);
                        return -1;
                    }
                    void* data = malloc(sz);
                    if (!data) {
                        ds_close(&ds);
                        return -1;
                    }
                    raw->mem.size = sz;
                    sz -= ds_read(&ds, sz, data);
                    ds_close(&ds);
                    if (sz) {
                        free(data);
                        return -1;
                    }
                    raw->type = RSRC_RAW_MEM;
                    raw->mem.data = data;
                    raw->mem.free = true;
                } break;
                case RSRC_RAW_DS: {
                    if (!ds_openfile(src->fs.path, NULL, false, 0, &raw->ds)) return -1;
                    raw->type = RSRC_RAW_DS;
                } break;
                case RSRC_RAW_FILE: {
                    FILE* f = fopen(src->fs.path, "rb");
                    if (!f) return -1;
                    raw->type = RSRC_RAW_FILE;
                    raw->file = f;
                } break;
            }
        break;
        //case RSRC_SRC_PAF:
        //break;
        case RSRC_SRC_DS:
            switch (typepref) {
                case RSRC_RAW_MEM: {
                    if (!(flags & GETRSRCRAW_FORCE)) return 0;
                    if (src->ds.base != ds_tell(src->ds.ds) && !ds_seek(src->ds.ds, src->ds.base)) return -1;
                    size_t sz = src->ds.size;
                    void* data = malloc(sz);
                    if (!data) return -1;
                    raw->mem.size = sz;
                    sz -= ds_read(src->ds.ds, sz, data);
                    if (sz) {
                        free(data);
                        return -1;
                    }
                    raw->type = RSRC_RAW_MEM;
                    raw->mem.data = data;
                    raw->mem.free = true;
                } break;
                case RSRC_RAW_DS: {
                    if (src->ds.base != ds_tell(src->ds.ds) && !ds_seek(src->ds.ds, src->ds.base)) return -1;
                    if (!ds_opensect(src->ds.ds, src->ds.size, NULL, false, 0, &raw->ds)) return -1;
                    raw->type = RSRC_RAW_DS;
                } break;
                case RSRC_RAW_FILE: {
                    if (!(flags & GETRSRCRAW_FORCE)) return 0;
                    return -1;
                } break;
            }
        break;
    }
    return 1;
}
void freeRsrcRaw(struct rsrc_raw* raw) {
    switch (raw->type) {
        case RSRC_RAW_MEM:
            if (raw->mem.free) free(raw->mem.data);
            break;
        case RSRC_RAW_DS:
            ds_close(&raw->ds);
            break;
        case RSRC_RAW_FILE:
            fclose(raw->file);
            break;
    }
}

void* getRsrc_internal(enum rsrc_type type, uint32_t key, uint32_t drive, const char* path, size_t pathlen, struct getrsrc_opt* opt, const void* rsrcopt, struct charbuf* err) {
    bool freepath;
    if (pathlen == (size_t)-1) pathlen = strlen(path);
    if (drive == -1U) {
        freepath = true;
        struct charbuf cb;
        if (!cb_init(&cb, 128)) goto retnull;
        bool isdir;
        if (!evalRsrcPath_internal(key, path, pathlen, &isdir, &drive, &cb) || isdir) {
            cb_dump(&cb);
            goto retnull;
        }
    } else {
        freepath = false;
    }

    struct rsrc_table* rtbl = &rsrc_tables[type];
    //struct rsrc_page* rp = 

    retnull:;
    if (freepath) free((char*)path);
    return NULL;
}
void* getRsrc(enum rsrc_type t, uint32_t k, uint32_t d, const char* p, size_t pl, struct getrsrc_opt* o, const void* ro, struct charbuf* e) {
    #if PSRC_MTLVL >= 1
    lockMutex(&rsrcmgr.lock);
    #endif
    void* ptr = getRsrc_internal(t, k, d, p, pl, o, ro, e);
    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
    return ptr;
}

bool initRsrcMgr(void) {
    #if PSRC_MTLVL >= 1
    if (!createMutex(&rsrcmgr.lock)) return false;
    #endif
    VLB_INIT(rsrcmgr.drives, 16, return false;);
    return true;
}
void quitRsrcMgr(bool quick) {
    if (!quick) {
        VLB_FREE(rsrcmgr.drives);
    }
    #if PSRC_MTLVL >= 1
    destroyMutex(&rsrcmgr.lock);
    #endif
}

// -----

#include <stdio.h>
#include <inttypes.h>

#include "common.h"

#define GOFAIL() do {failat = __LINE__; goto fail;} while (0)

void rsrcmgr_test_dumpstate(void);

void rsrcmgr_test(void) {
    unsigned failat;

    if (!initRsrcMgr()) GOFAIL();

    uint32_t di[8];

    if ((di[0] = newRsrcDrive(
        RSRCDRIVE_NODUPNAME, -1, "games",
        &(struct rsrc_drive_proto_opt){
            .type = RSRC_DRIVE_PROTO_FS,
            .fs.flags = RSRC_DRIVE_PROTO_FS_NODUPPATH,
            .fs.path = dirs[DIR_GAMES],
            .fs.pathlen = -1
        }
    )) == -1U) GOFAIL();
    if ((di[1] = newRsrcDrive(
        RSRCDRIVE_NODUPNAME, -1, "mods",
        &(struct rsrc_drive_proto_opt){
            .type = RSRC_DRIVE_PROTO_FS,
            .fs.flags = RSRC_DRIVE_PROTO_FS_NODUPPATH,
            .fs.path = dirs[DIR_MODS],
            .fs.pathlen = -1
        }
    )) == -1U) GOFAIL();
    if ((di[2] = newRsrcDrive(
        RSRCDRIVE_NODUPNAME, -1, "self",
        &(struct rsrc_drive_proto_opt){
            .type = RSRC_DRIVE_PROTO_REDIR,
            .redir.flags = RSRC_DRIVE_PROTO_REDIR_NODUPPATH,
            .redir.drive = di[0],
            .redir.path = "/h74",
            .redir.pathlen = -1
        }
    )) == -1U) GOFAIL();
    if ((di[3] = newRsrcDrive(
        RSRCDRIVE_NODUPNAME, -1, NULL,
        &(struct rsrc_drive_proto_opt){
            .type = RSRC_DRIVE_PROTO_REDIR,
            .redir.flags = RSRC_DRIVE_PROTO_REDIR_NODUPPATH,
            .redir.drive = di[2],
            .redir.path = NULL,
            .redir.pathlen = 0
        }
    )) == -1U) GOFAIL();

    uint32_t oi[8];

    if ((oi[0] = newRsrcOverlay(
        RSRCOVERLAY_NODUPSRCPATH | RSRCOVERLAY_NODUPDESTPATH, -1, -1,
        &(struct rsrc_overlay_opt){
            .srcdrive = -1,
            .srcdrivekey = -1,
            .srcpath = "games:",
            .srcpathlen = -1,
            .destdrive = -1,
            .destdrivekey = -1,
            .destpath = "mods:h74_hqsounds",
            .destpathlen = -1
        }
    )) == -1U) GOFAIL();

    puts("\n--------------------------------\n");
    rsrcmgr_test_dumpstate();

    #if 1
    puts("\n--------------------------------\n");
    {
        struct rsrc_src src;
//        if (getRsrcSrc(
//            RSRC_SCRIPT,
//            -1, -1, ":/Main", -1,
//            &src
//        )) GOFAIL();
        if (!getRsrcSrc(
            RSRC_SOUND,
            -1, -1, ":/sounds/ctf/capture", -1,
            &src
        )) GOFAIL();
        if (src.type != RSRC_SRC_FS) GOFAIL();
        printf("path: {%s}, freepath: [%d]\n", src.fs.path, src.fs.freepath);
        freeRsrcSrc(&src);
    }
    #endif

    puts("\n--------------------------------\n");
    puts("PASS");
    exit(0);
    fail:;
    printf("FAIL [%u]\n", failat);
    exit(1);

    puts("\n--------------------------------\n");
}

void rsrcmgr_test_dumpstate(void) {
    #if PSRC_MTLVL >= 1
    lockMutex(&rsrcmgr.lock);
    #endif
    puts("Drives:");
    for (uint32_t i = 0; i < rsrcmgr.drives.len; ++i) {
        printf("  Drive %u:\n", i);
        struct rsrc_drive* d = &rsrcmgr.drives.data[i];
        if (!d->valid) {
            puts("    (Invalid)");
            continue;
        }
        if (d->name) {
            printf("    Name:  \"%s\" (%08X)\n", d->name, d->namecrc);
        } else {
            puts("    Name:  (Default drive)");
        }
        printf("    Key:   %08X\n", d->key);
        fputs("    Flags: _", stdout);
        {
            uint8_t flags = d->flags;
            unsigned i = 6;
            while (1) {
                putchar('0' + ((flags >> i) & 1));
                if (i == 0) break;
                --i;
            }
            putchar('\n');
        }
        printf("    ID:    %016" PRIX64 "\n", d->id);
        puts("    Protocol:");
        switch (d->proto.type) {
            default:
                puts("      Type: NULL");
                break;
            case RSRC_DRIVE_PROTO_REDIR:
                puts("      Type:  REDIR");
                {
                    printf("      Drive:  %u (", d->proto.redir.drive);
                    const char* tmp = rsrcmgr.drives.data[d->proto.redir.drive].name;
                    if (tmp) {
                        putchar('"');
                        fputs(tmp, stdout);
                        puts("\")");
                    } else {
                        puts("Default drive)");
                    }
                }
                if (d->proto.redir.path) {
                    printf("      Path:  \"%.*s\" (%zu)\n", (int)d->proto.redir.pathlen, d->proto.redir.path, d->proto.redir.pathlen);
                } else {
                    puts("      Path:  (None)");
                }
                break;
            case RSRC_DRIVE_PROTO_FS:
                puts("      Type: FS");
                printf("      Path: \"%.*s\" (%zu)\n", (int)d->proto.fs.pathlen, d->proto.fs.path, d->proto.fs.pathlen);
                break;
            case RSRC_DRIVE_PROTO_MAPPER:
                puts("      Type: MAPPER");
                puts("      Items:");
                for (uint32_t i = 0; i < d->proto.mapper.items.len; ++i) {
                    printf("      Item %u:\n", i);
                    struct rsrc_drive_mapperitem* item = &d->proto.mapper.items.data[i];
                    if (!d->valid) {
                        puts("      (Invalid)");
                        continue;
                    }
                    printf("        ID:    %u:\n", item->id);
                    fputs("        Flags: _", stdout);
                    {
                        uint8_t flags = item->flags;
                        unsigned i = 6;
                        while (1) {
                            putchar('0' + ((flags >> i) & 1));
                            if (i == 0) break;
                            --i;
                        }
                        putchar('\n');
                    }
                    switch (item->type) {
                        case RSRC_DRIVE_MAPPERITEMTYPE_FILE:
                            puts("        Type:  FILE");
                            printf("        Path:  \"%s\"\n", item->file.path);
                            break;
                        case RSRC_DRIVE_MAPPERITEMTYPE_DIR:
                            puts("        Type:  DIR");
                            printf("        Path:  \"%.*s\" (%zu)\n", (int)item->dir.pathlen, item->dir.path, item->dir.pathlen);
                            break;
                    }
                }
                break;
        }
        puts("    Overlays:");
        if (d->overlays.head != -1U) {
            printf("      Head: %u\n", d->overlays.head);
        } else {
            puts("      Head: (None)");
        }
        for (uint32_t i = 0; i < d->overlays.len; ++i) {
            printf("      Overlay %u:\n", i);
            struct rsrc_drive_overlay* drvo = &d->overlays.data[i];
            if (drvo->index == -1U) {
                puts("(Invalid)");
                continue;
            }
            printf("        Index:  %u\n", drvo->index);
            fputs("        Next:   ", stdout);
            if (drvo->next != -1U) {
                printf("%u\n", drvo->next);
            } else {
                puts("(None)");
            }
        }
    }
    puts("Overlays:");
    for (uint32_t i = 0; i < rsrcmgr.overlays.len; ++i) {
        printf("  Overlay %u:\n", i);
        struct rsrc_overlay* o = &rsrcmgr.overlays.data[i];
        if (!o->valid) {
            puts("    (Invalid)");
            continue;
        }
        fputs("    Flags:      _", stdout);
        {
            uint8_t flags = o->flags;
            unsigned i = 6;
            while (1) {
                putchar('0' + ((flags >> i) & 1));
                if (i == 0) break;
                --i;
            }
            putchar('\n');
        }
        printf("    Key:        %08X\n", o->key);
        printf("    Src drive:  %u (", o->srcdrive);
        const char* tmp = rsrcmgr.drives.data[o->srcdrive].name;
        if (tmp) {
            putchar('"');
            fputs(tmp, stdout);
            puts("\")");
        } else {
            puts("Default drive)");
        }
        if (o->srcpath) {
            printf("    Src path:   \"%.*s\" (%zu)\n", (int)o->srcpathlen, o->srcpath, o->srcpathlen);
        } else {
            puts("    Src path:   (None)");
        }
        printf("    Dest drive: %u (", o->destdrive);
        tmp = rsrcmgr.drives.data[o->destdrive].name;
        if (tmp) {
            putchar('"');
            fputs(tmp, stdout);
            puts("\")");
        } else {
            puts("Default drive)");
        }
        if (o->destpath) {
            printf("    Dest path:  \"%.*s\" (%zu)\n", (int)o->destpathlen, o->destpath, o->destpathlen);
        } else {
            puts("    Dest path:  (None)");
        }
    }
    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
}
