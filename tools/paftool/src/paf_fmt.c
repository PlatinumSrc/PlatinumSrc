#include "paf_fmt.h"

#include <psrc/byteorder.h>

#define WRITE32(v, m) do {\
    if (!(membskip & (m))) {\
        uint32_t tmp = swaple32(in->v);\
        fwrite(&tmp, 4, 1, (f));\
    } else {\
        fseek(f, 4, SEEK_CUR);\
    }\
} while (0)
#define READ32(v) do {\
    uint32_t tmp;\
    fread(&tmp, 4, 1, f);\
    out->v = swaple32(tmp);\
} while (0)

void paf_fmt_wr_header(FILE* f, const struct paf_fmt_header* in, unsigned membskip) {
    if (!(membskip & PAF_FMT_HEADER_MEMB_MAGIC)) {
        fputc(in->magic[0], f);
        fputc(in->magic[1], f);
        fputc(in->magic[2], f);
    } else {
        fseek(f, 3, SEEK_CUR);
    }
    if (!(membskip & PAF_FMT_HEADER_MEMB_VERSION)) fputc(in->version, f);
    else fseek(f, 1, SEEK_CUR);
    WRITE32(rootdirpos, PAF_FMT_HEADER_MEMB_ROOTDIRPOS);
    WRITE32(freespctrkrpos, PAF_FMT_HEADER_MEMB_FREESPCTRKRPOS);
}
void paf_fmt_rd_header(FILE* f, struct paf_fmt_header* out) {
    fread(out->magic, 1, 3, f);
    out->version = fgetc(f);
    READ32(rootdirpos);
    READ32(freespctrkrpos);
}

void paf_fmt_wr_freespctrkr(FILE* f, const struct paf_fmt_freespctrkr* in, unsigned membskip) {
    WRITE32(usedslots, PAF_FMT_FREESPCTRKR_MEMB_USEDSLOTS);
    WRITE32(slotct, PAF_FMT_FREESPCTRKR_MEMB_SLOTCT);
    WRITE32(contpos, PAF_FMT_FREESPCTRKR_MEMB_CONTPOS);
}
void paf_fmt_wr_freespctrkr_cont(FILE* f, const struct paf_fmt_freespctrkr_cont* in, unsigned membskip) {
    WRITE32(slotct, PAF_FMT_FREESPCTRKR_CONT_MEMB_SLOTCT);
    WRITE32(contpos, PAF_FMT_FREESPCTRKR_CONT_MEMB_CONTPOS);
}
void paf_fmt_wr_freespctrkr_slot(FILE* f, const struct paf_fmt_freespctrkr_slot* in, unsigned membskip) {
    WRITE32(pos, PAF_FMT_FREESPCTRKR_SLOT_MEMB_POS);
    WRITE32(size, PAF_FMT_FREESPCTRKR_SLOT_MEMB_SIZE);
}
void paf_fmt_rd_freespctrkr(FILE* f, struct paf_fmt_freespctrkr* out) {
    READ32(usedslots);
    READ32(slotct);
    READ32(contpos);
}
void paf_fmt_rd_freespctrkr_cont(FILE* f, struct paf_fmt_freespctrkr_cont* out) {
    READ32(slotct);
    READ32(contpos);
}
void paf_fmt_rd_freespctrkr_slot(FILE* f, struct paf_fmt_freespctrkr_slot* out) {
    READ32(pos);
    READ32(size);
}

