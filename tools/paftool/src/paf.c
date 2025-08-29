#include "paf.h"
#include "paf_fmt.h"

#include <unistd.h>

#define PAF_SPCFREE_FSTEXP_TARGETSLOTCT 16
enum paf_spcalloc_cb_ret {
    PAF_SPCALLOC_CB_RET_PASS,
    PAF_SPCALLOC_CB_RET_TAKE,
    PAF_SPCALLOC_CB_RET_HALT
};
typedef enum paf_spcalloc_cb_ret (*paf_spcalloc_cb)(void* userdata, uint32_t pos, uint32_t sz, uint32_t* takesz);
static uint32_t paf_spcalloc(struct paf* p, paf_spcalloc_cb cb, void* userdata);
static void paf_spcfree(struct paf* p, uint32_t pos, uint32_t sz);

static char* paf_readstr(struct paf* p, uint32_t pos, uint32_t* len);
void paf_writestr(struct paf* p, uint32_t pos, const char* d, uint32_t len);
void paf_updatestr(struct paf* p, uint32_t pos, const char* d, uint32_t len);

void paf_create(FILE* f, struct paf* p, uint32_t rentct, uint32_t fstslotct) {
    p->f = f;
    struct paf_fmt_header h = {.magic = {PAF_MAGIC}, .version = PAF_VER};
    paf_fmt_wr_header(f, &h, PAF_FMT_HEADER_MEMB_ROOTDIRPOS | PAF_FMT_HEADER_MEMB_FREESPCTRKRPOS);
    h.freespctrkrpos = ftell(f);
    if (fstslotct) {
        struct paf_fmt_freespctrkr fst = {.slotct = fstslotct};
        paf_fmt_wr_freespctrkr(f, &fst, 0);
        struct paf_fmt_freespctrkr_slot fsts = {0};
        for (uint32_t i = 0; i < fstslotct; ++i) {
            paf_fmt_wr_freespctrkr_slot(f, &fsts, 0);
        }
    }
    p->rootdirpos = h.rootdirpos = ftell(f);
    struct paf_fmt_dir r = {.entct = rentct};
    paf_fmt_wr_dir(f, &r, 0);
    struct paf_fmt_dir_ent rent = {0};
    for (uint32_t i = 0; i < rentct; ++i) {
        paf_fmt_wr_dir_ent(f, &rent, 0);
    }
    fseek(f, 0, SEEK_SET);
    paf_fmt_wr_header(f, &h, ~(PAF_FMT_HEADER_MEMB_ROOTDIRPOS | PAF_FMT_HEADER_MEMB_FREESPCTRKRPOS));
    VLB_INIT(p->prevdirs, 16, VLB_OOM_NOP);
    p->fstpos = h.freespctrkrpos;
    p->curdir = (struct paf_curdir){.pos = h.rootdirpos, .entavail = rentct};
}
bool paf_open(FILE* f, struct paf* p) {
    p->f = f;
    struct paf_fmt_header h;
    paf_fmt_rd_header(f, &h);
    if (h.magic[0] != (char[]){PAF_MAGIC}[0] ||
        h.magic[1] != (char[]){PAF_MAGIC}[1] ||
        h.magic[2] != (char[]){PAF_MAGIC}[2] ||
        h.version != PAF_VER) return false;
    VLB_INIT(p->prevdirs, 16, VLB_OOM_NOP);
    p->rootdirpos = h.rootdirpos;
    p->fstpos = h.freespctrkrpos;
    fseek(f, h.rootdirpos, SEEK_SET);
    struct paf_fmt_dir r;
    paf_fmt_rd_dir(f, &r);
    p->curdir = (struct paf_curdir){
        .pos = h.rootdirpos,
        .entused = r.usedent,
        .entavail = r.entct,
        .contpos = r.contpos,
        .curent = -1
    };
    return true;
}
void paf_close(struct paf* p, FILE** f) {
    VLB_FREE(p->prevdirs);
    if (f) *f = p->f;
}

