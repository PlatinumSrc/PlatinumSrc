// TODO: redo

#ifndef PSRC_REUSABLE
    #include "../rcmgralloc.h"
#else
    #define PSRC_MTLVL 0
#endif

#include "config.h"

#include "../string.h"
#include "../crc.h"
#ifndef PSRC_REUSABLE
    #include "../logging.h"
    #include "../debug.h"
#endif

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "../glue.h"

char* cfg_getvar(struct cfg* cfg, const char* sect, const char* var) {
    #if PSRC_MTLVL >= 2
    lockMutex(&cfg->lock);
    #endif
    struct cfg_sect* sectptr = NULL;
    uint32_t crc;
    if (sect) {
        crc = strcasecrc32(sect);
        for (int i = 0; i < cfg->sectcount; ++i) {
            struct cfg_sect* ptr = &cfg->sectdata[i];
            if (ptr->name && ptr->namecrc == crc && !strcasecmp(ptr->name, sect)) {
                sectptr = ptr;
                break;
            }
        }
    } else {
        for (int i = 0; i < cfg->sectcount; ++i) {
            struct cfg_sect* ptr = &cfg->sectdata[i];
            if (ptr->name && !ptr->namecrc && !*ptr->name) {
                sectptr = ptr;
                break;
            }
        }
    }
    if (!sectptr) {
        #if PSRC_MTLVL >= 2
        unlockMutex(&cfg->lock);
        #endif
        return NULL;
    }
    crc = strcasecrc32(var);
    for (int i = 0; i < sectptr->varcount; ++i) {
        struct cfg_var* ptr = &sectptr->vardata[i];
        if (ptr->name && ptr->namecrc == crc && !strcasecmp(ptr->name, var)) {
            char* tmp = strdup(ptr->data);
            #if PSRC_MTLVL >= 2
            unlockMutex(&cfg->lock);
            #endif
            return tmp;
        }
    }
    #if PSRC_MTLVL >= 2
    unlockMutex(&cfg->lock);
    #endif
    return NULL;
}

bool cfg_getvarto(struct cfg* cfg, const char* sect, const char* var, char* data, size_t size) {
    #if PSRC_MTLVL >= 2
    lockMutex(&cfg->lock);
    #endif
    struct cfg_sect* sectptr = NULL;
    uint32_t crc;
    if (sect) {
        crc = strcasecrc32(sect);
        for (int i = 0; i < cfg->sectcount; ++i) {
            struct cfg_sect* ptr = &cfg->sectdata[i];
            if (ptr->name && ptr->namecrc == crc && !strcasecmp(ptr->name, sect)) {
                sectptr = ptr;
                break;
            }
        }
    } else {
        for (int i = 0; i < cfg->sectcount; ++i) {
            struct cfg_sect* ptr = &cfg->sectdata[i];
            if (ptr->name && !ptr->namecrc && !*ptr->name) {
                sectptr = ptr;
                break;
            }
        }
    }
    if (!sectptr) {
        #if PSRC_MTLVL >= 2
        unlockMutex(&cfg->lock);
        #endif
        return false;
    }
    crc = strcasecrc32(var);
    for (int i = 0; i < sectptr->varcount; ++i) {
        struct cfg_var* ptr = &sectptr->vardata[i];
        if (ptr->name && ptr->namecrc == crc && !strcasecmp(ptr->name, var)) {
            char* from = ptr->data;
            --size;
            for (size_t j = 0; j < size; ++j) {
                char c = *from;
                if (!c) break;
                *data++ = c;
                ++from;
            }
            *data = 0;
            #if PSRC_MTLVL >= 2
            unlockMutex(&cfg->lock);
            #endif
            return true;
        }
    }
    #if PSRC_MTLVL >= 2
    unlockMutex(&cfg->lock);
    #endif
    return false;
}

