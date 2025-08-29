#ifndef PAFTOOL_PAF_H
#define PAFTOOL_PAF_H

#include "paf_fmt.h"

#include <psrc/vlb.h>
#include <psrc/string.h>

#include <stdint.h>
#include <stdbool.h>

struct paf_dirent {
    enum paf_dirent_type type;
    const char* name;
    uint32_t namelen;
    uint32_t namecrc;
};

struct paf_curdir {
    uint32_t pos;
    uint32_t entused;
    uint32_t entavail;
    uint32_t contpos;
    uint32_t curent;
};
struct paf_curfile {
    uint32_t pos;
    enum paf_filez comp;
    uint32_t dataused;
    uint32_t dataavail;
    uint32_t nextpos;
    uint32_t prevpos;
    uint32_t filecursor;
};

struct paf {
    FILE* f;
    uint32_t rootdirpos;
    uint32_t fstpos;
    struct VLB(uint32_t) prevdirs;
    struct paf_curdir curdir;
    struct paf_fmt_dir_ent curent;
    struct paf_curfile curfile;
    struct {
        char* data;
        uint32_t len;
    } tmpstr;
};

void paf_create(FILE*, struct paf*, uint32_t rootdirent, uint32_t freespctrkrslots);
bool paf_open(FILE*, struct paf*);
void paf_close(struct paf*, FILE**);

void paf_gotoroot(struct paf*);
void paf_rewinddir(struct paf*);
bool paf_nextent(struct paf*, struct paf_dirent*);
void paf_renameent(struct paf*);
void paf_deleteent(struct paf*);
bool paf_enterdir(struct paf*);
void paf_exitdir(struct paf*);
bool paf_openfile(struct paf*);
void paf_rewindfile(struct paf*);
void paf_writefile(struct paf*, uint32_t datasz, const uint8_t* data);
void paf_readfile(struct paf*, uint32_t datasz, uint8_t* data);
void paf_skipfile(struct paf*, uint32_t len);
char* paf_readlink(struct paf*);
// these 3 create funcs will replace the current entry; rewind first to append; current entry is set to the one created or replaced; name = NULL to keep existing name
void paf_createdir(struct paf*, unsigned flags, char* name, uint32_t namelen, uint32_t namecrc, uint32_t entct);
void paf_createfile(struct paf*, unsigned flags, char* name, uint32_t namelen, uint32_t namecrc, uint32_t datasz, const uint8_t* data /* null for 0s */);
void paf_createlink(struct paf*, unsigned flags, char* name, uint32_t namelen, uint32_t namecrc, char* target, uint32_t targetlen);
#define PAF_CREATEDIR_CONTIGNAME (1 << 0)
#define PAF_CREATEDIR_CONTIGENTS (1 << 1)
#define PAF_CREATEDIR_SELECT     (1 << 2)
#define PAF_CREATEDIR_ENTER      (1 << 3)
#define PAF_CREATEFILE_CONTIGNAME (1 << 0)
#define PAF_CREATEFILE_CONTIGDATA (1 << 1)
#define PAF_CREATEFILE_SELECT     (1 << 2)
#define PAF_CREATELINK_CONTIGNAME   (1 << 0)
#define PAF_CREATELINK_CONTIGTARGET (1 << 1)
#define PAF_CREATELINK_SELECT       (1 << 2)

#endif
