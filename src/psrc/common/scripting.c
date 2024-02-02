#include "scripting.h"
#include "crc.h"
#include "filesystem.h"
#include "logging.h"

#include <stdio.h>
#include <limits.h>

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
static inline bool isvarchar(int c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9'));
}
struct compileScript_scope {
    int16_t* data;
    int index;
    int size;
};
static inline void compileScript_pushscope(struct compileScript_scope* s, enum scriptopcode o, uint8_t f) {
    ++s->index;
    if (s->index == s->size) {
        s->size *= 2;
        s->data = realloc(s->data, s->size * sizeof(*s->data));
    }
    s->data[s->index] = (o & 0xFF) | (f << 8);
}
static inline bool compileScript_chscope(struct compileScript_scope* s, enum scriptopcode o, uint8_t f) {
    if (s->index == -1) return false;
    s->data[s->index] = (o & 0xFF) | (f << 8);
    return true;
}
static inline bool compileScript_popscope(struct compileScript_scope* s) {
    if (s->index == -1) return false;
    --s->index;
    return true;
}
static inline enum scriptopcode compileScript_getscope(struct compileScript_scope* s, uint8_t* f) {
    if (s->index == -1) return SCRIPTOPCODE__INVAL;
    if (f) *f = (s->data[s->index] & 0xFF00) >> 8;
    return s->data[s->index] & 0xFF;
}
#define SCOPEFLAG_EXEC (1 << 0)
struct scriptsz {
    int opsz;
    int oplen;
    //int opct;
};
static inline void compileScript_addop(struct script* out, struct scriptsz* sz, struct scriptop* op) {
    switch (op->opcode) {
        case SCRIPTOPCODE_TRUE: puts("true"); break;
        case SCRIPTOPCODE_FALSE: puts("false"); break;
        case SCRIPTOPCODE_AND: puts("and"); break;
        case SCRIPTOPCODE_OR: puts("or"); break;
        case SCRIPTOPCODE_IF: puts("if"); break;
        case SCRIPTOPCODE_ELIF: puts("elif"); break;
        case SCRIPTOPCODE_ELSE: puts("else"); break;
        case SCRIPTOPCODE_WHILE: puts("while"); break;
        case SCRIPTOPCODE_TESTBLOCK: puts("testblock"); break;
        case SCRIPTOPCODE_END: puts("end"); break;
        case SCRIPTOPCODE_PIPE: puts("pipe"); break;
        case SCRIPTOPCODE_ADD: printf("add [%d, %d]\n", op->data.add.data.offset, op->data.add.data.len); break;
        case SCRIPTOPCODE_PUSH: puts("push"); break;
        default: printf("! %d\n", op->opcode); break;
    }
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
    struct scriptsz sz = {.opsz = 256};
    struct script out = {.ops = malloc(sz.opsz)};
    struct charbuf strings;
    cb_init(&strings, 256);
    struct charbuf cb;
    cb_init(&cb, 256);
    struct compileScript_scope scope = {.index = -1, .size = 4};
    scope.data = malloc(scope.size * sizeof(*scope.data));
    char instr = 0;
    bool reqcmd = false;
    while (1) {
        struct scriptop op;
        struct scriptop tmpop;
        int arg = 0;
        int c;
        do {
            c = compiler_fgetc(&f);
        } while (c == '\n' || c == ' ' || c == '\t' || c == ';' || !c);
        if (c == EOF) {
            if (reqcmd) {
                if (e) cb_addstr(e, "Unexpected EOF");
                ret = false;
                goto ret;
            }
            break;
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
                    } else if (c == '$') {
                        if (arg) {
                            int len = cb.len;
                            bool array = false;
                            int start = 0, end = INT_MAX;
                            c = compiler_fgetc(&f);
                            if (c >= '0' && c <= '9') {
                                do {
                                    cb_add(&cb, c);
                                    c = compiler_fgetc(&f);
                                } while (c >= '0' && c <= '9');
                                compiler_fungetc(&f);
                            } else if (c == '@' || c == '?') {
                                cb_add(&cb, c);
                            } else if (c == '{') {
                                c = compiler_fgetc(&f);
                                if (c == EOF) {
                                    if (e) cb_addstr(e, "Unexpected EOF");
                                    ret = false;
                                    goto ret;
                                }
                                if (c == '@') {
                                    cb_add(&cb, '@');
                                    c = compiler_fgetc(&f);
                                } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
                                    do {
                                        cb_add(&cb, c);
                                        c = compiler_fgetc(&f);
                                    } while (isvarchar(c));
                                } else {
                                    if (e) cb_addstr(e, "Syntax error");
                                    ret = false;
                                    goto ret;
                                }
                                if (c == EOF) {
                                    if (e) cb_addstr(e, "Unexpected EOF");
                                    ret = false;
                                    goto ret;
                                }
                                if (c == ':') {
                                    int8_t negative = -1;
                                    while (1) {
                                        c = compiler_fgetc(&f);
                                        if (c >= '0' && c <= '9') {
                                            if (negative == -1) {
                                                negative = 0;
                                                start = c - '0';
                                            } else {
                                                start *= 10;
                                                start += c - '0';
                                            }
                                        } else if (c == '-') {
                                            if (negative != -1) {
                                                if (e) cb_addstr(e, "Syntax error");
                                                ret = false;
                                                goto ret;
                                            }
                                            negative = 1;
                                            start = 0;
                                        } else {
                                            break;
                                        }
                                    }
                                    if (negative) start = -start;
                                    if (c == EOF) {
                                        if (e) cb_addstr(e, "Unexpected EOF");
                                        ret = false;
                                        goto ret;
                                    }
                                    if (c == ',') {
                                        negative = -1;
                                        while (1) {
                                            c = compiler_fgetc(&f);
                                            if (c >= '0' && c <= '9') {
                                                if (negative == -1) {
                                                    negative = 0;
                                                    end = c - '0';
                                                } else {
                                                    end *= 10;
                                                    end += c - '0';
                                                }
                                            } else if (c == '-') {
                                                if (negative != -1) {
                                                    if (e) cb_addstr(e, "Syntax error");
                                                    ret = false;
                                                    goto ret;
                                                }
                                                negative = 1;
                                                end = 0;
                                            } else {
                                                break;
                                            }
                                        }
                                        if (negative) end = -end;
                                        if (c == EOF) {
                                            if (e) cb_addstr(e, "Unexpected EOF");
                                            ret = false;
                                            goto ret;
                                        }
                                        if (c != '}') {
                                            if (e) cb_addstr(e, "Syntax error");
                                            ret = false;
                                            goto ret;
                                        }
                                    } else if (c != '}') {
                                        if (e) cb_addstr(e, "Syntax error");
                                        ret = false;
                                        goto ret;
                                    }
                                } else if (c != '}') {
                                    if (e) cb_addstr(e, "Syntax error");
                                    ret = false;
                                    goto ret;
                                }
                                array = true;
                            } else if (c == EOF) {
                                cb_add(&cb, '$');
                                goto longbreak;
                            } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
                                do {
                                    cb_add(&cb, c);
                                    c = compiler_fgetc(&f);
                                } while (isvarchar(c));
                                compiler_fungetc(&f);
                            } else {
                                cb_add(&cb, '$');
                                continue;
                            }
                            if (len > 0) {
                                tmpop.opcode = SCRIPTOPCODE_ADD;
                                tmpop.data.add.data.offset = strings.len;
                                tmpop.data.add.data.len = len;
                                cb_addpartstr(&strings, cb.data, len);
                                compileScript_addop(&out, &sz, &tmpop);
                            }
                            if (array) {
                                tmpop.opcode = SCRIPTOPCODE_READARRAY;
                                tmpop.data.readarray.name.offset = strings.len;
                                tmpop.data.readarray.name.len = cb.len - len;
                                cb_addpartstr(&strings, &cb.data[len], cb.len - len);
                                tmpop.data.readarray.namecrc = crc32((uint8_t*)&cb.data[len], cb.len - len);
                                tmpop.data.readarray.from = start;
                                tmpop.data.readarray.to = end;
                                compileScript_addop(&out, &sz, &tmpop);
                            } else {
                                tmpop.opcode = SCRIPTOPCODE_READVAR;
                                tmpop.data.readvar.name.offset = strings.len;
                                tmpop.data.readvar.name.len = cb.len - len;
                                cb_addpartstr(&strings, &cb.data[len], cb.len - len);
                                tmpop.data.readvar.namecrc = crc32((uint8_t*)&cb.data[len], cb.len - len);
                                compileScript_addop(&out, &sz, &tmpop);
                            }
                            cb_clear(&cb);
                        } else {
                            cb_add(&cb, '$');
                        }
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
                                    cb_add(&cb, '\\');
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
                        case '$':
                            if (arg) {
                                int len = cb.len;
                                bool array = false;
                                int start = 0, end = INT_MAX;
                                c = compiler_fgetc(&f);
                                if (c >= '0' && c <= '9') {
                                    do {
                                        cb_add(&cb, c);
                                        c = compiler_fgetc(&f);
                                    } while (c >= '0' && c <= '9');
                                    compiler_fungetc(&f);
                                } else if (c == '@' || c == '?') {
                                    cb_add(&cb, c);
                                } else if (c == '{') {
                                    c = compiler_fgetc(&f);
                                    if (c == EOF) {
                                        if (e) cb_addstr(e, "Unexpected EOF");
                                        ret = false;
                                        goto ret;
                                    }
                                    if (c == '@') {
                                        cb_add(&cb, '@');
                                        c = compiler_fgetc(&f);
                                    } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
                                        do {
                                            cb_add(&cb, c);
                                            c = compiler_fgetc(&f);
                                        } while (isvarchar(c));
                                    } else {
                                        if (e) cb_addstr(e, "Syntax error");
                                        ret = false;
                                        goto ret;
                                    }
                                    if (c == EOF) {
                                        if (e) cb_addstr(e, "Unexpected EOF");
                                        ret = false;
                                        goto ret;
                                    }
                                    if (c == ':') {
                                        int8_t negative = -1;
                                        while (1) {
                                            c = compiler_fgetc(&f);
                                            if (c >= '0' && c <= '9') {
                                                if (negative == -1) {
                                                    negative = 0;
                                                    start = c - '0';
                                                } else {
                                                    start *= 10;
                                                    start += c - '0';
                                                }
                                            } else if (c == '-') {
                                                if (negative != -1) {
                                                    if (e) cb_addstr(e, "Syntax error");
                                                    ret = false;
                                                    goto ret;
                                                }
                                                negative = 1;
                                                start = 0;
                                            } else {
                                                break;
                                            }
                                        }
                                        if (negative) start = -start;
                                        if (c == EOF) {
                                            if (e) cb_addstr(e, "Unexpected EOF");
                                            ret = false;
                                            goto ret;
                                        }
                                        if (c == ',') {
                                            negative = -1;
                                            while (1) {
                                                c = compiler_fgetc(&f);
                                                if (c >= '0' && c <= '9') {
                                                    if (negative == -1) {
                                                        negative = 0;
                                                        end = c - '0';
                                                    } else {
                                                        end *= 10;
                                                        end += c - '0';
                                                    }
                                                } else if (c == '-') {
                                                    if (negative != -1) {
                                                        if (e) cb_addstr(e, "Syntax error");
                                                        ret = false;
                                                        goto ret;
                                                    }
                                                    negative = 1;
                                                    end = 0;
                                                } else {
                                                    break;
                                                }
                                            }
                                            if (negative) end = -end;
                                            if (c == EOF) {
                                                if (e) cb_addstr(e, "Unexpected EOF");
                                                ret = false;
                                                goto ret;
                                            }
                                            if (c != '}') {
                                                if (e) cb_addstr(e, "Syntax error");
                                                ret = false;
                                                goto ret;
                                            }
                                        } else if (c != '}') {
                                            if (e) cb_addstr(e, "Syntax error");
                                            ret = false;
                                            goto ret;
                                        }
                                    } else if (c != '}') {
                                        if (e) cb_addstr(e, "Syntax error");
                                        ret = false;
                                        goto ret;
                                    }
                                    array = true;
                                } else if (c == EOF) {
                                    cb_add(&cb, '$');
                                    goto longbreak;
                                } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
                                    do {
                                        cb_add(&cb, c);
                                        c = compiler_fgetc(&f);
                                    } while (isvarchar(c));
                                    compiler_fungetc(&f);
                                } else {
                                    cb_add(&cb, '$');
                                    continue;
                                }
                                if (len > 0) {
                                    tmpop.opcode = SCRIPTOPCODE_ADD;
                                    tmpop.data.add.data.offset = strings.len;
                                    tmpop.data.add.data.len = len;
                                    cb_addpartstr(&strings, cb.data, len);
                                    compileScript_addop(&out, &sz, &tmpop);
                                }
                                if (array) {
                                    tmpop.opcode = SCRIPTOPCODE_READARRAYSEP;
                                    tmpop.data.readarray.name.offset = strings.len;
                                    tmpop.data.readarray.name.len = cb.len - len;
                                    cb_addpartstr(&strings, &cb.data[len], cb.len - len);
                                    tmpop.data.readarray.namecrc = crc32((uint8_t*)&cb.data[len], cb.len - len);
                                    tmpop.data.readarray.from = start;
                                    tmpop.data.readarray.to = end;
                                    compileScript_addop(&out, &sz, &tmpop);
                                } else {
                                    tmpop.opcode = SCRIPTOPCODE_READVARSEP;
                                    tmpop.data.readvar.name.offset = strings.len;
                                    tmpop.data.readvar.name.len = cb.len - len;
                                    cb_addpartstr(&strings, &cb.data[len], cb.len - len);
                                    tmpop.data.readvar.namecrc = crc32((uint8_t*)&cb.data[len], cb.len - len);
                                    compileScript_addop(&out, &sz, &tmpop);
                                }
                                cb_clear(&cb);
                            } else {
                                cb_add(&cb, '$');
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
                if (cb.len > 0) {
                    tmpop.opcode = SCRIPTOPCODE_ADD;
                    tmpop.data.add.data.offset = strings.len;
                    tmpop.data.add.data.len = cb.len;
                    cb_addpartstr(&strings, cb.data, cb.len);
                    compileScript_addop(&out, &sz, &tmpop);
                }
                tmpop.opcode = SCRIPTOPCODE_PUSH;
                compileScript_addop(&out, &sz, &tmpop);
            } else {
                cb_nullterm(&cb);
                //printf("CMD: {%s}\n", cb.data);
                if (!strcmp(cb.data, ":") || !strcmp(cb.data, "true")) {
                    op.opcode = SCRIPTOPCODE_TRUE;
                } else if (!strcmp(cb.data, "false")) {
                    op.opcode = SCRIPTOPCODE_FALSE;
                } else if (!strcmp(cb.data, "if")) {
                    compileScript_pushscope(&scope, SCRIPTOPCODE_IF, 0);
                    tmpop.opcode = SCRIPTOPCODE_IF;
                    compileScript_addop(&out, &sz, &tmpop);
                    reqcmd = true;
                    goto findcmd;
                } else if (!strcmp(cb.data, "elif")) {
                    uint8_t f;
                    if (compileScript_getscope(&scope, &f) != SCRIPTOPCODE_IF) {
                        if (e) cb_addstr(e, "Misplaced 'elif'");
                        ret = false;
                        goto ret;
                    }
                    if (!(f & SCOPEFLAG_EXEC)) {
                        if (e) cb_addstr(e, "Missing 'then' before 'elif'");
                        ret = false;
                        goto ret;
                    }
                    compileScript_chscope(&scope, SCRIPTOPCODE_ELIF, 0);
                    tmpop.opcode = SCRIPTOPCODE_ELIF;
                    compileScript_addop(&out, &sz, &tmpop);
                    reqcmd = true;
                    goto findcmd;
                } else if (!strcmp(cb.data, "else")) {
                    uint8_t f;
                    enum scriptopcode tmp = compileScript_getscope(&scope, &f);
                    if (tmp != SCRIPTOPCODE_IF && tmp != SCRIPTOPCODE_ELIF) {
                        if (e) cb_addstr(e, "Misplaced 'else'");
                        ret = false;
                        goto ret;
                    }
                    if (!(f & SCOPEFLAG_EXEC)) {
                        if (e) cb_addstr(e, "Missing 'then' before 'else'");
                        ret = false;
                        goto ret;
                    }
                    compileScript_chscope(&scope, SCRIPTOPCODE_ELSE, SCOPEFLAG_EXEC);
                    op.opcode = SCRIPTOPCODE_ELSE;
                } else if (!strcmp(cb.data, "while")) {
                    compileScript_pushscope(&scope, SCRIPTOPCODE_WHILE, 0);
                    tmpop.opcode = SCRIPTOPCODE_WHILE;
                    compileScript_addop(&out, &sz, &tmpop);
                    reqcmd = true;
                    goto findcmd;
                } else if (!strcmp(cb.data, "then")) {
                    uint8_t f;
                    enum scriptopcode tmp = compileScript_getscope(&scope, &f);
                    if ((tmp != SCRIPTOPCODE_IF && tmp != SCRIPTOPCODE_ELIF) || (f & SCOPEFLAG_EXEC)) {
                        if (e) cb_addstr(e, "Misplaced 'then'");
                        ret = false;
                        goto ret;
                    }
                    compileScript_chscope(&scope, tmp, f | SCOPEFLAG_EXEC);
                    tmpop.opcode = SCRIPTOPCODE_TESTBLOCK;
                    compileScript_addop(&out, &sz, &tmpop);
                    reqcmd = true;
                    goto findcmd;
                } else if (!strcmp(cb.data, "do")) {
                    uint8_t f;
                    enum scriptopcode tmp = compileScript_getscope(&scope, &f);
                    if (tmp != SCRIPTOPCODE_WHILE || (f & SCOPEFLAG_EXEC)) {
                        if (e) cb_addstr(e, "Misplaced 'do'");
                        ret = false;
                        goto ret;
                    }
                    compileScript_chscope(&scope, tmp, f | SCOPEFLAG_EXEC);
                    tmpop.opcode = SCRIPTOPCODE_TESTBLOCK;
                    compileScript_addop(&out, &sz, &tmpop);
                    reqcmd = true;
                    goto findcmd;
                } else if (!strcmp(cb.data, "end")) {
                    uint8_t f;
                    enum scriptopcode tmp = compileScript_getscope(&scope, &f);
                    if (tmp == SCRIPTOPCODE_IF || tmp == SCRIPTOPCODE_ELIF) {
                        if (!(f & SCOPEFLAG_EXEC)) {
                            if (e) cb_addstr(e, "Missing 'then' before 'end'");
                            ret = false;
                            goto ret;
                        }
                    } else if (tmp == SCRIPTOPCODE_WHILE) {
                        if (!(f & SCOPEFLAG_EXEC)) {
                            if (e) cb_addstr(e, "Missing 'do' before 'end'");
                            ret = false;
                            goto ret;
                        }
                    }
                    if (!compileScript_popscope(&scope)) {
                        if (e) cb_addstr(e, "Misplaced 'end'");
                        ret = false;
                        goto ret;
                    }
                    op.opcode = SCRIPTOPCODE_END;
                }
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
        compileScript_addop(&out, &sz, &op);
        if (c == '|') {
            c = compiler_fgetc(&f);
            if (c == '|') {
                tmpop.opcode = SCRIPTOPCODE_OR;
                compileScript_addop(&out, &sz, &tmpop);
            } else if (c == EOF) {
                if (e) cb_addstr(e, "Unexpected EOF");
                ret = false;
                goto ret;
            } else {
                tmpop.opcode = SCRIPTOPCODE_PIPE;
                compileScript_addop(&out, &sz, &tmpop);
                compiler_fungetc(&f);
            }
        } else if (c == '&') {
            c = compiler_fgetc(&f);
            if (c == '&') {
                tmpop.opcode = SCRIPTOPCODE_AND;
                compileScript_addop(&out, &sz, &tmpop);
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
            break;
        }
        continue;
        findcmd:;
        cb_clear(&cb);
        compiler_fungetc(&f);
    }
    if (scope.index != -1) {
        if (e) cb_addstr(e, "Missing 'end'");
        ret = false;
        goto ret;
    }
    ret:;
    if (ret) {
        _out->ops = out.ops;
        _out->strings = cb_finalize(&strings);
    } else {
        if (e) {
            cb_addstr(e, " on line ");
            char tmp[16];
            sprintf(tmp, "%d", f.line);
            cb_addstr(e, tmp);
        }
        cb_dump(&strings);
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
