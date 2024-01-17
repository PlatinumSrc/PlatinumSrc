#include "scripting.h"
#include "crc.h"
#include "filesystem.h"
#include "logging.h"

#include <stdio.h>

struct compilerfile {
    FILE* f;
    int line;
    int prev;
    bool undo;
};
static inline int compiler_fgetc(struct compilerfile* f) {
    int c = f->prev;
    if (f->undo) {
        f->undo = false;
        return c;
    }
    if (c == '\n') ++f->line;
    c = fgetc(f->f);
    f->prev = c;
    return c;
}
static inline void compiler_fungetc(struct compilerfile* f) {
    f->undo = true;
}
static inline bool compiler_fopen(const char* p, struct compilerfile* f) {
    f->f = fopen(p, "r");
    if (!f->f) return false;
    f->line = 1;
    f->prev = EOF;
    return true;
}

#define IESC_DOL (1 << 0)
#define IESC_QUO (1 << 1)
#define IESC_APO (1 << 2)
static inline int gethex(int c) {
    if (c >= 'a' && c <= 'f') c -= 32;
    return (c >= '0' && c <= '9') ? c - 48 : ((c >= 'A' && c <= 'F') ? c - 55 : -1);
}
static inline int interpesc(struct compilerfile* f, int c, uint8_t flags, char* out, int* l) {
    switch (c) {
        case EOF:
            return -1;
        case '0':
            *out++ = '0'; *l = 1; return 1;
        case 'a':
            *out++ = '\a'; *l = 1; return 1;
        case 'b':
            *out++ = '\b'; *l = 1; return 1;
        case 'e':
            *out++ = '\e'; *l = 1; return 1;
        case 'f':
            *out++ = '\f'; *l = 1; return 1;
        case 'n':
            *out++ = '\n'; *l = 1; return 1;
        case 'r':
            *out++ = '\r'; *l = 1; return 1;
        case 't':
            *out++ = '\t'; *l = 1; return 1;
        case 'v':
            *out++ = '\v'; *l = 1; return 1;
        case 'x': {
            int c1 = compiler_fgetc(f);
            if (c1 == EOF) return -1;
            int c2 = compiler_fgetc(f);
            if (c2 == EOF) return -1;
            int h1, h2;
            if ((h1 = gethex(c1)) == -1 || (h2 = gethex(c2)) == -1) {
                *out++ = 'x'; *out++ = c1; *out++ = c2;
                *l = 3;
                return 0;
            }
            *out = (h1 << 4) + h2;
            *l = 1;
            return 1;
        } break;
        case '$': {
            if (flags & IESC_DOL) {
                *out++ = '$'; *l = 1; return 1;
            }
            *out++ = '\\'; *out++ = '$'; *l = 2; return 0;
        } break;
        case '"': {
            if (flags & IESC_QUO) {
                *out++ = '"'; *l = 1; return 1;
            }
            *out++ = '\\'; *out++ = '"'; *l = 2; return 0;
        } break;
        case '\'': {
            if (flags & IESC_APO) {
                *out++ = '\''; *l = 1; return 1;
            }
            *out++ = '\\'; *out++ = '\''; *l = 2; return 0;
        } break;
        case '\n':
            *l = 0; return 1;
    }
    *out++ = c;
    *l = 1;
    return 0;
}
bool compileScript(char* p, scriptfunc_t (*findcmd)(struct charbuf*), struct script* out, char** e) {
    (void)findcmd; (void)out;
    struct compilerfile f;
    {
        int tmp = isFile(p);
        if (tmp < 1) {
            if (tmp) {
                plog(LL_ERROR | LF_FUNC, LE_NOEXIST(p));
                if (e) *e = strdup("Script path does not exist");
            } else {
                plog(LL_ERROR | LF_FUNC, LE_ISDIR(p));
                if (e) *e = strdup("Script path is a directory");
            }
            return false;
        }
        if (!compiler_fopen(p, &f)) {
            plog(LL_WARN | LF_FUNC, LE_CANTOPEN(p, errno));
            if (e) *e = strdup("Failed to open script file");
            return false;
        }
    }
    bool ret = true;
    struct charbuf cb;
    cb_init(&cb, 256);
    //int scope = 0;
    char instr;
    while (1) {
        int tmp;
        do {
            tmp = compiler_fgetc(&f);
        } while (tmp == ' ' || tmp == '\t' || tmp == '\n');
        compiler_fungetc(&f);
        instr = 0;
        while (1) {
            tmp = compiler_fgetc(&f);
            if (tmp == '"') {
                instr = '"';
            } else if (tmp == '\'') {
                instr = '\'';
            } else {
                if (instr == '"') {
                    if (tmp == EOF) {
                        ret = false;
                        if (e) *e = strdup("Unexpected EOF");
                        goto ret;
                    }
                    if (tmp == '"') {
                        instr = 0;
                    } else if (tmp == '\\') {
                        char tmp2[3];
                        int len;
                        tmp = interpesc(&f, tmp, IESC_QUO, tmp2, &len);
                        cb_addpartstr(&cb, tmp2, len);
                    } else {
                        cb_add(&cb, tmp);
                    }
                } else if (instr == '\'') {
                    if (tmp == EOF) {
                        ret = false;
                        if (e) *e = strdup("Unexpected EOF");
                        goto ret;
                    }
                    if (tmp == '\'') {
                        instr = 0;
                    } else if (tmp == '\\') {
                        char tmp2[3];
                        int len;
                        tmp = interpesc(&f, tmp, IESC_APO, tmp2, &len);
                        cb_addpartstr(&cb, tmp2, len);
                    } else {
                        cb_add(&cb, tmp);
                    }
                } else {
                    if (tmp == EOF || tmp == ' ' || tmp == '\n' || tmp == '\t' || !tmp) {
                        break;
                    } else if (tmp == '\\') {
                        if (tmp == '\\') {
                            cb_add(&cb, '\\');
                        } else if (tmp != '\n') {
                            cb_add(&cb, '\\');
                            cb_add(&cb, tmp);
                        }
                    } else {
                        cb_add(&cb, tmp);
                    }
                }
            }
        }
        compiler_fungetc(&f);
        do {
            tmp = compiler_fgetc(&f);
        } while (tmp == ' ' || tmp == '\t');
        if (tmp == '\n' || !tmp) goto nextcmd;
        if (tmp == EOF) goto ret;
        compiler_fungetc(&f);
        while (1) {
            instr = 0;
            while (1) {
                tmp = compiler_fgetc(&f);
                if (tmp == '"') {
                    instr = '"';
                } else if (tmp == '\'') {
                    instr = '\'';
                } else {
                    if (instr == '"') {
                        if (tmp == EOF) {
                            ret = false;
                            if (e) *e = strdup("Unexpected EOF");
                            goto ret;
                        }
                        if (tmp == '"') {
                            instr = 0;
                        } else if (tmp == '\\') {
                            char tmp2[3];
                            int len;
                            tmp = interpesc(&f, tmp, IESC_QUO, tmp2, &len);
                            cb_addpartstr(&cb, tmp2, len);
                        } else {
                            cb_add(&cb, tmp);
                        }
                    } else if (instr == '\'') {
                        if (tmp == EOF) {
                            ret = false;
                            if (e) *e = strdup("Unexpected EOF");
                            goto ret;
                        }
                        if (tmp == '\'') {
                            instr = 0;
                        } else if (tmp == '\\') {
                            char tmp2[3];
                            int len;
                            tmp = interpesc(&f, tmp, IESC_APO, tmp2, &len);
                            cb_addpartstr(&cb, tmp2, len);
                        } else {
                            cb_add(&cb, tmp);
                        }
                    } else {
                        if (tmp == EOF || tmp == ' ' || tmp == '\n' || tmp == '\t' || !tmp) {
                            break;
                        } else if (tmp == '\\') {
                            if (tmp == '\\') {
                                cb_add(&cb, '\\');
                            } else if (tmp != '\n') {
                                cb_add(&cb, '\\');
                                cb_add(&cb, tmp);
                            }
                        } else {
                            cb_add(&cb, tmp);
                        }
                    }
                }
            }
            //nextarg:;
            do {
                tmp = compiler_fgetc(&f);
            } while (tmp == ' ' || tmp == '\t');
            if (tmp == '\n' || !tmp) goto nextcmd;
            if (tmp == EOF) goto ret;
        }
        nextcmd:;
        cb_clear(&cb);
    }
    ret:;
    cb_dump(&cb);
    return ret;
}

bool createScriptEventTable(struct scripteventtable* t, int s) {
    if (!(t->data = calloc(s, sizeof(*t->data)))) return false;
    t->len = 0;
    t->size = s;
    return true;
}

void destroyScriptEventTable(struct scripteventtable* t) {
    free(t->data);
}

void fireScriptEvent(struct scripteventtable* t, char* name, int argc, struct charbuf* argv) {
    (void)t; (void)name; (void)argc; (void)argv;
    //uint32_t namecrc = strcrc32(name);
}