void cfg_setvar(struct cfg* cfg, const char* sect, const char* var, const char* data, bool overwrite) {
    #if PSRC_MTLVL >= 2
    lockMutex(&cfg->lock);
    #endif
    struct cfg_sect* sectptr = NULL;
    uint32_t crc;
    if (sect) {
        uint32_t crc = strcasecrc32(sect);
        for (int i = 0; i < cfg->sectcount; ++i) {
            struct cfg_sect* ptr = &cfg->sectdata[i];
            if (ptr->name && ptr->namecrc == crc && !strcasecmp(ptr->name, sect)) {
                sectptr = ptr;
                break;
            }
        }
        if (!sectptr) {
            for (int i = 0; i < cfg->sectcount; ++i) {
                struct cfg_sect* ptr = &cfg->sectdata[i];
                if (!ptr->name) {
                    sectptr = ptr;
                    break;
                }
            }
            if (!sectptr) {
                int i = cfg->sectcount++;
                cfg->sectdata = realloc(cfg->sectdata, cfg->sectcount * sizeof(*cfg->sectdata));
                sectptr = &cfg->sectdata[i];
            }
            sectptr->name = strdup(sect);
            sectptr->namecrc = crc;
            sectptr->varcount = 0;
            sectptr->vardata = NULL;
        }
    } else {
        for (int i = 0; i < cfg->sectcount; ++i) {
            struct cfg_sect* ptr = &cfg->sectdata[i];
            if (ptr->name && !ptr->namecrc && !*ptr->name) {
                sectptr = ptr;
                break;
            }
        }
        if (!sectptr) {
            for (int i = 0; i < cfg->sectcount; ++i) {
                struct cfg_sect* ptr = &cfg->sectdata[i];
                if (!ptr->name) {
                    sectptr = ptr;
                    break;
                }
            }
            if (!sectptr) {
                int i = cfg->sectcount++;
                cfg->sectdata = realloc(cfg->sectdata, cfg->sectcount * sizeof(*cfg->sectdata));
                sectptr = &cfg->sectdata[i];
            }
            sectptr->name = calloc(1, 1);
            sectptr->namecrc = 0;
            sectptr->varcount = 0;
            sectptr->vardata = NULL;
        }
    }
    crc = strcasecrc32(var);
    struct cfg_var* varptr = NULL;
    for (int i = 0; i < sectptr->varcount; ++i) {
        struct cfg_var* ptr = &sectptr->vardata[i];
        if (ptr->name && ptr->namecrc == crc && !strcasecmp(ptr->name, var)) {
            varptr = ptr;
            break;
        }
    }
    if (varptr) {
        if (overwrite) {
            free(varptr->data);
            varptr->data = strdup(data);
        }
    } else {
        for (int i = 0; i < sectptr->varcount; ++i) {
            struct cfg_var* ptr = &sectptr->vardata[i];
            if (!ptr->name) {
                varptr = ptr;
                break;
            }
        }
        if (!varptr) {
            int i = sectptr->varcount++;
            sectptr->vardata = realloc(sectptr->vardata, sectptr->varcount * sizeof(*sectptr->vardata));
            varptr = &sectptr->vardata[i];
        }
        varptr->name = strdup(var);
        varptr->namecrc = crc;
        varptr->data = strdup(data);
    }
    #if PSRC_MTLVL >= 2
    unlockMutex(&cfg->lock);
    #endif
}

void cfg_delvar(struct cfg* cfg, const char* sect, const char* var) {
    #if PSRC_MTLVL >= 2
    lockMutex(&cfg->lock);
    #endif
    struct cfg_sect* sectptr = NULL;
    uint32_t crc;
    if (sect) {
        crc = strcasecrc32(sect);
        for (int i = 0; i < cfg->sectcount; ++i) {
            struct cfg_sect* ptr = &cfg->sectdata[i];
            if (ptr->name && ptr->namecrc == crc && !strcasecmp(ptr->name, sect)) {
                sectptr = ptr;
                break;
            }
        }
    } else {
        for (int i = 0; i < cfg->sectcount; ++i) {
            struct cfg_sect* ptr = &cfg->sectdata[i];
            if (ptr->name && !ptr->namecrc && !*ptr->name) {
                sectptr = ptr;
                break;
            }
        }
    }
    if (!sectptr) {
        #if PSRC_MTLVL >= 2
        unlockMutex(&cfg->lock);
        #endif
        return;
    }
    crc = strcasecrc32(var);
    for (int i = 0; i < sectptr->varcount; ++i) {
        struct cfg_var* ptr = &sectptr->vardata[i];
        if (ptr->name && ptr->namecrc == crc && !strcasecmp(ptr->name, var)) {
            free(ptr->name);
            ptr->name = NULL;
            free(ptr->data);
            break;
        }
    }
    #if PSRC_MTLVL >= 2
    unlockMutex(&cfg->lock);
    #endif
}