void paf_gotoroot(struct paf* p) {
    p->prevdirs.len = 0;
    fseek(p->f, p->rootdirpos, SEEK_SET);
    struct paf_fmt_dir r;
    paf_fmt_rd_dir(p->f, &r);
    p->curdir = (struct paf_curdir){
        .pos = p->rootdirpos,
        .entused = r.usedent,
        .entavail = r.entct,
        .contpos = r.contpos,
        .curent = -1
    };
}
void paf_rewinddir(struct paf* p) {
    fseek(p->f, p->curdir.pos, SEEK_SET);
    struct paf_fmt_dir d;
    paf_fmt_rd_dir(p->f, &d);
    p->curdir.entused = d.usedent;
    p->curdir.entavail = d.entct;
    p->curdir.contpos = d.contpos;
    p->curdir.curent = -1;
}
bool paf_nextent(struct paf* p, struct paf_dirent* e) {
    if (p->curdir.curent + 1U == p->curdir.curent) return false;
    paf_fmt_rd_dir_ent(p->f, &p->curent);
    e->type = p->curent.type;
    e->name = paf_readstr(p, p->curent.namepos, &e->namelen);
    e->namecrc = p->curent.namecrc;
    return true;
}

static char* paf_readstr(struct paf* p, uint32_t pos, uint32_t* len) {
    uint32_t oldpos = ftell(p->f);
    fseek(p->f, pos, SEEK_SET);
    struct paf_fmt_str s;
    paf_fmt_rd_str(p->f, &s);
    char* d = malloc(s.strlen + 1);
    *len = s.strlen;
    d[s.strlen] = 0;
    char* tmpd = d;
    while (1) {
        if (s.strlen <= s.bytect) {
            fread(tmpd, 1, s.strlen, p->f);
            break;
        } else {
            fread(tmpd, 1, s.bytect, p->f);
            s.strlen -= s.bytect;
            fseek(p->f, s.contpos, SEEK_SET);
            struct paf_fmt_str_cont sc;
            paf_fmt_rd_str_cont(p->f, &sc);
            s.bytect = sc.bytect;
            s.contpos = sc.contpos;
        }
    }
    fseek(p->f, oldpos, SEEK_SET);
    return d;
}

