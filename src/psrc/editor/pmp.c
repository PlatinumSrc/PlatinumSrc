#include "pmp.h"

#include "../byteorder.h"

bool pmp_read_open(char* p, bool text, struct pmp_read* pr) {
    FILE* f;
    if (!text) {
        if (!(f = fopen(p, "rb"))) return false;
        char tmp[4];
        fread(tmp, 1, 4, pr->f);
        if (tmp[0] != 'P' || tmp[1] != 'M' || tmp[2] != 'P' || tmp[3] != PMP_VER) {
            fclose(f);
            return false;
        }
        pr->istext = 0;
    } else {
        if (!(f = fopen(p, "r"))) return false;
        pr->istext = 1;
    }
    pr->f = f;
    pr->lastwasvar = 0;
    return true;
}

bool pmp_read_next(struct pmp_read* pr, struct charbuf* n, struct pmp_vartype* ot) {
    if (!pr->istext) {
        if (pr->lastwasvar) {
            long sz;
            bool isarray = pr->lasttype.isarray;
            switch ((uint8_t)pr->lasttype.type) {
                case PMP_TYPE_BOOL: sz = (!isarray) ? 1 : (pr->lasttype.size + 7) / 8; break;
                case PMP_TYPE_I8: sz = (!isarray) ? 1 : pr->lasttype.size; break;
                case PMP_TYPE_I16: sz = (!isarray) ? 1 : pr->lasttype.size * 2; break;
                case PMP_TYPE_I32: sz = (!isarray) ? 1 : pr->lasttype.size * 4; break;
                case PMP_TYPE_I64: sz = (!isarray) ? 1 : pr->lasttype.size * 8; break;
                case PMP_TYPE_U8: sz = (!isarray) ? 1 : pr->lasttype.size; break;
                case PMP_TYPE_U16: sz = (!isarray) ? 1 : pr->lasttype.size * 2; break;
                case PMP_TYPE_U32: sz = (!isarray) ? 1 : pr->lasttype.size * 4; break;
                case PMP_TYPE_U64: sz = (!isarray) ? 1 : pr->lasttype.size * 8; break;
                case PMP_TYPE_F32: sz = (!isarray) ? 1 : pr->lasttype.size * 4; break;
                case PMP_TYPE_F64: sz = (!isarray) ? 1 : pr->lasttype.size * 8; break;
                case PMP_TYPE_STR: {
                    uint32_t tmpu32;
                    fread(&tmpu32, 4, 1, pr->f);
                    sz = swaple32(tmpu32);
                    if (isarray) {
                        for (uint32_t i = 1; i < pr->lasttype.size; ++i) {
                            fseek(pr->f, sz, SEEK_CUR);
                            fread(&tmpu32, 4, 1, pr->f);
                            sz = swaple32(tmpu32);
                        }
                    }
                } break;
            }
            fseek(pr->f, sz, SEEK_CUR);
        }
        int c = fgetc(pr->f);
        if (c == EOF) return false;
        enum pmp_type t = c & 0x7F;
        if (t >= PMP_TYPE__COUNT) return false;
        ot->type = t;
        if (!(pr->lastwasvar = (t >= PMP_TYPE__VAR))) return true;
        pr->lasttype.type = t;
        bool isarray = ((c & 0x80) != 0);
        ot->isarray = isarray;
        pr->lasttype.isarray = isarray;
        if (isarray) {
            uint32_t tmpu32;
            fread(&tmpu32, 4, 1, pr->f);
            tmpu32 = swaple32(tmpu32);
            ot->size = tmpu32;
            pr->lasttype.size = tmpu32;
        }
    } else {
    }
}

void* pmp_read_readvar(struct pmp_read* pr) {
    pr->lastwasvar = 0;
}

void pmp_read_close(struct pmp_read* pr) {
    fclose(pr->f);
}

bool pmp_write_open(char* p, struct pmp_write* pw, bool text, enum pmp_write_comp comp) {
    
}

void pmp_write_next(struct pmp_write* pw, const char* n, long nl, struct pmp_vartype* t, void* d) {
    
}

void pmp_write_close(struct pmp_write* pw) {
    
}
