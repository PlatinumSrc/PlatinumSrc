#include "resource_new.h"
#include "threading.h"
#include "vlb.h"
#include "crc.h"
#include "util.h"
#include "filesystem.h"

PACKEDENUM rsrc_drive_mapperitem_type {
    RSRC_DRIVE_MAPPERITEMTYPE_FILE,
    RSRC_DRIVE_MAPPERITEMTYPE_DIR
};
struct rsrc_drive_mapperitem {
    uint32_t id;
    uint8_t flags : 7;
    uint8_t valid : 1;
    enum rsrc_drive_mapperitem_type type;
    enum rsrc_type rsrctype;
    enum rsrc_subtype rsrcsubtype;
    const char* path;
};
struct rsrc_drive_proto {
    enum rsrc_drive_proto_type type;
    union {
        struct {
            uint8_t flags;
            const char* path;
        } fs;
        #if 0
        struct {
            uint8_t flags;
            struct paf* paf;
            const char* path;
        } paf;
        #endif
        struct {
            uint8_t flags;
            uint32_t drive;
            const char* path;
        } redir;
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

static bool canonRsrcPath(const char* path, struct charbuf* outpath) {
    size_t baselen = outpath->len;
    while (1) {
        while (*path == '/') {
            slash:;
            ++path;
        }
        if (!*path) goto ret;
        if (*path == '.') {
            ++path;
            if (*path == '.') {
                ++path;
                if (*path == '/' || !*path) {
                    while (outpath->len > baselen) {
                        --outpath->len;
                        if (outpath->data[outpath->len] == '/') break;
                    }
                    if (!*path) goto ret;
                    goto slash;
                }
                //if (!cb_add(outpath, '/') || !cb_add(outpath, '.') || !cb_add(outpath, '.')) goto retfalse;
                //if (!cb_addpartstr(outpath, "/..", 3)) goto retfalse;
                {
                    register size_t ol = outpath->len;
                    if (!cb_addmultifake(outpath, 3)) goto retfalse;
                    outpath->data[ol++] = '/';
                    outpath->data[ol++] = '.';
                    outpath->data[ol] = '.';
                }
            } else if (*path == '/') {
                goto slash;
            } else if (!*path) {
                goto ret;
            } else {
                if (!cb_add(outpath, '/') || !cb_add(outpath, '.')) goto retfalse;
            }
        } else if (!cb_add(outpath, '/')) {
            goto retfalse;
        }
        while (1) {
            if (!cb_add(outpath, *path++)) goto retfalse;
            if (*path == '/') goto slash;
            if (!*path) goto ret;
        }
    }

    ret:;
    //if (outpath->len == baselen && !cb_add(outpath, '/')) goto retfalse;
    return true;

    retfalse:;
    outpath->len = baselen;
    return false;
}

static bool evalRsrcPath_internal(uint32_t key, const char* path, uint32_t* outdrive, struct charbuf* outpath) {
    if (*path == ':') {
        uint32_t di = findDefaultRsrcDrive(key);
        if (di == -1U) return false;
        *outdrive = di;
        ++path;
    } else {
        size_t oldoutlen = outpath->len;
        const char* tmppath = path;
        while (1) {
            char c = *tmppath;
            if (!c) {
                outpath->len = oldoutlen;
                break;
            }
            ++tmppath;
            if (c == ':') {
                path = tmppath;
                uint32_t di;
                if (!cb_nullterm(outpath) || (di = findRsrcDrive(key, outpath->data)) == -1U) {
                    outpath->len = oldoutlen;
                    return false;
                }
                *outdrive = di;
                outpath->len = oldoutlen;
                break;
            }
            cb_add(outpath, c);
        }
    }
    if (!canonRsrcPath(path, outpath)) return false;
    return true;
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
        case RSRC_DRIVE_PROTO_FS: {
            d->proto.fs.flags = opt->fs.flags;
            if (!(d->proto.fs.flags & RSRC_DRIVE_PROTO_FS_NODUPPATH)) {
                char* tmp = strdup(opt->fs.path);
                if (!tmp) {
                    if (d->proto.fs.flags & RSRC_DRIVE_PROTO_FS_FREEPATH) free((char*)opt->fs.path);
                    return false;
                }
                if (d->proto.fs.flags & RSRC_DRIVE_PROTO_FS_FREEPATH) free((char*)opt->fs.path);
                d->proto.fs.path = tmp;
                d->proto.fs.flags |= RSRC_DRIVE_PROTO_FS_FREEPATH;
            } else {
                d->proto.fs.path = opt->fs.path;
            }
        } break;
        case RSRC_DRIVE_PROTO_REDIR: {
            d->proto.redir.flags = opt->redir.flags;
            d->proto.redir.drive = opt->redir.drive;
            if (!(d->proto.redir.flags & RSRC_DRIVE_PROTO_REDIR_NODUPPATH)) {
                char* tmp = strdup(opt->redir.path);
                if (!tmp) {
                    if (d->proto.redir.flags & RSRC_DRIVE_PROTO_REDIR_FREEPATH) free((char*)opt->redir.path);
                    return false;
                }
                if (d->proto.redir.flags & RSRC_DRIVE_PROTO_REDIR_FREEPATH) free((char*)opt->redir.path);
                d->proto.redir.path = tmp;
                d->proto.redir.flags |= RSRC_DRIVE_PROTO_REDIR_FREEPATH;
            } else {
                d->proto.redir.path = opt->redir.path;
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
        DEFAULTCASE(RSRC_DRIVE_PROTO_FS):
            if (d->proto.fs.flags & RSRC_DRIVE_PROTO_FS_FREEPATH) free((char*)d->proto.fs.path);
            break;
        case RSRC_DRIVE_PROTO_REDIR:
            if (d->proto.redir.flags & RSRC_DRIVE_PROTO_REDIR_FREEPATH) free((char*)d->proto.redir.path);
            break;
        case RSRC_DRIVE_PROTO_MAPPER:
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

bool evalRsrcPath(uint32_t key, const char* path, uint32_t* outdrive, struct charbuf* outpath) {
    #if PSRC_MTLVL >= 1
    lockMutex(&rsrcmgr.lock);
    #endif
    bool ret = evalRsrcPath_internal(key, path, outdrive, outpath);
    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
    return ret;
}

uint32_t newRsrcOverlay(unsigned flags, uint32_t after, uint32_t key, const struct rsrc_overlay_opt* opt) {
    #if PSRC_MTLVL >= 1
    lockMutex(&rsrcmgr.lock);
    #endif

    uint32_t srcdrive, destdrive;
    const char* srcpath = opt->srcpath;
    const char* destpath = opt->destpath;
    size_t srcpathlen, destpathlen;
    if (rsrcmgr.overlays.len == -1U) goto retbad_freepaths;
    if (opt->srcdrive == -1U) {
        struct charbuf cb;
        if (!cb_init(&cb, 32)) goto retbad_freepaths;
        char* tmp;
        if (!evalRsrcPath_internal(opt->srcdrivekey, srcpath, &srcdrive, &cb)) {
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
            char* tmp = strdup(srcpath);
            if (!tmp) {
                if (flags & RSRCOVERLAY_FREESRCPATH) free((char*)srcpath);
                goto retbad_freepaths;
            }
            if (flags & RSRCOVERLAY_FREESRCPATH) free((char*)srcpath);
            srcpath = tmp;
            flags |= RSRCOVERLAY_FREESRCPATH;
        }
        if (opt->srcpathlen == (size_t)-1) srcpathlen = strlen(srcpath);
        else srcpathlen = opt->srcpathlen;
    }
    if (opt->destdrive == -1U) {
        struct charbuf cb;
        if (!cb_init(&cb, 32)) goto retbad_freepaths;
        char* tmp;
        if (!evalRsrcPath_internal(opt->destdrivekey, destpath, &destdrive, &cb)) {
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
            char* tmp = strdup(destpath);
            if (!tmp) {
                if (flags & RSRCOVERLAY_FREEDESTPATH) free((char*)destpath);
                goto retbad_freepaths;
            }
            if (flags & RSRCOVERLAY_FREEDESTPATH) free((char*)destpath);
            destpath = tmp;
            flags |= RSRCOVERLAY_FREEDESTPATH;
        }
        if (opt->destpathlen == (size_t)-1) destpathlen = strlen(destpath);
        else destpathlen = opt->destpathlen;
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
    struct charbuf inpath;
    struct rsrc_drive* d;
    struct charbuf path;
    uint32_t oi;
};
struct fro_state {
    struct fro_state_elem cur;
    struct VLB(struct fro_state_elem) prev;
    bool isdir;
};
static bool followRsrcOverlay_start(struct fro_state* fro, uint32_t key, struct rsrc_drive* d, const char* path, size_t pathlen, bool isdir) {
    if (!cb_init(&fro->cur.path, 32)) return false;
    fro->cur.key = key;
    fro->cur.inpath.data = (char*)path;
    fro->cur.inpath.len = pathlen;
    fro->cur.inpath.size = 0;
    fro->cur.d = d;
    fro->cur.oi = d->overlays.head;
    VLB_ZINIT(fro->prev);
    fro->isdir = isdir;
    return true;
}
static void followRsrcOverlay_end(struct fro_state* fro) {
    cb_dump(&fro->cur.path);
    for (size_t i = 0; i < fro->prev.len; ++i) {
        cb_dump(&fro->prev.data[i].path);
    }
    VLB_FREE(fro->prev);
}
static int followRsrcOverlay_next(struct fro_state* fro, struct rsrc_drive** d, struct charbuf** path) {
    next:;
    if (fro->cur.oi != -1U) {
        next_nocheck:;
        struct rsrc_drive_overlay* drvo = &fro->cur.d->overlays.data[fro->cur.oi];
        fro->cur.oi = drvo->next;
        struct rsrc_overlay* o = &rsrcmgr.overlays.data[drvo->index];
        if (o->key != fro->cur.key) goto next;
        const char* inpath = fro->cur.inpath.data;
        if (inpath) {
            if (o->srcpath) {
                if (fro->cur.inpath.len < o->srcpathlen) goto next;
                if (!fro->isdir) {
                    if (fro->cur.inpath.len == o->srcpathlen || inpath[o->srcpathlen] != '/') goto next;
                } else {
                    if (fro->cur.inpath.len > o->srcpathlen && inpath[o->srcpathlen] != '/') goto next;
                }
                if (strncmp(inpath, o->srcpath, o->srcpathlen)) goto next;
            }
        } else {
            if (o->srcpath) goto next;
        }
        fro->cur.path.len = 0;
        if (!cb_addpartstr(&fro->cur.path, o->destpath, o->destpathlen)) return -1;
        if (!cb_addpartstr(&fro->cur.path, inpath + o->srcpathlen, fro->cur.inpath.len - o->srcpathlen)) return -1;
        //if (!cb_nullterm(&fro->cur.path)) return -1;
        struct rsrc_drive* nd = &rsrcmgr.drives.data[o->destdrive];
        if (nd->overlays.head == -1U) {
            *d = nd;
            *path = &fro->cur.path;
            return 1;
        } else {
            if (fro->prev.len == 31) return -1;
            struct fro_state_elem* e;
            VLB_NEXTPTR(fro->prev, e, 2, 1, return -1;);
            *e = fro->cur;
            fro->cur.key = nd->key;
            fro->cur.inpath = fro->cur.path;
            fro->cur.d = nd;
            if (!cb_init(&fro->cur.path, 32)) {
                fro->cur.path.data = NULL;
                return false;
            }
            fro->cur.oi = nd->overlays.head;
            goto next_nocheck;
        }
    } else {
        *d = fro->cur.d;
        if (fro->prev.len) {
            cb_dump(&fro->cur.path);
            fro->cur = fro->prev.data[--fro->prev.len];
            *path = &fro->cur.path;
            return 1;
        }
        *path = &fro->cur.inpath;
        return 0;
    }
}

static bool getRsrcSrc_internal(enum rsrc_type type, uint32_t key, struct rsrc_drive* d, const char* path, size_t pathlen, struct rsrc_src* src) {
    if (d->overlays.head == -1U) {
        
    } else {
        
    }
    return false;
}
bool getRsrcSrc(enum rsrc_type t, uint32_t k, uint32_t d, const char* p, size_t pl, struct rsrc_src* s) {
    #if PSRC_MTLVL >= 1
    lockMutex(&rsrcmgr.lock);
    #endif
    bool ret;
    if (d == -1U) {
        struct charbuf cb;
        if (!cb_init(&cb, 64)) return false;
        if (!evalRsrcPath_internal(k, p, &d, &cb) || !cb_finalize(&cb)) {
            cb_dump(&cb);
            return false;
        }
        ret = getRsrcSrc_internal(t, k, &rsrcmgr.drives.data[d], cb.data, cb.len, s);
        cb_dump(&cb);
    } else {
        if (pl == (size_t)-1) pl = strlen(p);
        ret = getRsrcSrc_internal(t, k, &rsrcmgr.drives.data[d], p, pl, s);
    }
    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
    return ret;
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

void rsrcmgr_test_dumpstate(void);

void rsrcmgr_test(void) {
    if (!initRsrcMgr()) goto fail;

    uint32_t di[8];

    if ((di[0] = newRsrcDrive(
        RSRCDRIVE_NODUPNAME, -1, NULL,
        &(struct rsrc_drive_proto_opt){.type = RSRC_DRIVE_PROTO_NULL}
    )) == -1U) goto fail;
    if ((di[1] = newRsrcDrive(
        RSRCDRIVE_NODUPNAME, -1, "C",
        &(struct rsrc_drive_proto_opt){.type = RSRC_DRIVE_PROTO_NULL}
    )) == -1U) goto fail;
    if ((di[2] = newRsrcDrive(
        RSRCDRIVE_NODUPNAME, -1, "main",
        &(struct rsrc_drive_proto_opt){.type = RSRC_DRIVE_PROTO_NULL}
    )) == -1U) goto fail;

    #if 0
    if (!editRsrcDrive(
        di[0], RSRCDRIVE_NODUPNAME, NULL,
        &(struct rsrc_drive_proto_opt){.type = -1}
    )) goto fail;
    #endif

    uint32_t oi[8];

    if ((oi[0] = newRsrcOverlay(
        RSRCOVERLAY_NODUPSRCPATH | RSRCOVERLAY_NODUPDESTPATH, -1, -1,
        &(struct rsrc_overlay_opt){
            .srcdrive = -1,
            .srcdrivekey = -1,
            .srcpath = "C:/..",
            .destdrive = di[0],
            .destpath = "/a",
            .destpathlen = -1
        }
    )) == -1U) goto fail;
    if ((oi[1] = newRsrcOverlay(
        RSRCOVERLAY_NODUPSRCPATH | RSRCOVERLAY_NODUPDESTPATH, -1, -1,
        &(struct rsrc_overlay_opt){
            .srcdrive = di[0],
            .srcpath = "/a",
            .srcpathlen = -1,
            .destdrive = -1,
            .destdrivekey = -1,
            .destpath = "main:b/"
        }
    )) == -1U) goto fail;
    if ((oi[2] = newRsrcOverlay(
        RSRCOVERLAY_NODUPSRCPATH | RSRCOVERLAY_NODUPDESTPATH, -1, -1,
        &(struct rsrc_overlay_opt){
            .srcdrive = di[0],
            .srcpath = "/c",
            .srcpathlen = -1,
            .destdrive = di[1],
            .destpath = NULL,
            .destpathlen = 0
        }
    )) == -1U) goto fail;

    //delRsrcOverlay(1);

    if ((oi[3] = newRsrcOverlay(
        RSRCOVERLAY_NODUPSRCPATH | RSRCOVERLAY_NODUPDESTPATH, -1, -1,
        &(struct rsrc_overlay_opt){
            .srcdrive = di[0],
            .srcpath = "/abc",
            .srcpathlen = -1,
            .destdrive = di[0],
            .destpath = "/def",
            .destpathlen = -1
        }
    )) == -1U) goto fail;

    //delRsrcOverlay(2);

    rsrcmgr_test_dumpstate();

    #if 0
    puts("\n--------------------------------\n");
    {
        struct fro_state fro;
        struct rsrc_drive* ind = &rsrcmgr.drives.data[di[0]];
        static char* inpath = "/c/bruh";
        size_t inpathlen = (inpath) ? strlen(inpath) : 0;
        printf("FOLLOW {");
        if (ind->name) printf("\"%s\"", ind->name);
        else fputs("NULL", stdout);
        fputs(", ", stdout);
        if (inpath) printf("\"%s\"", inpath);
        else fputs("NULL", stdout);
        printf(" (%zu)}\n", inpathlen);
        followRsrcOverlay_start(&fro, -1, ind, inpath, inpathlen, false);
        while (1) {
            struct rsrc_drive* d;
            struct charbuf* path;
            int ret = followRsrcOverlay_next(&fro, &d, &path);
            if (ret == -1) {
                followRsrcOverlay_end(&fro);
                goto fail;
            }
            printf("  - TRY {");
            if (d->name) printf("\"%s\"", d->name);
            else fputs("NULL", stdout);
            fputs(", ", stdout);
            if (path->size) cb_nullterm(path);
            printf("\"%s\"", path->data);
            printf(" (%zu, %zu)}\n", path->len, path->size);
            if (!ret) break;
        }
        followRsrcOverlay_end(&fro);
    }
    #endif

    #if 0
    puts("\n--------------------------------\n");
    {
        uint32_t drive;
        struct charbuf path;
        if (!cb_init(&path, 16)) goto fail;
        if (!evalRsrcPath(-1, ":b/c/a/./../..", &drive, &path)) {
            cb_dump(&path);
            goto fail;
        }
        printf("[%08X] {%s}\n", drive, cb_peek(&path));
        cb_dump(&path);
    }
    #endif

    puts("\n--------------------------------\n");
    puts("PASS");
    exit(0);
    fail:;
    puts("FAIL");
    exit(1);
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
            case RSRC_DRIVE_PROTO_FS:
                puts("      Type: FS");
                printf("      Path: \"%s\"\n", d->proto.fs.path);
                break;
            case RSRC_DRIVE_PROTO_REDIR:
                puts("      Type:  REDIR");
                printf("      Drive: %u\n", d->proto.redir.drive);
                if (d->proto.redir.path) {
                    printf("      Path:  \"%s\"\n", d->proto.redir.path);
                } else {
                    puts("      Path:  (None)");
                }
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
                            break;
                        case RSRC_DRIVE_MAPPERITEMTYPE_DIR:
                            puts("        Type:  DIR");
                            break;
                    }
                    printf("        Path:  \"%s\"\n", item->path);
                }
                break;
        }
        puts("    Overlays:");
        if (d->overlays.head != -1U) {
            printf("      Head: %u\n", d->overlays.head);
        } else {
            puts("      Head: (Invalid)");
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
            printf("    Src path:   \"%s\"\n", o->srcpath);
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
            printf("    Dest path:  \"%s\"\n", o->destpath);
        } else {
            puts("    Dest path:  (None)");
        }
    }
    #if PSRC_MTLVL >= 1
    unlockMutex(&rsrcmgr.lock);
    #endif
}