// TODO: make this allocation and dellocation logic smarter
// - pos is assumed to only be non-zero if a contiguous allocation is wanted and other cases are not handled
// - possibly other problems
static uint32_t paf_spcalloc(struct paf* p, paf_spcalloc_cb cb, void* userdata) {
    if (!p->fstpos) return 0;
    uint32_t oldpos = ftell(p->f);
    fseek(p->f, p->fstpos, SEEK_SET);
    struct paf_fmt_freespctrkr fst;
    paf_fmt_rd_freespctrkr(p->f, &fst);
    struct paf_fmt_freespctrkr tmpfst = fst;
    uint32_t sloti;
    uint32_t slotpos;
    struct paf_fmt_freespctrkr_slot slot;
    uint32_t sz;
    while (1) {
        if (tmpfst.usedslots <= tmpfst.slotct) {
            for (sloti = 0; sloti < tmpfst.usedslots; ++sloti) {
                paf_fmt_rd_freespctrkr_slot(p->f, &slot);
                switch (cb(userdata, slot.pos, slot.sz, &sz)) {
                    case PAF_SPCALLOC_CB_RET_PASS:
                        break;
                    case PAF_SPCALLOC_CB_RET_TAKE:
                        slotpos = ftell(p->f) - PAF_FMT_FREESPCTRKR_SLOT_SZ;
                        goto slotfound;
                    case PAF_SPCALLOC_CB_RET_HALT:
                        fseek(p->f, oldpos, SEEK_SET);
                        return 0;
                }
            }
            fseek(p->f, oldpos, SEEK_SET);
            return 0;
        } else {
            for (sloti = 0; sloti < tmpfst.slotct; ++sloti) {
                paf_fmt_rd_freespctrkr_slot(p->f, &slot);
                switch (cb(userdata, slot.pos, slot.sz, &sz)) {
                    case PAF_SPCALLOC_CB_RET_PASS:
                        break;
                    case PAF_SPCALLOC_CB_RET_TAKE:
                        slotpos = ftell(p->f) - PAF_FMT_FREESPCTRKR_SLOT_SZ;
                        goto slotfound;
                    case PAF_SPCALLOC_CB_RET_HALT:
                        fseek(p->f, oldpos, SEEK_SET);
                        return 0;
                }
            }
            tmpfst.usedslots -= tmpfst.slotct;
            fseek(p->f, tmpfst.contpos, SEEK_SET);
            struct paf_fmt_freespctrkr_cont fstc;
            paf_fmt_rd_freespctrkr_cont(p->f, &fstc);
            tmpfst.slotct = fstc.slotct;
            tmpfst.contpos = fstc.contpos;
        }
    }
    slotfound:;
    if (slot.size != sz) {
        uint32_t tmp = slot.pos;
        slot.pos += sz;
        fseek(p->f, slotpos, SEEK_SET);
        paf_fmt_wr_freespctrkr_slot(p->f, &slot, ~PAF_FMT_FREESPCTRKR_SLOT_MEMB_POS);
        fseek(p->f, oldpos, SEEK_SET);
        return tmp;
    }
    --tmpfst.usedslots;
    if (sloti != tmpfst.usedslots) {
        while (1) {
            if (tmpfst.usedslots < tmpfst.slotct) {
                struct paf_fmt_freespctrkr_slot tmpslot;
                fseek(p->f, PAF_FMT_FREESPCTRKR_SLOT_SZ * tmpfst.usedslots, SEEK_CUR);
                paf_fmt_rd_freespctrkr_slot(p->f, &tmpslot);
                fseek(p->f, slotpos, SEEK_SET);
                paf_fmt_wr_freespctrkr_slot(p->f, &tmpslot, 0);
                break;
            } else {
                tmpfst.usedslots -= tmpfst.slotct;
                fseek(p->f, tmpfst.contpos, SEEK_SET);
                struct paf_fmt_freespctrkr_cont fstc;
                paf_fmt_rd_freespctrkr_cont(p->f, &fstc);
                tmpfst.slotct = fstc.slotct;
                tmpfst.contpos = fstc.contpos;
            }
        }
    }
    fseek(p->f, p->fstpos, SEEK_SET);
    --fst.usedslots;
    paf_fmt_wr_freespctrkr(p->f, &fst, ~PAF_FMT_FREESPCTRKR_MEMB_USEDSLOTS);
    fseek(p->f, oldpos, SEEK_SET);
    return slot.pos;
}
static void paf_spcfree(struct paf* p, uint32_t pos, uint32_t sz) {
    {
        fflush(p->f);
        fseek(p->f, 0, SEEK_END);
        uint32_t end = ftell(p->f);
        if (pos + sz == end) {
            #ifndef _WIN32
                ftruncate(fileno(p->f), pos);
                fflush(p->f);
                return;
            #else
                // TODO
            #endif
            //return;
        }
    }
    uint32_t oldpos = ftell(p->f);
    struct paf_fmt_freespctrkr fst;
    if (p->fstpos) {
        retry:;
        fseek(p->f, p->fstpos, SEEK_SET);
        paf_fmt_rd_freespctrkr(p->f, &fst);
        struct paf_fmt_freespctrkr tmpfst = fst;
        struct paf_fmt_freespctrkr_slot slot;
        uint32_t fstcpos = 0;
        while (1) {
            if (tmpfst.usedslots <= tmpfst.slotct) {
                for (uint32_t sloti = 0; sloti < tmpfst.usedslots; ++sloti) {
                    paf_fmt_rd_freespctrkr_slot(p->f, &slot);
                    if (slot.pos + slot.size == pos) {
                        slot.size += sz;
                        fseek(p->f, -(long)PAF_FMT_FREESPCTRKR_SLOT_SZ, SEEK_CUR);
                        paf_fmt_wr_freespctrkr_slot(p->f, &slot, ~PAF_FMT_FREESPCTRKR_SLOT_MEMB_SIZE);
                        goto ret;
                    }
                }
                if (tmpfst.usedslots != tmpfst.slotct) {
                    slot = (struct paf_fmt_freespctrkr_slot){.pos = pos, .size = sz};
                    paf_fmt_wr_freespctrkr_slot(p->f, &slot, 0);
                } else {
                    uint32_t tmpo = PAF_FMT_FREESPCTRKR_SLOT_SZ * tmpfst.slotct;
                    tmpo += (!fstcpos) ? p->fstpos + PAF_FMT_FREESPCTRKR_SZ : fstcpos + PAF_FMT_FREESPCTRKR_CONT_SZ;
                    uint32_t tmp;
                    uint32_t ct;
                    for (ct = PAF_SPCFREE_FSTEXP_TARGETSLOTCT; ct > 0; ct /= 2) {
                        tmp = paf_spcalloc(p, tmpo, PAF_FMT_FREESPCTRKR_SLOT_SZ * ct);
                        if (tmp) {
                            tmpfst.slotct += ct;
                            if (!fstcpos) {
                                fseek(p->f, p->fstpos, SEEK_SET);
                                paf_fmt_wr_freespctrkr(p->f, &tmpfst, ~PAF_FMT_FREESPCTRKR_MEMB_SLOTCT);
                            } else {
                                fseek(p->f, fstcpos, SEEK_SET);
                                struct paf_fmt_freespctrkr_cont tmpfstc = {.slotct = tmpfst.slotct};
                                paf_fmt_wr_freespctrkr_cont(p->f, &tmpfstc, ~PAF_FMT_FREESPCTRKR_CONT_MEMB_SLOTCT);
                            }
                            goto retry;
                        }
                    }
                    for (ct = PAF_SPCFREE_FSTEXP_TARGETSLOTCT; ct > 0; ct /= 2) {
                        tmp = paf_spcalloc(p, 0, PAF_FMT_FREESPCTRKR_CONT_SZ + PAF_FMT_FREESPCTRKR_SLOT_SZ * ct);
                        if (tmp) goto mkcont;
                    }
                    fseek(p->f, 0, SEEK_END);
                    tmp = ftell(p->f);
                    ct = PAF_SPCFREE_FSTEXP_TARGETSLOTCT;
                    mkcont:;
                    if (!fstcpos) {
                        tmpfst.contpos = tmp;
                        fseek(p->f, p->fstpos, SEEK_SET);
                        paf_fmt_wr_freespctrkr(p->f, &tmpfst, ~PAF_FMT_FREESPCTRKR_MEMB_CONTPOS);
                    } else {
                        fseek(p->f, fstcpos, SEEK_SET);
                        struct paf_fmt_freespctrkr_cont tmpfstc = {.contpos = tmp};
                        paf_fmt_wr_freespctrkr_cont(p->f, &tmpfstc, ~PAF_FMT_FREESPCTRKR_CONT_MEMB_CONTPOS);
                    }
                    fseek(p->f, tmp, SEEK_SET);
                    struct paf_fmt_freespctrkr_cont tmpfstc = {.slotct = ct};
                    paf_fmt_wr_freespctrkr_cont(p->f, &tmpfstc, 0);
                    slot = (struct paf_fmt_freespctrkr_slot){.pos = pos, .size = sz};
                    paf_fmt_wr_freespctrkr_slot(p->f, &slot, 0);
                    slot = (struct paf_fmt_freespctrkr_slot){0};
                    for (uint32_t i = 1; i < ct; ++i) {
                        paf_fmt_wr_freespctrkr_slot(p->f, &slot, 0);
                    }
                }
                goto ret;
            } else {
                for (uint32_t sloti = 0; sloti < tmpfst.slotct; ++sloti) {
                    paf_fmt_rd_freespctrkr_slot(p->f, &slot);
                    if (slot.pos + slot.size == pos) {
                        slot.size += sz;
                        fseek(p->f, -(long)PAF_FMT_FREESPCTRKR_SLOT_SZ, SEEK_CUR);
                        paf_fmt_wr_freespctrkr_slot(p->f, &slot, ~PAF_FMT_FREESPCTRKR_SLOT_MEMB_SIZE);
                        goto ret;
                    }
                }
                tmpfst.usedslots -= tmpfst.slotct;
                fstcpos = tmpfst.contpos;
                fseek(p->f, tmpfst.contpos, SEEK_SET);
                struct paf_fmt_freespctrkr_cont fstc;
                paf_fmt_rd_freespctrkr_cont(p->f, &fstc);
                tmpfst.slotct = fstc.slotct;
                tmpfst.contpos = fstc.contpos;
            }
        }
    }
    fseek(p->f, 0, SEEK_END);
    p->fstpos = ftell(p->f);
    fst = (struct paf_fmt_freespctrkr){.usedslots = 1, .slotct = PAF_SPCFREE_FSTEXP_TARGETSLOTCT};
    paf_fmt_wr_freespctrkr(p->f, &fst, 0);
    struct paf_fmt_freespctrkr_slot slot = {.pos = pos, .size = sz};
    paf_fmt_wr_freespctrkr_slot(p->f, &slot, 0);
    slot = (struct paf_fmt_freespctrkr_slot){0};
    for (uint32_t i = 1; i < PAF_SPCFREE_FSTEXP_TARGETSLOTCT; ++i) {
        paf_fmt_wr_freespctrkr_slot(p->f, &slot, 0);
    }
    fseek(p->f, 0, SEEK_SET);
    struct paf_fmt_header h = {.freespctrkrpos = p->fstpos};
    paf_fmt_wr_header(p->f, &h, ~PAF_FMT_HEADER_MEMB_FREESPCTRKRPOS);
    ret:;
    fseek(p->f, oldpos, SEEK_SET);
    return;
}
