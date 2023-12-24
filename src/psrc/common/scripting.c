#include "scripting.h"
#include "../utils/crc.h"
#include "../utils/filesystem.h"
#include "../utils/logging.h"

#include <stdio.h>

struct compilerfile {
    FILE* f;
    int line;
    int prevc;
};
static inline int compiler_fgetc(struct compilerfile* f) {
    int c = f->prevc;
    if (c == '\n') ++f->line;
    c = fgetc(f->f);
    f->prevc = c;
    return c;
}
static inline bool compiler_fopen(const char* p, struct compilerfile* f) {
    f->f = fopen(p, "r");
    if (!f->f) return false;
    f->line = 1;
    f->prevc = EOF;
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
static inline bool compiler_getcmd(struct compilerfile* f, struct charbuf* out) {
    char inStr = 0;
    while (1) {
        int c = compiler_fgetc(f);
        if (c == '"') {
            inStr = '"';
        } else if (c == '\'') {
            inStr = '\'';
        } else {
            if (inStr == '"') {
                if (c == EOF) {
                    return false;
                }
                if (c == '"') {
                    inStr = 0;
                } else if (c == '\\') {
                    char tmp[3];
                    int len;
                    c = interpesc(f, c, IESC_QUO, tmp, &len);
                    cb_addpartstr(out, tmp, len);
                } else {
                    cb_add(out, c);
                }
            } else if (inStr == '\'') {
                if (c == EOF) {
                    return false;
                }
                if (c == '\'') {
                    inStr = 0;
                } else if (c == '\\') {
                    char tmp[3];
                    int len;
                    c = interpesc(f, c, IESC_APO, tmp, &len);
                    cb_addpartstr(out, tmp, len);
                } else {
                    cb_add(out, c);
                }
            } else {
                if (c == EOF || c == ' ' || c == '\n' || c == '\t' || !c) {
                    return true;
                } else if (c == '\\') {
                    if (c == '\\') {
                        cb_add(out, '\\');
                    } else if (c != '\n') {
                        cb_add(out, '\\');
                        cb_add(out, c);
                    }
                } else {
                    cb_add(out, c);
                }
            }
        }
    }
}
bool compileScript(char* p, scriptfunc_t (*findcmd)(struct charbuf*), struct script* out, char** e) {
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
    struct charbuf cmd;
    struct charbuf args;
    cb_init(&cmd, 32);
    cb_init(&args, 256);
    while (1) {
        cb_clear(&cmd);
        //if (!compiler_getcmd(f, &cmd);
        //if (tmp == -1)
    }
    cb_dump(&cmd);
    cb_dump(&args);
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
    uint32_t namecrc = strcrc32(name);
    
}
