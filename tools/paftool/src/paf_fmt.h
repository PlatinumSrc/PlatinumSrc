#ifndef PAFTOOL_PAF_FMT_H
#define PAFTOOL_PAF_FMT_H

#include <stdio.h>
#include <stdint.h>

#define PAF_VER 0
#define PAF_MAGIC 'P', 'A', 'F'

#define PAF_FMT_HEADER_SZ           (8U)
#define PAF_FMT_FREESPCTRKR_SZ      (12U)
#define PAF_FMT_FREESPCTRKR_CONT_SZ (8U)
#define PAF_FMT_FREESPCTRKR_SLOT_SZ (8U)
#define PAF_FMT_DIR_SZ              (12U)
#define PAF_FMT_DIR_CONT_SZ         (8U)
#define PAF_FMT_DIR_ENT_SZ          (13U)
#define PAF_FMT_FILE_SZ             (17U)
#define PAF_FMT_FILE_CONT_SZ        (12U)
#define PAF_FMT_STR_SZ              (12U)
#define PAF_FMT_STR_CONT_SZ         (8U)

enum paf_dirent_type {
    PAF_DIRENT_DIR,
    PAF_DIRENT_FILE,
    PAF_DIRENT_LINK
};
enum paf_filez {
    PAF_FILEZ_NONE,
    PAF_FILEZ_LZ4
};

struct paf_fmt_header {
    char magic[3];
    uint8_t version;
    uint32_t rootdirpos;
    uint32_t freespctrkrpos;
};
#define PAF_FMT_HEADER_MEMB_MAGIC          (1U << 0)
#define PAF_FMT_HEADER_MEMB_VERSION        (1U << 1)
#define PAF_FMT_HEADER_MEMB_ROOTDIRPOS     (1U << 2)
#define PAF_FMT_HEADER_MEMB_FREESPCTRKRPOS (1U << 3)

struct paf_fmt_freespctrkr {
    uint32_t usedslots;
    uint32_t slotct;
    uint32_t contpos;
};
#define PAF_FMT_FREESPCTRKR_MEMB_USEDSLOTS (1U << 0)
#define PAF_FMT_FREESPCTRKR_MEMB_SLOTCT    (1U << 1)
#define PAF_FMT_FREESPCTRKR_MEMB_CONTPOS   (1U << 2)
struct paf_fmt_freespctrkr_cont {
    uint32_t slotct;
    uint32_t contpos;
};
#define PAF_FMT_FREESPCTRKR_CONT_MEMB_SLOTCT  (1U << 0)
#define PAF_FMT_FREESPCTRKR_CONT_MEMB_CONTPOS (1U << 1)
struct paf_fmt_freespctrkr_slot {
    uint32_t pos;
    uint32_t size;
};
#define PAF_FMT_FREESPCTRKR_SLOT_MEMB_POS  (1U << 0)
#define PAF_FMT_FREESPCTRKR_SLOT_MEMB_SIZE (1U << 1)

struct paf_fmt_dir {
    uint32_t usedent;
    uint32_t entct;
    uint32_t contpos;
};
#define PAF_FMT_DIR_MEMB_USEDENT (1U << 0)
#define PAF_FMT_DIR_MEMB_SLOTCT  (1U << 1)
#define PAF_FMT_DIR_MEMB_CONTPOS (1U << 2)
struct paf_fmt_dir_cont {
    uint32_t entct;
    uint32_t contpos;
};
#define PAF_FMT_DIR_CONT_MEMB_SLOTCT  (1U << 0)
#define PAF_FMT_DIR_CONT_MEMB_CONTPOS (1U << 1)
struct paf_fmt_dir_ent {
    enum paf_dirent_type type;
    uint32_t namepos;
    uint32_t namecrc;
    uint32_t datapos;
};
#define PAF_FMT_DIR_ENT_MEMB_TYPE    (1U << 0)
#define PAF_FMT_DIR_ENT_MEMB_NAMEPOS (1U << 1)
#define PAF_FMT_DIR_ENT_MEMB_NAMECRC (1U << 2)
#define PAF_FMT_DIR_ENT_MEMB_DATAPOS (1U << 3)