void cfg_delsect(struct cfg* cfg, const char* sect) {
    #if PSRC_MTLVL >= 2
    lockMutex(&cfg->lock);
    #endif
    struct cfg_sect* sectptr = NULL;
    if (sect) {
        uint32_t crc = strcasecrc32(sect);
        for (int i = 0; i < cfg->sectcount; ++i) {
            struct cfg_sect* ptr = &cfg->sectdata[i];
            if (ptr->name && ptr->namecrc == crc && !strcasecmp(ptr->name, sect)) {
                sectptr = ptr;
                break;
            }
        }
    } else {
        for (int i = 0; i < cfg->sectcount; ++i) {
            struct cfg_sect* ptr = &cfg->sectdata[i];
            if (ptr->name && !ptr->namecrc && !*ptr->name) {
                sectptr = ptr;
                break;
            }
        }
    }
    if (!sectptr) {
        #if PSRC_MTLVL >= 2
        unlockMutex(&cfg->lock);
        #endif
        return;
    }
    for (int i = 0; i < sectptr->varcount; ++i) {
        struct cfg_var* ptr = &sectptr->vardata[i];
        if (!ptr->name) continue;
        free(ptr->name);
        free(ptr->data);
    }
    free(sectptr->name);
    sectptr->name = NULL;
    free(sectptr->vardata);
    #if PSRC_MTLVL >= 2
    unlockMutex(&cfg->lock);
    #endif
}

void cfg_delall(struct cfg* cfg) {
    #if PSRC_MTLVL >= 2
    lockMutex(&cfg->lock);
    #endif
    for (int i = 0; i < cfg->sectcount; ++i) {
        struct cfg_sect* sectptr = &cfg->sectdata[i];
        if (!sectptr->name) continue;
        for (int j = 0; j < sectptr->varcount; ++j) {
            struct cfg_var* ptr = &sectptr->vardata[j];
            if (!ptr->name) continue;
            free(ptr->name);
            free(ptr->data);
        }
        free(sectptr->name);
        sectptr->name = NULL;
        free(sectptr->vardata);
    }
    #if PSRC_MTLVL >= 2
    unlockMutex(&cfg->lock);
    #endif
}

static inline int gethex(int c) {
    if (c >= 'a' && c <= 'f') c -= 32;
    return (c >= '0' && c <= '9') ? c - 48 : ((c >= 'A' && c <= 'F') ? c - 55 : -1);
}

static int interpesc(PSRC_DATASTREAM_T ds, int c, char* out) {
    switch (c) {
        case DS_END:
            return -1;
        case 'a':
            *out++ = '\a'; *out = 0; return 1;
        case 'b':
            *out++ = '\b'; *out = 0; return 1;
        case 'e':
            *out++ = '\x1b'; *out = 0; return 1;
        case 'f':
            *out++ = '\f'; *out = 0; return 1;
        case 'n':
            *out++ = '\n'; *out = 0; return 1;
        case 'r':
            *out++ = '\r'; *out = 0; return 1;
        case 't':
            *out++ = '\t'; *out = 0; return 1;
        case 'v':
            *out++ = '\v'; *out = 0; return 1;
        case 'x': {
            int c1 = ds_text_getc_fullinline(ds);
            if (c1 == DS_END) return -1;
            int c2 = ds_text_getc_fullinline(ds);
            if (c2 == DS_END) return -1;
            int h1, h2;
            if ((h1 = gethex(c1)) == -1 || (h2 = gethex(c2)) == -1) {
                *out++ = 'x'; *out++ = c1; *out++ = c2; *out = 0;
                return 0;
            }
            *out++ = (h1 << 4) + h2; *out = 0;
            return 1;
        } break;
    }
    *out++ = c; *out = 0;
    return 0;
}

