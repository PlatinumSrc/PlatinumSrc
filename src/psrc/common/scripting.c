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
struct compileScript_scope {
    enum scriptopcode* data;
    int index;
    int size;
};
static inline void compileScript_pushscope(struct compileScript_scope* s, int o) {
    ++s->index;
    if (s->index == s->size) {
        s->size *= 2;
        s->data = realloc(s->data, s->size * sizeof(*s->data));
    }
    s->data[s->index] = o;
}
static inline bool compileScript_popscope(struct compileScript_scope* s) {
    if (s->index == -1) return false;
    --s->index;
    return true;
}
struct scriptsz {
    int stringsz;
    int stringlen;
    int opsz;
    int oplen;
    //int opct;
};
static inline void compileScript_addop(struct script* out, struct scriptsz* sz, struct scriptop* op) {
    
}
static inline void compileScript_addstr(struct script* out, struct scriptsz* sz, struct charbuf* cb) {
    
}
bool compileScript(char* p, scriptfunc_t (*findcmd)(char*), struct script* _out, struct charbuf* e) {
    (void)findcmd;
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
    struct script out;
    struct scriptsz sz = {.stringsz = 256, .opsz = 256};
    out.strings = malloc(sz.stringsz);
    out.ops = malloc(sz.opsz);
    struct charbuf cb;
    cb_init(&cb, 256);
    struct compileScript_scope scope = {.index = -1, .size = 4};
    scope.data = malloc(scope.size * sizeof(*scope.data));
    char instr = 0;
    bool reqcmd = false;
    while (1) {
        struct scriptop op;
        int arg = 0;
        int c;
        do {
            c = compiler_fgetc(&f);
        } while (c == '\n' || c == ' ' || c == '\t' || c == ';' || !c);
        if (c == EOF) {
            if (reqcmd) {
                if (e) cb_addstr(e, "Unexpected EOF");
                ret = false;
            }
            goto ret;
        }
        if (c == '|' || c == '&') {
            if (e) cb_addstr(e, "Syntax error");
            ret = false;
            goto ret;
        }
        reqcmd = false;
        while (1) {
            while (1) {
                if (instr == '"') {
                    if (c == EOF) {
                        if (e) cb_addstr(e, "Unexpected EOF");
                        ret = false;
                        goto ret;
                    }
                    if (!arg && !c) {
                        if (e) cb_addstr(e, "Unexpected null char");
                        ret = false;
                        goto ret;
                    }
                    if (c == '"') {
                        instr = 0;
                    } else if (c == '\\') {
                        char tmp[3];
                        int len;
                        c = interpesc(&f, compiler_fgetc(&f), (arg) ? (IESC_NUL | IESC_DOL) : 0, tmp, &len);
                        if (c == -1) {
                            if (e) cb_addstr(e, "Unexpected EOF");
                            ret = false;
                            goto ret;
                        }
                        cb_addpartstr(&cb, tmp, len);
                    } else {
                        cb_add(&cb, c);
                    }
                } else if (instr == '\'') {
                    if (c == EOF) {
                        if (e) cb_addstr(e, "Unexpected EOF");
                        ret = false;
                        goto ret;
                    }
                    if (!arg && !c) {
                        if (e) cb_addstr(e, "Unexpected null char");
                        ret = false;
                        goto ret;
                    }
                    if (c == '\'') {
                        instr = 0;
                    } else {
                        cb_add(&cb, c);
                    }
                } else {
                    switch (c) {
                        case '"':
                            instr = '"';
                            break;
                        case '\'':
                            instr = '\'';
                            break;
                        case ' ':
                        case '\t':
                        case 0:
                            do {
                                c = compiler_fgetc(&f);
                            } while (c == ' ' || c == '\t' || !c);
                            goto longbreak;
                        case '\n':
                        case ';':
                        case '|':
                        case '&':
                        case EOF:
                            goto longbreak;
                        case '\\':
                            c = compiler_fgetc(&f);
                            switch (c) {
                                case EOF:
                                    goto longbreak;
                                case '\\':
                                case '"':
                                case '\'':
                                case ' ':
                                case '\t':
                                case ';':
                                case '|':
                                case '&':
                                    cb_add(&cb, c);
                                    break;
                                case '\n':
                                    break;
                                case '$':
                                    if (arg) {
                                        cb_add(&cb, '$');
                                    } else {
                                        cb_add(&cb, '\\');
                                        cb_add(&cb, '$');
                                    }
                                    break;
                            }
                            break;
                        default:
                            cb_add(&cb, c);
                            break;
                    }
                }
                c = compiler_fgetc(&f);
            }
            longbreak:;
            if (arg) {
                if (arg == 16) {
                    if (e) cb_addstr(e, "Syntax error");
                    ret = false;
                    goto ret;
                }
                printf("ARG %d: {%s}\n", arg, cb_peek(&cb));
            } else {
                printf("CMD: {%s}\n", cb_peek(&cb));
            }
            if (c == '|' || c == '&') {
                reqcmd = true;
                break;
            }
            if (c == '\n' || c == ';') {
                break;
            }
            ++arg;
            cb_clear(&cb);
        }
        cb_clear(&cb);
        puts("ENTER");
        if (c == '|') {
            c = compiler_fgetc(&f);
            if (c == '|') {
                puts("OR");
            } else if (c == EOF) {
                if (e) cb_addstr(e, "Unexpected EOF");
                ret = false;
                goto ret;
            } else {
                puts("PIPE");
                compiler_fungetc(&f);
            }
        } else if (c == '&') {
            c = compiler_fgetc(&f);
            if (c == '&') {
                puts("AND");
            } else if (c == EOF) {
                if (e) cb_addstr(e, "Unexpected EOF");
                ret = false;
                goto ret;
            } else {
                if (e) cb_addstr(e, "Syntax error");
                ret = false;
                goto ret;
            }
        } else if (c == EOF) {
            goto ret;
        }
    }
    ret:;
    if (ret) {
        *_out = out;
    } else {
        if (e) {
            cb_addstr(e, " on line ");
            char tmp[16];
            sprintf(tmp, "%d", f.line);
            cb_addstr(e, tmp);
        }
        free(out.strings);
        free(out.ops);
    }
    free(scope.data);
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
