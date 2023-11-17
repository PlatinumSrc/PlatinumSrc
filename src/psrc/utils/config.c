#include "config.h"
#include "filesystem.h"
#include "string.h"
#include "crc.h"
#include "logging.h"
#include "../debug.h"

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "../glue.h"

char* cfg_getvar(struct cfg* cfg, const char* sect, const char* var) {
    lockMutex(&cfg->lock);
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
        unlockMutex(&cfg->lock);
        return NULL;
    }
    crc = strcasecrc32(var);
    for (int i = 0; i < sectptr->varcount; ++i) {
        struct cfg_var* ptr = &sectptr->vardata[i];
        if (ptr->name && ptr->namecrc == crc && !strcasecmp(ptr->name, var)) {
            unlockMutex(&cfg->lock);
            return strdup(ptr->data);
        }
    }
    unlockMutex(&cfg->lock);
    return NULL;
}

bool cfg_getvarto(struct cfg* cfg, const char* sect, const char* var, char* data, size_t size) {
    lockMutex(&cfg->lock);
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
        unlockMutex(&cfg->lock);
        return false;
    }
    crc = strcasecrc32(var);
    for (int i = 0; i < sectptr->varcount; ++i) {
        struct cfg_var* ptr = &sectptr->vardata[i];
        if (ptr->name && ptr->namecrc == crc && !strcasecmp(ptr->name, var)) {
            unlockMutex(&cfg->lock);
            char* from = ptr->data;
            for (size_t j = 0; j < size; ++j) {
                if (!(*data = *from)) break;
                ++data;
                ++from;
            }
            return true;
        }
    }
    unlockMutex(&cfg->lock);
    return false;
}

void cfg_setvar(struct cfg* cfg, const char* sect, const char* var, const char* data, bool overwrite) {
    lockMutex(&cfg->lock);
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
    unlockMutex(&cfg->lock);
}

void cfg_delvar(struct cfg* cfg, const char* sect, const char* var) {
    lockMutex(&cfg->lock);
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
        unlockMutex(&cfg->lock);
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
    unlockMutex(&cfg->lock);
}

void cfg_delsect(struct cfg* cfg, const char* sect) {
    lockMutex(&cfg->lock);
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
        unlockMutex(&cfg->lock);
        return;
    }
    for (int i = 0; i < sectptr->varcount; ++i) {
        struct cfg_var* ptr = &sectptr->vardata[i];
        if (ptr->name) {
            free(ptr->name);
            free(ptr->data);
        }
    }
    free(sectptr->name);
    sectptr->name = NULL;
    free(sectptr->vardata);
    unlockMutex(&cfg->lock);
}

static int fgetc_skip(FILE* f) {
    int c;
    do {
        c = fgetc(f);
    } while ((c < ' ' || c >= 127) && (c != '\n' && c != '\t' && c != EOF));
    return c;
}

static inline int gethex(int c) {
    if (c >= 'a' && c <= 'f') c -= 32;
    return (c >= '0' && c <= '9') ? c - 48 : ((c >= 'A' && c <= 'F') ? c - 55 : -1);
}

static inline int interpesc(FILE* f, int c, char* out) {
    switch (c) {
        case EOF:
            return -1;
        case 'a':
            *out++ = '\a'; *out = 0; return 1;
        case 'b':
            *out++ = '\b'; *out = 0; return 1;
        case 'e':
            *out++ = '\e'; *out = 0; return 1;
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
            int c1 = fgetc_skip(f);
            if (c1 == EOF) return -1;
            int c2 = fgetc_skip(f);
            if (c2 == EOF) return -1;
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

void cfg_read(struct cfg* cfg, FILE* f, bool overwrite) {
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
            c = fgetc_skip(f);
        } while (c == ' ' || c == '\t' || c == '\n');
        if (c == EOF) goto endloop;
        if (c == '#') {
            while (1) {
                c = fgetc_skip(f);
                if (c == EOF) goto endloop;
                if (c == '\n') goto newline;
            }
        } else if (c == '[') {
            do {
                c = fgetc_skip(f);
                if (c == '\n') goto newline;
            } while (c == ' ' || c == '\t');
            if (c == EOF) goto endloop;
            while (1) {
                if (inStr) {
                    if (c == '\\') {
                        c = fgetc_skip(f);
                        if (c == EOF) goto endloop;
                        if (c == '\n') goto newline;
                        char tmpbuf[4];
                        c = interpesc(f, c, tmpbuf);
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
                            c = fgetc_skip(f);
                        } while (c == ' ' || c == '\t');
                        if (c == EOF) goto endloop;
                        if (c != '\n') {
                            int c2 = c;
                            do {
                                c = fgetc_skip(f);
                                if (c == EOF) goto endloop;
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
                        char* tmp = cb_reinit(&sect, 256);
                        interpfinal(tmp, &sect);
                        free(tmp);
                        tmp = cb_reinit(&sect, 256);
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
                c = fgetc_skip(f);
                if (c == EOF) goto endloop;
                if (c == '\n') goto newline;
            }
        } else {
            while (1) {
                if (inStr) {
                    if (c == '\\') {
                        c = fgetc_skip(f);
                        if (c == EOF) goto endloop;
                        if (c == '\n') goto newline;
                        char tmpbuf[4];
                        c = interpesc(f, c, tmpbuf);
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
                            c = fgetc_skip(f);
                        } while (c == ' ' || c == '\t');
                        while (1) {
                            if (inStr) {
                                if (c == '\\') {
                                    c = fgetc_skip(f);
                                    if (c == EOF) goto endloop;
                                    if (c == '\n') {
                                        cb_add(&data, '\\');
                                        cb_add(&data, '\n');
                                    } else {
                                        char tmpbuf[4];
                                        c = interpesc(f, c, tmpbuf);
                                        if (c == -1) goto endloop;
                                        if (c == 0) cb_add(&sect, '\\');
                                        cb_addstr(&data, tmpbuf);
                                    }
                                } else {
                                    cb_add(&data, c);
                                    if (c == '"') inStr = false;
                                }
                            } else {
                                if (c == '\n' || c == '#' || c == EOF) {
                                    char* varstr;
                                    char* datastr;
                                    if (var.len > 0) {
                                        char tmpc = var.data[var.len - 1];
                                        while (tmpc == ' ' || tmpc == '\t') {
                                            --var.len;
                                            tmpc = var.data[var.len - 1];
                                        }
                                    }
                                    varstr = cb_reinit(&var, 256);
                                    interpfinal(varstr, &var);
                                    free(varstr);
                                    varstr = cb_reinit(&var, 256);
                                    if (data.len > 0) {
                                        char tmpc = data.data[data.len - 1];
                                        while (tmpc == ' ' || tmpc == '\t') {
                                            --data.len;
                                            tmpc = data.data[data.len - 1];
                                        }
                                    }
                                    datastr = cb_reinit(&data, 256);
                                    interpfinal(datastr, &data);
                                    free(datastr);
                                    datastr = cb_reinit(&data, 256);
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
                                            c = fgetc_skip(f);
                                            if (c == EOF) goto endloop;
                                            if (c == '\n') goto newline;
                                        }
                                    }
                                    goto newline;
                                } else {
                                    cb_add(&data, c);
                                    if (c == '"') inStr = true;
                                }
                            }
                            c = fgetc_skip(f);
                        }
                    } else {
                        cb_add(&var, c);
                        if (c == '"') inStr = true;
                    }
                }
                c = fgetc_skip(f);
                if (c == EOF) goto endloop;
                if (c == '\n') goto newline;
            }
        }
    }
    endloop:;
    cb_dump(&sect);
    cb_dump(&var);
    cb_dump(&data);
}

