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

#define IESC_NUL (1 << 0)
#define IESC_DOL (1 << 1)
static inline int gethex(int c) {
    if (c >= 'a' && c <= 'f') c -= 32;
    return (c >= '0' && c <= '9') ? c - 48 : ((c >= 'A' && c <= 'F') ? c - 55 : -1);
}
static inline int interpesc(struct compilerfile* f, int c, uint8_t flags, char* out, int* l) {
    switch (c) {
        case EOF:
            return -1;
        case '0':
            if (flags & IESC_NUL) {
                *out++ = '0'; *l = 1; return 1;
            }
            *out++ = '\\'; *out++ = '0'; *l = 2; return 0;
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
        case '$':
            if (flags & IESC_DOL) {
                *out++ = '$'; *l = 1; return 1;
            }
            *out++ = '\\'; *out++ = '$'; *l = 2; return 0;
        case '"':
            *out++ = '"'; *l = 1; return 1;
        case '\n':
            *l = 0; return 1;
    }
    *out++ = c;
    *l = 1;
    return 0;
}
static inline int getstr(struct compilerfile* f, int flags, struct charbuf* cb, struct charbuf* e) {
    int instr = 0;
    while (1) {
        int tmp = compiler_fgetc(f);
        if (instr == '"') {
            if (tmp == EOF) {
                if (e) cb_addstr(e, "Unexpected EOF");
                return -1;
            }
            if (!(flags & IESC_NUL) && !tmp) {
                if (e) cb_addstr(e, "Unexpected null char");
                return -1;
            }
            if (tmp == '"') {
                instr = 0;
            } else if (tmp == '\\') {
                char tmp2[3];
                int len;
                tmp = interpesc(f, tmp, flags, tmp2, &len);
                if (tmp == -1) {
                    if (e) cb_addstr(e, "Unexpected EOF");
                    return -1;
                }
                cb_addpartstr(cb, tmp2, len);
            } else {
                cb_add(cb, tmp);
            }
        } else if (instr == '\'') {
            if (tmp == EOF) {
                if (e) cb_addstr(e, "Unexpected EOF");
                return -1;
            }
            if (!(flags & IESC_NUL) && !tmp) {
                if (e) cb_addstr(e, "Unexpected null char");
                return -1;
            }
            if (tmp == '\'') {
                instr = 0;
            } else {
                cb_add(cb, tmp);
            }
        } else {
            if (tmp == '"') {
                instr = '"';
            } else if (tmp == '\'') {
                instr = '\'';
            } else if (tmp == ' ' || tmp == '\t' || !tmp) {
                return 1;
            } else if (tmp == '\n' || tmp == ';' || tmp == '|' || tmp == '&' || tmp == EOF) {
                return 0;
            } else if (tmp == '\\') {
                tmp = compiler_fgetc(f);
                if (tmp == EOF) {
                    if (e) cb_addstr(e, "Unexpected EOF");
                    return -1;
                } else if (tmp == '\\') {
                    cb_add(cb, '\\');
                } else if (tmp == '"') {
                    cb_add(cb, '"');
                } else if (tmp == '\'') {
                    cb_add(cb, '\'');
                } else if (tmp != '\n') {
                    cb_add(cb, '\\');
                    cb_add(cb, tmp);
                }
            } else {
                cb_add(cb, tmp);
            }
        }
    }
}
bool compileScript(char* p, scriptfunc_t (*findcmd)(char*), struct script* out, struct charbuf* e) {
    (void)findcmd; (void)out;
    struct compilerfile f;
    {
        int tmp = isFile(p);
        if (tmp < 1) {
            if (tmp) {
                plog(LL_ERROR | LF_FUNC, LE_NOEXIST(p));
                if (e) cb_addstr(e, "Script path does not exist");
            } else {
                plog(LL_ERROR | LF_FUNC, LE_ISDIR(p));
                if (e) cb_addstr(e, "Script path is a directory");
            }
            return false;
        }
        if (!compiler_fopen(p, &f)) {
            plog(LL_WARN | LF_FUNC, LE_CANTOPEN(p, errno));
            if (e) cb_addstr(e, "Failed to open script file");
            return false;
        }
    }
    bool ret = true;
    struct charbuf cb;
    cb_init(&cb, 256);
    // TODO: decrustify
    //int scope = 0;
    bool needcmd = false;
    while (1) {
        int tmp;
        do {
            tmp = compiler_fgetc(&f);
        } while (tmp == ' ' || tmp == '\t' || tmp == '\n');
        if (tmp == EOF) {
            if (needcmd) {
                if (e) cb_addstr(e, "Expected a command");
                ret = false;
                goto ret;
            }
            goto ret;
        }
        compiler_fungetc(&f);
        tmp = getstr(&f, 0, &cb, e);
        if (tmp == -1) {
            ret = false;
            goto ret;
        }
        printf("CMD: {%s}\n", cb_peek(&cb));
        if (tmp == 1) {
            compiler_fungetc(&f);
            do {
                tmp = compiler_fgetc(&f);
            } while (tmp == ' ' || tmp == '\t');
            if (tmp == '\n' || tmp == ';' || tmp == '|' || tmp == '&' || tmp == EOF) goto nextcmd;
            compiler_fungetc(&f);
            cb_clear(&cb);
            while (1) {
                tmp = getstr(&f, IESC_NUL | IESC_DOL, &cb, e);
                if (tmp == -1) {
                    ret = false;
                    goto ret;
                }
                printf("ARG: {%s}\n", cb_peek(&cb));
                compiler_fungetc(&f);
                do {
                    tmp = compiler_fgetc(&f);
                } while (tmp == ' ' || tmp == '\t');
                if (tmp == '\n' || tmp == ';' || tmp == '|' || tmp == '&' || tmp == EOF) goto nextcmd;
                compiler_fungetc(&f);
                cb_clear(&cb);
            }
        } else {
            if (needcmd) {
                if (e) cb_addstr(e, "Expected a command");
                ret = false;
                goto ret;
            }
            goto nextcmd;
        }
        compiler_fungetc(&f);
        nextcmd:;
        if (tmp == '|') {
            if (needcmd) {
                if (e) cb_addstr(e, "Expected a command");
                ret = false;
                goto ret;
            }
            tmp = compiler_fgetc(&f);
            if (tmp == '|') {
                puts("OR");
            } else {
                puts("PIPE");
                compiler_fungetc(&f);
            }
            needcmd = true;
        } else if (tmp == '&') {
            if (needcmd) {
                if (e) cb_addstr(e, "Expected a command");
                ret = false;
                goto ret;
            }
            tmp = compiler_fgetc(&f);
            if (tmp != '&') {
                if (e) cb_addstr(e, "Syntax error");
                ret = false;
                goto ret;
            }
            needcmd = true;
        } else {
            needcmd = false;
        }
        puts("---");
        if (tmp == EOF) goto ret;
        cb_clear(&cb);
    }
    ret:;
    cb_dump(&cb);
    return ret;
}

void cleanUpScript(struct script* s) {
    free(s->strings);
    free(s->ops);
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