struct paf_fmt_file {
    enum paf_filez comp;
    uint32_t usedbytes;
    uint32_t origsz;
    uint32_t bytect;
    uint32_t contpos;
};
#define PAF_FMT_FILE_MEMB_COMP      (1U << 0)
#define PAF_FMT_FILE_MEMB_USEDBYTES (1U << 1)
#define PAF_FMT_FILE_MEMB_ORIGSZ    (1U << 2)
#define PAF_FMT_FILE_MEMB_BYTECT    (1U << 3)
#define PAF_FMT_FILE_MEMB_CONTPOS   (1U << 4)
struct paf_fmt_file_cont {
    uint32_t bytect;
    uint32_t nextpos;
    uint32_t prevpos;
};
#define PAF_FMT_FILE_CONT_MEMB_BYTECT  (1U << 0)
#define PAF_FMT_FILE_CONT_MEMB_NEXTPOS (1U << 1)
#define PAF_FMT_FILE_CONT_MEMB_PREVPOS (1U << 2)

struct paf_fmt_str {
    uint32_t strlen;
    uint32_t bytect;
    uint32_t contpos;
};
#define PAF_FMT_STR_MEMB_STRLEN  (1U << 0)
#define PAF_FMT_STR_MEMB_BYTECT  (1U << 1)
#define PAF_FMT_STR_MEMB_CONTPOS (1U << 2)
struct paf_fmt_str_cont {
    uint32_t bytect;
    uint32_t contpos;
};
#define PAF_FMT_STR_CONT_MEMB_BYTECT  (1U << 0)
#define PAF_FMT_STR_CONT_MEMB_CONTPOS (1U << 1)

void paf_fmt_wr_header(FILE*, const struct paf_fmt_header*, unsigned membskip);
void paf_fmt_rd_header(FILE*, struct paf_fmt_header*);

void paf_fmt_wr_freespctrkr(FILE*, const struct paf_fmt_freespctrkr*, unsigned membskip);
void paf_fmt_wr_freespctrkr_cont(FILE*, const struct paf_fmt_freespctrkr_cont*, unsigned membskip);
void paf_fmt_wr_freespctrkr_slot(FILE*, const struct paf_fmt_freespctrkr_slot*, unsigned membskip);
void paf_fmt_rd_freespctrkr(FILE*, struct paf_fmt_freespctrkr*);
void paf_fmt_rd_freespctrkr_cont(FILE*, struct paf_fmt_freespctrkr_cont*);
void paf_fmt_rd_freespctrkr_slot(FILE*, struct paf_fmt_freespctrkr_slot*);

void paf_fmt_wr_dir(FILE*, const struct paf_fmt_dir*, unsigned membskip);
void paf_fmt_wr_dir_cont(FILE*, const struct paf_fmt_dir_cont*, unsigned membskip);
void paf_fmt_wr_dir_ent(FILE*, const struct paf_fmt_dir_ent*, unsigned membskip);
void paf_fmt_rd_dir(FILE*, struct paf_fmt_dir*);
void paf_fmt_rd_dir_cont(FILE*, struct paf_fmt_dir_cont*);
void paf_fmt_rd_dir_ent(FILE*, struct paf_fmt_dir_ent*);

void paf_fmt_wr_file(FILE*, const struct paf_fmt_file*, unsigned membskip);
void paf_fmt_wr_file_cont(FILE*, const struct paf_fmt_file_cont*, unsigned membskip);
void paf_fmt_rd_file(FILE*, struct paf_fmt_file*);
void paf_fmt_rd_file_cont(FILE*, struct paf_fmt_file_cont*);

void paf_fmt_wr_str(FILE*, const struct paf_fmt_str*, unsigned membskip);
void paf_fmt_wr_str_cont(FILE*, const struct paf_fmt_str_cont*, unsigned membskip);
void paf_fmt_rd_str(FILE*, struct paf_fmt_str*);
void paf_fmt_rd_str_cont(FILE*, struct paf_fmt_str_cont*);

#endif
