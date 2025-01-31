#include "pmp.h"

#include "../byteorder.h"

#define _STR(x) #x
#define STR(x) _STR(x)

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
            bool isarray = pr->lasttype.isarray;
            switch ((uint8_t)pr->lasttype.type) {
                case PMP_TYPE_BOOL: fseek(pr->f, (!isarray) ? 1 : (pr->lasttype.size + 7) / 8, SEEK_CUR); break;
                case PMP_TYPE_I8:
                case PMP_TYPE_U8: fseek(pr->f, (!isarray) ? 1 : pr->lasttype.size, SEEK_CUR); break;
                case PMP_TYPE_I16:
                case PMP_TYPE_U16: fseek(pr->f, (!isarray) ? 1 : pr->lasttype.size * 2, SEEK_CUR); break;
                case PMP_TYPE_I32:
                case PMP_TYPE_U32:
                case PMP_TYPE_F32: fseek(pr->f, (!isarray) ? 1 : pr->lasttype.size * 4, SEEK_CUR); break;
                case PMP_TYPE_I64:
                case PMP_TYPE_U64:
                case PMP_TYPE_F64: fseek(pr->f, (!isarray) ? 1 : pr->lasttype.size * 8, SEEK_CUR); break;
                case PMP_TYPE_STR: {
                    uint32_t tmpu32;
                    if (!isarray) {
                        fread(&tmpu32, 4, 1, pr->f);
                        tmpu32 = swaple32(tmpu32);
                    } else {
                        for (uint32_t i = 0; i < pr->lasttype.size; ++i) {
                            fread(&tmpu32, 4, 1, pr->f);
                            tmpu32 = swaple32(tmpu32);
                            fseek(pr->f, tmpu32, SEEK_CUR);
                        }
                    }
                } break;
            }
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
        uint32_t tmpu32;
        if (isarray) {
            fread(&tmpu32, 4, 1, pr->f);
            tmpu32 = swaple32(tmpu32);
            ot->size = tmpu32;
            pr->lasttype.size = tmpu32;
        }
        fread(&tmpu32, 4, 1, pr->f);
        tmpu32 = swaple32(tmpu32);
        for (uint32_t i = 0; i < tmpu32; ++i) {
            cb_add(n, fgetc(pr->f));
        }
    } else {
    }
    return true;
}

void pmp_read_readvar(struct pmp_read* pr, void* o) {
    pr->lastwasvar = 0;
    bool isarray = pr->lasttype.isarray;
    switch ((uint8_t)pr->lasttype.type) {
        case PMP_TYPE_BOOL: {
            if (!isarray) {
                *(bool*)o = fgetc(pr->f);
            } else {
                uint8_t* a = malloc((pr->lasttype.size + 7) / 8);
                fread(a, 1, (pr->lasttype.size + 7) / 8, pr->f);
                *(uint8_t**)o = a;
            }
        } break;
        case PMP_TYPE_I8:
        case PMP_TYPE_U8: {
            if (!isarray) {
                *(uint8_t*)o = fgetc(pr->f);
            } else {
                uint8_t* a = malloc(pr->lasttype.size);
                fread(a, 1, pr->lasttype.size, pr->f);
                *(uint8_t**)o = a;
            }
        } break;
        case PMP_TYPE_I16:
        case PMP_TYPE_U16: {
            if (!isarray) {
                uint16_t v;
                fread(&v, 2, 1, pr->f);
                *(uint16_t*)o = swaple16(v);
            } else {
                uint16_t* a = malloc(pr->lasttype.size * sizeof(*a));
                fread(a, 2, pr->lasttype.size, pr->f);
                #if BYTEORDER == BO_BE
                for (uint32_t i = 0; i < pr->lasttype.size; ++i) {
                    a[i] = swaple16(a[i]);
                }
                #endif
                *(uint16_t**)o = a;
            }
        } break;
        case PMP_TYPE_I32:
        case PMP_TYPE_U32:
        case PMP_TYPE_F32: {
            if (!isarray) {
                uint32_t v;
                fread(&v, 4, 1, pr->f);
                *(uint32_t*)o = swaple32(v);
            } else {
                uint32_t* a = malloc(pr->lasttype.size * sizeof(*a));
                fread(a, 4, pr->lasttype.size, pr->f);
                #if BYTEORDER == BO_BE
                for (uint32_t i = 0; i < pr->lasttype.size; ++i) {
                    a[i] = swaple32(a[i]);
                }
                #endif
                *(uint32_t**)o = a;
            }
        } break;
        case PMP_TYPE_I64:
        case PMP_TYPE_U64:
        case PMP_TYPE_F64: {
            if (!isarray) {
                uint64_t v;
                fread(&v, 8, 1, pr->f);
                *(uint64_t*)o = swaple32(v);
            } else {
                uint64_t* a = malloc(pr->lasttype.size * sizeof(*a));
                fread(a, 8, pr->lasttype.size, pr->f);
                #if BYTEORDER == BO_BE
                for (uint32_t i = 0; i < pr->lasttype.size; ++i) {
                    a[i] = swaple64(a[i]);
                }
                #endif
                *(uint64_t**)o = a;
            }
        } break;
        case PMP_TYPE_STR: {
            uint32_t tmpu32;
            if (!isarray) {
                fread(&tmpu32, 4, 1, pr->f);
                tmpu32 = swaple32(tmpu32);
                char* s = malloc(tmpu32 + 1);
                ((struct pmp_string*)o)->data = s;
                ((struct pmp_string*)o)->len = tmpu32;
                fread(s, 1, tmpu32, pr->f);
                s[tmpu32] = 0;
            } else {
                struct pmp_string* a = malloc(pr->lasttype.size * sizeof(*a));
                for (uint32_t i = 0; i < pr->lasttype.size; ++i) {
                    fread(&tmpu32, 4, 1, pr->f);
                    tmpu32 = swaple32(tmpu32);
                    char* s = malloc(tmpu32 + 1);
                    a[i].data = s;
                    a[i].len = tmpu32;
                    fread(s, 1, tmpu32, pr->f);
                    s[tmpu32] = 0;
                }
                *(struct pmp_string**)o = a;
            }
        } break;
    }
}