static void interpfinal(char* s, struct charbuf* b) {
    char c;
    bool inStr = false;
    while ((c = *s++)) {
        if (inStr) {
            if (c == '\\') {
                c = *s++;
                if (!c) {
                    cb_add(b, '\\');
                    return;
                } else if (c == '"') {
                    cb_add(b, '"');
                } else {
                    cb_add(b, '\\');
                    cb_add(b, c);
                }
            } else {
                if (c == '"') inStr = false;
                else cb_add(b, c);
            }
        } else {
            if (c == '"') inStr = true;
            else cb_add(b, c);
        }
    }
}

void cfg_read(struct cfg* cfg, PSRC_DATASTREAM_T ds, bool overwrite) {
    struct cfg_sect* sectptr = NULL;
    {
        for (int i = 0; i < cfg->sectcount; ++i) {
            struct cfg_sect* ptr = &cfg->sectdata[i];
            if (ptr->name && !ptr->namecrc && !*ptr->name) {
                sectptr = ptr;
                break;
            }
        }
        if (!sectptr) {
            for (int i = 0; i < cfg->sectcount; ++i) {
                struct cfg_sect* ptr = &cfg->sectdata[i];
                if (!ptr->name) {
                    sectptr = ptr;
                    break;
                }
            }
            if (!sectptr) {
                int i = cfg->sectcount++;
                cfg->sectdata = realloc(cfg->sectdata, cfg->sectcount * sizeof(*cfg->sectdata));
                sectptr = &cfg->sectdata[i];
            }
            sectptr->name = calloc(1, 1);
            sectptr->namecrc = 0;
            sectptr->varcount = 0;
            sectptr->vardata = NULL;
        }
    }
    struct charbuf sect;
    struct charbuf var;
    struct charbuf data;
    cb_init(&sect, 256);
    cb_init(&var, 256);
    cb_init(&data, 256);
    bool inStr;
    while (1) {
        int c;
        newline:;
        inStr = false;
        cb_clear(&sect);
        cb_clear(&var);
        cb_clear(&data);
        do {
            c = ds_text_getc_inline(ds);
        } while (c == ' ' || c == '\t' || c == '\n');
        if (c == DS_END) goto endloop;
        if (c == '#') {
            while (1) {
                c = ds_text_getc(ds);
                if (c == DS_END) goto endloop;
                if (c == '\n') goto newline;
            }
        } else if (c == '[') {
            do {
                c = ds_text_getc(ds);
                if (c == '\n') goto newline;
            } while (c == ' ' || c == '\t');
            if (c == DS_END) goto endloop;
            while (1) {
                if (inStr) {
                    if (c == '\\') {
                        c = ds_text_getc(ds);
                        if (c == DS_END) goto endloop;
                        if (c == '\n') goto newline;
                        char tmpbuf[4];
                        c = interpesc(ds, c, tmpbuf);
                        if (c == -1) goto endloop;
                        if (c == 0) cb_add(&sect, '\\');
                        cb_addstr(&sect, tmpbuf);
                    } else {
                        cb_add(&sect, c);
                        if (c == '"') inStr = false;
                    }
                } else {
                    if (c == ']') {
                        do {
                            c = ds_text_getc_inline(ds);
                        } while (c == ' ' || c == '\t');
                        if (c == DS_END) goto endloop;
                        if (c != '\n') {
                            int c2 = c;
                            do {
                                c = ds_text_getc(ds);
                                if (c == DS_END) goto endloop;
                            } while (c != '\n');
                            if (c2 != '#') goto newline;
                        }
                        if (sect.len > 0) {
                            char tmpc = sect.data[sect.len - 1];
                            while (tmpc == ' ' || tmpc == '\t') {
                                --sect.len;
                                tmpc = sect.data[sect.len - 1];
                            }
                        }
                        char* tmp;
                        cb_reinit(&sect, 256, &tmp);
                        interpfinal(tmp, &sect);
                        free(tmp);
                        cb_reinit(&sect, 256, &tmp);
                        sectptr = NULL;
                        uint32_t crc = strcasecrc32(tmp);
                        for (int i = 0; i < cfg->sectcount; ++i) {
                            struct cfg_sect* ptr = &cfg->sectdata[i];
                            if (ptr->name && ptr->namecrc == crc && !strcasecmp(ptr->name, tmp)) {
                                sectptr = ptr;
                                free(tmp);
                                break;
                            }
                        }
                        if (!sectptr) {
                            for (int i = 0; i < cfg->sectcount; ++i) {
                                struct cfg_sect* ptr = &cfg->sectdata[i];
                                if (!ptr->name) {
                                    sectptr = ptr;
                                    break;
                                }
                            }
                            if (!sectptr) {
                                int i = cfg->sectcount++;
                                cfg->sectdata = realloc(cfg->sectdata, cfg->sectcount * sizeof(*cfg->sectdata));
                                sectptr = &cfg->sectdata[i];
                            }
                            sectptr->name = tmp;
                            sectptr->namecrc = crc;
                            sectptr->varcount = 0;
                            sectptr->vardata = NULL;
                        }
                        goto newline;
                    } else {
                        cb_add(&sect, c);
                        if (c == '"') inStr = true;
                    }
                }
                c = ds_text_getc_fullinline(ds);
                if (c == DS_END) goto endloop;
                if (c == '\n') goto newline;
            }
        } else {
            while (1) {
                if (inStr) {
                    if (c == '\\') {
                        c = ds_text_getc(ds);
                        if (c == DS_END) goto endloop;
                        if (c == '\n') goto newline;
                        char tmpbuf[4];
                        c = interpesc(ds, c, tmpbuf);
                        if (c == -1) goto endloop;
                        if (c == 0) cb_add(&var, '\\');
                        cb_addstr(&var, tmpbuf);
                    } else {
                        cb_add(&var, c);
                        if (c == '"') inStr = false;
                    }
                } else {
                    if (c == '=') {
                        do {
                            c = ds_text_getc_inline(ds);
                        } while (c == ' ' || c == '\t');
                        while (1) {
                            if (inStr) {
                                if (c == '\\') {
                                    c = ds_text_getc(ds);
                                    if (c == DS_END) goto endloop;
                                    if (c == '\n') {
                                        cb_add(&data, '\\');
                                        cb_add(&data, '\n');
                                    } else {
                                        char tmpbuf[4];
                                        c = interpesc(ds, c, tmpbuf);
                                        if (c == -1) goto endloop;
                                        if (c == 0) cb_add(&sect, '\\');
                                        cb_addstr(&data, tmpbuf);
                                    }
                                } else {
                                    cb_add(&data, c);
                                    if (c == '"') inStr = false;
                                }
                            } else {
                                if (c == '\n' || c == '#' || c == DS_END) {
                                    char* varstr;
                                    char* datastr;
                                    if (var.len > 0) {
                                        char tmpc = var.data[var.len - 1];
                                        while (tmpc == ' ' || tmpc == '\t') {
                                            --var.len;
                                            tmpc = var.data[var.len - 1];
                                        }
                                    }
                                    cb_reinit(&var, 256, &varstr);
                                    interpfinal(varstr, &var);
                                    free(varstr);
                                    cb_reinit(&var, 256, &varstr);
                                    if (data.len > 0) {
                                        char tmpc = data.data[data.len - 1];
                                        while (tmpc == ' ' || tmpc == '\t') {
                                            --data.len;
                                            tmpc = data.data[data.len - 1];
                                        }
                                    }
                                    cb_reinit(&data, 256, &datastr);
                                    interpfinal(datastr, &data);
                                    free(datastr);
                                    cb_reinit(&data, 256, &datastr);
                                    uint32_t crc = strcasecrc32(varstr);
                                    struct cfg_var* varptr = NULL;
                                    for (int i = 0; i < sectptr->varcount; ++i) {
                                        struct cfg_var* ptr = &sectptr->vardata[i];
                                        if (ptr->name && ptr->namecrc == crc && !strcasecmp(ptr->name, varstr)) {
                                            varptr = ptr;
                                            free(varstr);
                                            break;
                                        }
                                    }
                                    if (varptr) {
                                        if (overwrite) {
                                            free(varptr->data);
                                            varptr->data = datastr;
                                        } else {
                                            free(datastr);
                                        }
                                    } else {
                                        for (int i = 0; i < sectptr->varcount; ++i) {
                                            struct cfg_var* ptr = &sectptr->vardata[i];
                                            if (!ptr->name) {
                                                varptr = ptr;
                                                break;
                                            }
                                        }
                                        if (!varptr) {
                                            int i = sectptr->varcount++;
                                            sectptr->vardata = realloc(sectptr->vardata, sectptr->varcount * sizeof(*sectptr->vardata));
                                            varptr = &sectptr->vardata[i];
                                        }
                                        varptr->name = varstr;
                                        varptr->namecrc = crc;
                                        varptr->data = datastr;
                                    }
                                    if (c == '#') {
                                        while (1) {
                                            c = ds_text_getc(ds);
                                            if (c == DS_END) goto endloop;
                                            if (c == '\n') goto newline;
                                        }
                                    }
                                    goto newline;
                                } else {
                                    cb_add(&data, c);
                                    if (c == '"') inStr = true;
                                }
                            }
                            c = ds_text_getc_fullinline(ds);
                        }
                    } else {
                        cb_add(&var, c);
                        if (c == '"') inStr = true;
                    }
                }
                c = ds_text_getc_fullinline(ds);
                if (c == DS_END) goto endloop;
                if (c == '\n') goto newline;
            }
        }
    }
    endloop:;
    cb_dump(&sect);
    cb_dump(&var);
    cb_dump(&data);
}