static inline struct cfg* cfg_open_new(void) {
    struct cfg* cfg = malloc(sizeof(*cfg));
    memset(cfg, 0, sizeof(*cfg));
    createMutex(&cfg->lock);
    return cfg;
}

struct cfg* cfg_open(const char* p) {
    if (p) {
        int tmp = isFile(p);
        if (tmp < 1) {
            if (tmp) {
                #if DEBUG(1)
                plog(LL_ERROR | LF_DEBUG | LF_FUNC, LE_NOEXIST(p));
                #endif
            } else {
                plog(LL_ERROR | LF_FUNC, LE_ISDIR(p));
            }
            return NULL;
        }
        FILE* f = fopen(p, "r");
        if (!f) {
            int e = errno;
            plog(LL_WARN | LF_FUNC, LE_CANTOPEN(p, e));
            return NULL;
        }
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG, "Reading config %s...", p);
        #endif
        struct cfg* cfg = cfg_open_new();
        cfg_read(cfg, f, true);
        #if DEBUG(2)
        {
            putchar('\n');
            int sectcount = cfg->sectcount;
            for (int secti = 0; secti < sectcount; ++secti) {
                struct cfg_sect* sect = &cfg->sectdata[secti];
                if (*sect->name) printf("[ %s ]\n", sect->name);
                int varcount = sect->varcount;
                for (int vari = 0; vari < varcount; ++ vari) {
                    struct cfg_var* var = &sect->vardata[vari];
                    if (*sect->name) fputs("  ", stdout);
                    printf("%s = %s\n", var->name, var->data);
                }
                putchar('\n');
            }
        }
        #endif
        fclose(f);
        return cfg;
    }
    return cfg_open_new();
}

bool cfg_merge(struct cfg* cfg, const char* p, bool overwrite) {
    int tmp = isFile(p);
    if (tmp < 1) {
        if (tmp) {
            #if DEBUG(1)
            plog(LL_ERROR | LF_DEBUG | LF_FUNC, LE_NOEXIST(p));
            #endif
        } else {
            plog(LL_ERROR | LF_FUNC, LE_ISDIR(p));
        }
        return NULL;
    }
    FILE* f = fopen(p, "r");
    if (!f) {
        int e = errno;
        plog(LL_WARN | LF_FUNC, LE_CANTOPEN(p, e));
        return NULL;
    }
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG | LF_DEBUG, "Reading config (to merge) %s...", p);
    #endif
    cfg_read(cfg, f, overwrite);
    #if DEBUG(2)
    {
        putchar('\n');
        int sectcount = cfg->sectcount;
        for (int secti = 0; secti < sectcount; ++secti) {
            struct cfg_sect* sect = &cfg->sectdata[secti];
            if (*sect->name) printf("[ %s ]\n", sect->name);
            int varcount = sect->varcount;
            for (int vari = 0; vari < varcount; ++ vari) {
                struct cfg_var* var = &sect->vardata[vari];
                if (*sect->name) fputs("  ", stdout);
                printf("%s = %s\n", var->name, var->data);
            }
            putchar('\n');
        }
    }
    #endif
    fclose(f);
    return true;
}

void cfg_close(struct cfg* cfg) {
    int sectcount = cfg->sectcount;
    for (int secti = 0; secti < sectcount; ++secti) {
        struct cfg_sect* sect = &cfg->sectdata[secti];
        if (sect->name) {
            int varcount = sect->varcount;
            for (int vari = 0; vari < varcount; ++ vari) {
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
    destroyMutex(&cfg->lock);
}