void paf_fmt_wr_dir(FILE* f, const struct paf_fmt_dir* in, unsigned membskip) {
    WRITE32(usedent, PAF_FMT_DIR_MEMB_USEDENT);
    WRITE32(entct, PAF_FMT_DIR_MEMB_SLOTCT);
    WRITE32(contpos, PAF_FMT_DIR_MEMB_CONTPOS);
}
void paf_fmt_wr_dir_cont(FILE* f, const struct paf_fmt_dir_cont* in, unsigned membskip) {
    WRITE32(entct, PAF_FMT_DIR_CONT_MEMB_SLOTCT);
    WRITE32(contpos, PAF_FMT_DIR_CONT_MEMB_CONTPOS);
}
void paf_fmt_wr_dir_ent(FILE* f, const struct paf_fmt_dir_ent* in, unsigned membskip) {
    if (!(membskip & PAF_FMT_DIR_ENT_MEMB_TYPE)) fputc(in->type, f);
    else fseek(f, 1, SEEK_CUR);
    WRITE32(namepos, PAF_FMT_DIR_ENT_MEMB_NAMEPOS);
    WRITE32(namecrc, PAF_FMT_DIR_ENT_MEMB_NAMECRC);
    WRITE32(datapos, PAF_FMT_DIR_ENT_MEMB_DATAPOS);
}
void paf_fmt_rd_dir(FILE* f, struct paf_fmt_dir* out) {
    READ32(usedent);
    READ32(entct);
    READ32(contpos);
}
void paf_fmt_rd_dir_cont(FILE* f, struct paf_fmt_dir_cont* out) {
    READ32(entct);
    READ32(contpos);
}
void paf_fmt_rd_dir_ent(FILE* f, struct paf_fmt_dir_ent* out) {
    out->type = fgetc(f);
    READ32(namepos);
    READ32(namecrc);
    READ32(datapos);
}

void paf_fmt_wr_file(FILE* f, const struct paf_fmt_file* in, unsigned membskip) {
    if (!(membskip & PAF_FMT_DIR_ENT_MEMB_TYPE)) fputc(in->comp, f);
    else fseek(f, 1, SEEK_CUR);
    WRITE32(usedbytes, PAF_FMT_FILE_MEMB_USEDBYTES);
    WRITE32(origsz, PAF_FMT_FILE_MEMB_ORIGSZ);
    WRITE32(bytect, PAF_FMT_FILE_MEMB_BYTECT);
    WRITE32(contpos, PAF_FMT_FILE_MEMB_CONTPOS);
}
void paf_fmt_wr_file_cont(FILE* f, const struct paf_fmt_file_cont* in, unsigned membskip) {
    WRITE32(bytect, PAF_FMT_FILE_CONT_MEMB_BYTECT);
    WRITE32(nextpos, PAF_FMT_FILE_CONT_MEMB_NEXTPOS);
    WRITE32(prevpos, PAF_FMT_FILE_CONT_MEMB_PREVPOS);
}
void paf_fmt_rd_file(FILE* f, struct paf_fmt_file* out) {
    out->comp = fgetc(f);
    READ32(usedbytes);
    READ32(origsz);
    READ32(bytect);
    READ32(contpos);
}
void paf_fmt_rd_file_cont(FILE* f, struct paf_fmt_file_cont* out) {
    READ32(bytect);
    READ32(nextpos);
    READ32(prevpos);
}

void paf_fmt_wr_str(FILE* f, const struct paf_fmt_str* in, unsigned membskip) {
    WRITE32(strlen, PAF_FMT_STR_MEMB_STRLEN);
    WRITE32(bytect, PAF_FMT_STR_MEMB_BYTECT);
    WRITE32(contpos, PAF_FMT_STR_MEMB_CONTPOS);
}
void paf_fmt_wr_str_cont(FILE* f, const struct paf_fmt_str_cont* in, unsigned membskip) {
    WRITE32(bytect, PAF_FMT_STR_CONT_MEMB_BYTECT);
    WRITE32(contpos, PAF_FMT_STR_CONT_MEMB_CONTPOS);
}
void paf_fmt_rd_str(FILE* f, struct paf_fmt_str* out) {
    READ32(strlen);
    READ32(bytect);
    READ32(contpos);
}
void paf_fmt_rd_str_cont(FILE* f, struct paf_fmt_str_cont* out) {
    READ32(bytect);
    READ32(contpos);
}