void cfg_open(PSRC_DATASTREAM_T ds, struct cfg* cfg) {
    memset(cfg, 0, sizeof(*cfg));
    #if PSRC_MTLVL >= 2
    createMutex(&cfg->lock);
    #endif
    if (ds) {
        #ifndef PSRC_REUSABLE
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG, "Reading config from '%s'...", ds->name);
        #endif
        #endif
        cfg_read(cfg, ds, true);
    }
}

void cfg_merge(struct cfg* cfg, PSRC_DATASTREAM_T ds, bool overwrite) {
    #ifndef PSRC_REUSABLE
    #if !defined(PSRC_REUSABLE) && DEBUG(1)
    plog(LL_INFO | LF_DEBUG | LF_DEBUG, "Reading config (to merge) from '%s'...", ds->name);
    #endif
    #endif
    cfg_read(cfg, ds, overwrite);
}

void cfg_mergemem(struct cfg* cfg, struct cfg* from, bool overwrite) {
    int sectcount = from->sectcount;
    for (int secti = 0; secti < sectcount; ++secti) {
        struct cfg_sect* sect = &from->sectdata[secti];
        if (sect->name) {
            int varcount = sect->varcount;
            for (int vari = 0; vari < varcount; ++ vari) {
                struct cfg_var* var = &sect->vardata[vari];
                if (var->name) cfg_setvar(cfg, sect->name, var->name, var->data, overwrite);
            }
        }
    }
}

void cfg_close(struct cfg* cfg) {
    int sectcount = cfg->sectcount;
    for (int secti = 0; secti < sectcount; ++secti) {
        struct cfg_sect* sect = &cfg->sectdata[secti];
        if (sect->name) {
            int varcount = sect->varcount;
            for (int vari = 0; vari < varcount; ++vari) {
                struct cfg_var* var = &sect->vardata[vari];
                if (var->name) {
                    free(var->name);
                    free(var->data);
                }
            }
            free(sect->name);
            free(sect->vardata);
        }
    }
    free(cfg->sectdata);
    #if PSRC_MTLVL >= 2
    destroyMutex(&cfg->lock);
    #endif
}
