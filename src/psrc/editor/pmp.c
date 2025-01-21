#include "pmp.h"

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

bool pmp_read_next(struct pmp_read* pr, struct charbuf* n, struct pmp_vartype* t) {
    if (!pr->istext) {
        int c = fgetc(pr->f);
        if (c == EOF || (c & 0x7F) >= PMP_TYPE__COUNT) return false;
        t->type = c;
        bool isarray = ((c & 0x80) != 0);
        t->isarray = isarray;
        if (isarray) {
            
        }
    } else {
    }
}

void* pmp_read_readvar(struct pmp_read* pr) {
    
}

void pmp_read_close(struct pmp_read* pr) {
    
}

bool pmp_write_open(char* p, struct pmp_write* pw, bool text, enum pmp_write_comp comp) {
    
}

void pmp_write_next(struct pmp_write* pw, const char* n, long nl, struct pmp_vartype* t, void* d) {
    
}

void pmp_write_close(struct pmp_write* pw) {
    
}