void pmp_read_close(struct pmp_read* pr) {
    fclose(pr->f);
}

bool pmp_write_open(char* p, struct pmp_write* pw, bool text, enum pmp_write_comp comp) {
    FILE* f;
    if (!text) {
        if (!(f = fopen(p, "wb"))) return false;
        fputc('P', f);
        fputc('M', f);
        fputc('P', f);
        fputc(PMP_VER, f);
        pw->istext = 0;
    } else {
        if (!(f = fopen(p, "w"))) return false;
        fputs("# PlatinumSrc Map Project Text version " STR(PMP_VER) "\n", f);
        pw->istext = 1;
    }
    pw->f = f;
    return true;
}

void pmp_write_next(struct pmp_write* pw, const char* n, uint32_t nl, struct pmp_vartype* t, void* d) {
    if (!pw->istext) {
        fputc((t->type & 0x7F) | (t->isarray << 7), pw->f);
        bool isarray = t->isarray;
        uint32_t tmpu32;
        if (isarray) {
            tmpu32 = swaple32(t->size);
            fwrite(&tmpu32, 4, 1, pw->f);
        }
        tmpu32 = swaple32(nl);
        fwrite(&tmpu32, 4, 1, pw->f);
        fwrite(n, 1, nl, pw->f);
        switch (t->type) {
            default:
                break;
            case PMP_TYPE_BOOL: {
                if (!isarray) {
                    
                } else {
                    
                }
            } break;
            case PMP_TYPE_I8:
            case PMP_TYPE_U8: {
                if (!isarray) {
                    
                } else {
                    
                }
            } break;
            case PMP_TYPE_I16:
            case PMP_TYPE_U16: {
                if (!isarray) {
                    
                } else {
                    
                }
            } break;
            case PMP_TYPE_I32:
            case PMP_TYPE_U32:
            case PMP_TYPE_F32: {
                if (!isarray) {
                    
                } else {
                    
                }
            } break;
            case PMP_TYPE_I64:
            case PMP_TYPE_U64:
            case PMP_TYPE_F64: {
                if (!isarray) {
                    
                } else {
                    
                }
            } break;
            case PMP_TYPE_STR: {
                
            } break;
        }
    } else {
        
    }
}

void pmp_write_close(struct pmp_write* pw) {
    fclose(pw->f);
}
