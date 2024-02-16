#include "scripting.h"
#include "crc.h"
#include "filesystem.h"
#include "logging.h"
#include "time.h"

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
    //printf("push %d\n", o);
    ++s->index;
    if (s->index == s->size) {
        s->size *= 2;
        s->data = realloc(s->data, s->size * sizeof(*s->data));
    }
    s->data[s->index] = (o & 0xFF) | (f << 8);
}
static inline bool compileScript_chscope(struct compileScript_scope* s, enum scriptopcode o, uint8_t f) {
    //printf("ch %d\n", o);
    if (s->index == -1) return false;
    s->data[s->index] = (o & 0xFF) | (f << 8);
    return true;
}
static inline bool compileScript_popscope(struct compileScript_scope* s) {
    //puts("pop");
    if (s->index == -1) return false;
    --s->index;
    return true;
}
static inline enum scriptopcode compileScript_getscope(struct compileScript_scope* s, uint8_t* f) {
    if (s->index == -1) return SCRIPTOPCODE__INVAL;
    if (f) *f = (s->data[s->index] & 0xFF00) >> 8;
    //printf("get %d\n", s->data[s->index] & 0xFF);
    return s->data[s->index] & 0xFF;
}
#define SCOPEFLAG_EXEC (1 << 0)
#define SCOPEFLAG_PUTEND (1 << 1)
struct scriptsz {
    int opsz;
    int oplen;
    //int opct;
};
static inline void compileScript_addop(struct script* out, struct scriptsz* sz, struct scriptop* op) {
    int len = sizeof(op->opcode);
    switch (op->opcode) {
        case SCRIPTOPCODE_ADD:
            len += sizeof(op->data.add);
            break;
        case SCRIPTOPCODE_READVAR:
        case SCRIPTOPCODE_READVARSEP:
            len += sizeof(op->data.readvar);
            break;
        case SCRIPTOPCODE_READARRAY:
        case SCRIPTOPCODE_READARRAYSEP:
            len += sizeof(op->data.readarray);
            break;
        case SCRIPTOPCODE_CMD:
            len += sizeof(op->data.cmd);
            break;
        default: break;
    }
    if (sz->oplen + len >= sz->opsz) {
        do {
            sz->opsz *= 2;
        } while (sz->oplen + len >= sz->opsz);
        out->ops = realloc(out->ops, sz->opsz);
    }
    memcpy(((void*)out->ops) + sz->oplen, op, len);
    sz->oplen += len;
    //printf("len: %d, oplen: %d, opsz: %d\n", len, sz->oplen, sz->opsz);
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
    bool putend = false;
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
        if (c == '|' || c == '&' || c == ')') {
            if (e) cb_addstr(e, "Syntax error");
            ret = false;
            goto ret;
        }
        if (c == '#') {
            do {
                c = compiler_fgetc(&f);
            } while (c != '\n' && c != EOF);
            continue;
        } else if (c == '(') {
            compileScript_pushscope(&scope, SCRIPTOPCODE_GROUP, 0);
            tmpop.opcode = SCRIPTOPCODE_GROUP;
            compileScript_addop(&out, &sz, &tmpop);
            continue;
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
                        case '(':
                        case ')':
                        case '#':
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
                } else if (!strcmp(cb.data, "continue")) {
                    uint8_t f;
                    if (compileScript_getscope(&scope, &f) != SCRIPTOPCODE_WHILE) {
                        if (e) cb_addstr(e, "Misplaced 'continue'");
                        ret = false;
                        goto ret;
                    }
                    if (!(f & SCOPEFLAG_EXEC)) {
                        if (e) cb_addstr(e, "Missing 'do' before 'continue'");
                        ret = false;
                        goto ret;
                    }
                    op.opcode = SCRIPTOPCODE_CONT;
                } else if (!strcmp(cb.data, "break")) {
                    uint8_t f;
                    if (compileScript_getscope(&scope, &f) != SCRIPTOPCODE_WHILE) {
                        if (e) cb_addstr(e, "Misplaced 'break'");
                        ret = false;
                        goto ret;
                    }
                    if (!(f & SCOPEFLAG_EXEC)) {
                        if (e) cb_addstr(e, "Missing 'do' before 'break'");
                        ret = false;
                        goto ret;
                    }
                    op.opcode = SCRIPTOPCODE_BREAK;
                } else if (!strcmp(cb.data, "then")) {
                    uint8_t f;
                    enum scriptopcode tmp = compileScript_getscope(&scope, &f);
                    if ((tmp != SCRIPTOPCODE_IF && tmp != SCRIPTOPCODE_ELIF) || (f & SCOPEFLAG_EXEC)) {
                        if (e) cb_addstr(e, "Misplaced 'then'");
                        ret = false;
                        goto ret;
                    }
                    compileScript_chscope(&scope, tmp, f | SCOPEFLAG_EXEC);
                    tmpop.opcode = SCRIPTOPCODE_TESTIF;
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
                    tmpop.opcode = SCRIPTOPCODE_TESTWHILE;
                    compileScript_addop(&out, &sz, &tmpop);
                    reqcmd = true;
                    goto findcmd;
                } else if (!strcmp(cb.data, "def")) {
                    compileScript_pushscope(&scope, SCRIPTOPCODE_DEF, 0);
                    op.opcode = SCRIPTOPCODE_DEF;
                } else if (!strcmp(cb.data, "on")) {
                    compileScript_pushscope(&scope, SCRIPTOPCODE_ON, 0);
                    op.opcode = SCRIPTOPCODE_ON;
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
                    if (tmp == SCRIPTOPCODE_GROUP || !compileScript_popscope(&scope)) {
                        if (e) cb_addstr(e, "Misplaced 'end'");
                        ret = false;
                        goto ret;
                    }
                    op.opcode = SCRIPTOPCODE_END;
                } else if (!strcmp(cb.data, "sub")) {
                    op.opcode = SCRIPTOPCODE_SUB;
                } else if (!strcmp(cb.data, "return")) {
                    op.opcode = SCRIPTOPCODE_RET;
                } else if (!strcmp(cb.data, "undef")) {
                    op.opcode = SCRIPTOPCODE_UNDEF;
                } else if (!strcmp(cb.data, "unon")) {
                    op.opcode = SCRIPTOPCODE_UNON;
                } else if (!strcmp(cb.data, "set")) {
                    op.opcode = SCRIPTOPCODE_SET;
                } else if (!strcmp(cb.data, "unset")) {
                    op.opcode = SCRIPTOPCODE_UNSET;
                } else if (!strcmp(cb.data, "get")) {
                    op.opcode = SCRIPTOPCODE_GET;
                } else if (!strcmp(cb.data, "test")) {
                    op.opcode = SCRIPTOPCODE_TEST;
                } else if (!strcmp(cb.data, "[")) {
                    op.opcode = SCRIPTOPCODE_BRACTEST;
                } else if (!strcmp(cb.data, "$") || !strcmp(cb.data, "text")) {
                    op.opcode = SCRIPTOPCODE_TEXT;
                } else if (!strcmp(cb.data, "math")) {
                    op.opcode = SCRIPTOPCODE_MATH;
                } else if (!strcmp(cb.data, "fire")) {
                    op.opcode = SCRIPTOPCODE_FIRE;
                } else if (!strcmp(cb.data, "sleep")) {
                    op.opcode = SCRIPTOPCODE_SLEEP;
                } else if (!strcmp(cb.data, "exit")) {
                    op.opcode = SCRIPTOPCODE_EXIT;
                } else {
                    if (findcmd && (op.data.cmd.func = findcmd(cb.data))) {
                        op.opcode = SCRIPTOPCODE_CMD;
                    } else {
                        if (e) cb_addstr(e, "Unknown command '");
                        cb_addstr(e, cb.data);
                        cb_add(e, '\'');
                        ret = false;
                        goto ret;
                    }
                }
            }
            if (c == '|' || c == '&' || c == '(') {
                reqcmd = true;
                break;
            }
            if (c == '\n' || c == ';' || c == ')' || c == '#') {
                break;
            }
            ++arg;
            cb_clear(&cb);
        }
        cb_clear(&cb);
        compileScript_addop(&out, &sz, &op);
        if (c == ')') {
            uint8_t f;
            enum scriptopcode tmp = compileScript_getscope(&scope, &f);
            if (tmp != SCRIPTOPCODE_GROUP || !compileScript_popscope(&scope)) {
                if (e) cb_addstr(e, "Misplaced ')'");
                ret = false;
                goto ret;
            }
            tmpop.opcode = SCRIPTOPCODE_END;
            compileScript_addop(&out, &sz, &tmpop);
        }
        if (scope.index >= 0) {
            if (scope.data[scope.index] & SCOPEFLAG_PUTEND) {
                tmpop.opcode = SCRIPTOPCODE_END;
                compileScript_addop(&out, &sz, &tmpop);
                scope.data[scope.index] &= ~(SCOPEFLAG_PUTEND << 8);
            }
        } else {
            if (putend) {
                tmpop.opcode = SCRIPTOPCODE_END;
                compileScript_addop(&out, &sz, &tmpop);
                putend = false;
            }
        }
        if (c == '|') {
            c = compiler_fgetc(&f);
            if (c == '|') {
                if (scope.index >= 0) {
                    scope.data[scope.index] |= SCOPEFLAG_PUTEND << 8;
                } else {
                    putend = true;
                }
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
                if (scope.index >= 0) {
                    scope.data[scope.index] |= SCOPEFLAG_PUTEND << 8;
                } else {
                    putend = true;
                }
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
        } else if (c == '#') {
            compiler_fungetc(&f);
        } else if (c == EOF) {
            break;
        }
        continue;
        findcmd:;
        cb_clear(&cb);
        compiler_fungetc(&f);
    }
    if (scope.index != -1) {
        if (e) {
            enum scriptopcode tmp = compileScript_getscope(&scope, NULL);
            if (tmp == SCRIPTOPCODE_GROUP) {
                cb_addstr(e, "Missing ')'");
            } else {
                cb_addstr(e, "Missing 'end'");
            }
        }
        ret = false;
        goto ret;
    }
    ret:;
    if (ret) {
        struct scriptop tmpop;
        tmpop.opcode = SCRIPTOPCODE_EXIT;
        compileScript_addop(&out, &sz, &tmpop);
        _out->ops = realloc(out.ops, sz.oplen);
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

struct scriptstate* newScriptState(struct script* s, struct scripteventtable* t) {
    struct scriptstate* ss = malloc(sizeof(*ss));
    #ifndef PSRC_NOMT
    if (!createMutex(&ss->lock)) {
        free(ss);
        return NULL;
    }
    #endif
    ss->script = s;
    ss->eventtable = t;
    ss->state.index = -1;
    ss->state.size = 4;
    ss->state.data = malloc(ss->state.size * sizeof(*ss->state.data));
    //ss_pushstate(ss, 
    ss->vars.len = 0;
    ss->vars.size = 4;
    ss->vars.data = malloc(ss->vars.size * sizeof(*ss->vars.data));
    ss->subs.len = 0;
    ss->subs.size = 4;
    ss->subs.data = malloc(ss->subs.size * sizeof(*ss->subs.data));
    ss->eventsubs.len = 0;
    ss->eventsubs.size = 4;
    ss->eventsubs.data = malloc(ss->eventsubs.size * sizeof(*ss->eventsubs.data));
    ss->args.len = 0;
    ss->args.size = 16;
    ss->args.data = malloc(ss->args.size * sizeof(*ss->args.data));
    cb_init(&ss->acc, 256);
    cb_init(&ss->in, 256);
    cb_init(&ss->out, 256);
    ss->waitstate = SCRIPTWAIT_NONE;
    return ss;
}

bool execScriptState(struct scriptstate* ss, int* ec, struct charbuf* out) {
    switch (ss->waitstate) {
        case SCRIPTWAIT_NONE:
            break;
        case SCRIPTWAIT_INTUNTIL:
        case SCRIPTWAIT_UNTIL:
            if (altutime() >= ss->waituntil) return true;
            break;
        case SCRIPTWAIT_INTINF:
        case SCRIPTWAIT_INF:
            return true;
        case SCRIPTWAIT_EXIT:
            return false;
    }
    struct scriptop op;
    struct scriptstatedata* state = &ss->state.data[ss->state.index];
    while (1) {
        int ret = -1;
        //ss_readop(ss->script, ss->state.data[ss->state.index].pc, &op);
        if (op.opcode == SCRIPTOPCODE_PIPE) {
            union {
                char* c;
                int i;
            } tmp;
            tmp.c = ss->in.data;
            ss->in.data = ss->out.data;
            ss->out.data = tmp.c;
            tmp.i = ss->in.size;
            ss->in.size = ss->out.size;
            ss->out.size = tmp.i;
            ss->in.len = ss->out.len;
            ss->out.len = 0;
            continue;
        } else if (out && ss->out.len > 0) {
            cb_addpartstr(out, ss->out.data, ss->out.len);
            cb_clear(&ss->out);
        }
        switch (op.opcode) {
            case SCRIPTOPCODE_TRUE: {
                state->ret = 0;
            } break;
            case SCRIPTOPCODE_FALSE: {
                state->ret = 1;
            } break;
            case SCRIPTOPCODE_AND: {
            } break;
            case SCRIPTOPCODE_OR: {
            } break;
            case SCRIPTOPCODE_GROUP: {
            } break;
            case SCRIPTOPCODE_IF: {
            } break;
            case SCRIPTOPCODE_ELIF: {
            } break;
            case SCRIPTOPCODE_ELSE: {
            } break;
            case SCRIPTOPCODE_TESTIF: {
            } break;
            case SCRIPTOPCODE_WHILE: {
            } break;
            case SCRIPTOPCODE_TESTWHILE: {
            } break;
            case SCRIPTOPCODE_CONT: {
            } break;
            case SCRIPTOPCODE_BREAK: {
            } break;
            case SCRIPTOPCODE_DEF: {
            } break;
            case SCRIPTOPCODE_ON: {
            } break;
            case SCRIPTOPCODE_END: {
            } break;
            case SCRIPTOPCODE_SUB: {
            } break;
            case SCRIPTOPCODE_RET: {
            } break;
            case SCRIPTOPCODE_UNDEF: {
            } break;
            case SCRIPTOPCODE_UNON: {
            } break;
            case SCRIPTOPCODE_ADD: {
            } break;
            case SCRIPTOPCODE_PUSH: {
            } break;
            case SCRIPTOPCODE_READ: {
            } break;
            case SCRIPTOPCODE_READVAR: {
            } break;
            case SCRIPTOPCODE_READVARSEP: {
            } break;
            case SCRIPTOPCODE_READARRAY: {
            } break;
            case SCRIPTOPCODE_READARRAYSEP: {
            } break;
            case SCRIPTOPCODE_ARRAY: {
            } break;
            case SCRIPTOPCODE_SET: {
            } break;
            case SCRIPTOPCODE_GET: {
            } break;
            case SCRIPTOPCODE_UNSET: {
            } break;
            case SCRIPTOPCODE_TEST: {
            } break;
            case SCRIPTOPCODE_BRACTEST: {
            } break;
            case SCRIPTOPCODE_TEXT: {
                if (ss->args.len) {
                    cb_addpartstr(&ss->out, ss->args.data[0].data, ss->args.data[0].len);
                    for (int i = 1; i < ss->args.len; ++i) {
                        cb_add(&ss->out, 0);
                        cb_addpartstr(&ss->out, ss->args.data[i].data, ss->args.data[i].len);
                    }
                }
                cb_nullterm(&ss->out);
            } break;
            case SCRIPTOPCODE_MATH: {
            } break;
            case SCRIPTOPCODE_FIRE: {
            } break;
            case SCRIPTOPCODE_SLEEP: {
                if (ss->args.len == 0) {
                    cb_addstr(&ss->out, "sleep: too few arguments\n");
                    cb_nullterm(&ss->out);
                    state->ret = 1;
                } else if (ss->args.len == 1) {
                    bool negative;
                    char* arg = ss->args.data[0].data;
                    if (*arg == '-') {
                        ++arg;
                        negative = true;
                    } else {
                        negative = false;
                    }
                    if (!*arg) {
                        cb_addstr(&ss->out, "sleep: syntax error\n");
                        cb_nullterm(&ss->out);
                        state->ret = 1;
                        break;
                    }
                    if (!strcmp(arg, "inf")) {
                        ss->waitstate = (negative) ? SCRIPTWAIT_INF : SCRIPTWAIT_INTINF;
                    } else {
                        uint64_t t = altutime();
                        uint64_t d = 0;
                        while (1) {
                            if (!*arg) break;
                            if (*arg == '.') {
                                int mul = 100000;
                                while (1) {
                                    ++arg;
                                    if (!*arg) break;
                                    if (*arg < '0' || *arg > '9') {
                                        cb_addstr(&ss->out, "sleep: syntax error\n");
                                        cb_nullterm(&ss->out);
                                        state->ret = 1;
                                        goto longbreak;
                                    }
                                    d += (*arg - '0') * mul;
                                    mul /= 10;
                                }
                                break;
                            }
                            d *= 10;
                            if (*arg < '0' || *arg > '9') {
                                cb_addstr(&ss->out, "sleep: syntax error\n");
                                cb_nullterm(&ss->out);
                                state->ret = 1;
                                goto longbreak;
                            }
                            d += (*arg - '0') * 1000000;
                            ++arg;
                        }
                        ss->waituntil = t + d;
                        ss->waitstate = (negative) ? SCRIPTWAIT_UNTIL : SCRIPTWAIT_INTUNTIL;
                    }
                    state->ret = 0;
                    ret = 1;
                } else {
                    cb_addstr(&ss->out, "sleep: too many arguments\n");
                    cb_nullterm(&ss->out);
                    state->ret = 1;
                }
                longbreak:;
            } break;
            case SCRIPTOPCODE_CMD: {
                state->ret = op.data.cmd.func(ss, &ss->in, ss->args.len, ss->args.data, &ss->out);
                cb_nullterm(&ss->out);
            } break;
            case SCRIPTOPCODE_EXIT: {
                if (ss->args.len == 0) {
                    if (ec) *ec = 0;
                } else if (ss->args.len == 1) {
                    if (ec) *ec = atoi(ss->args.data[0].data);
                } else {
                    cb_addstr(&ss->out, "exit: too many arguments\n");
                    cb_nullterm(&ss->out);
                    state->ret = 1;
                    break;
                }
                ss->waitstate = SCRIPTWAIT_EXIT;
                ret = 0;
            } break;
            default: break;
        }
        if (ret == 0) {
            if (out) cb_addpartstr(out, ss->out.data, ss->out.len);
            return false;
        } else if (ret == 1) {
            return true;
        }
    }
}

void resetScriptState(struct scriptstate* ss, struct script* s) {
    
}

void clearScriptState(struct scriptstate* ss, struct script* s) {
    
}

void deleteScriptState(struct scriptstate* ss) {
    while (ss->state.index >= 0) {
        //ss_popstate(ss);
    }
    for (int i = 0; i < ss->vars.len; ++i) {
        struct scriptstatevar* v = &ss->vars.data[i];
        if (v->name) {
            free(v->name);
            if (v->arraysize >= 0) {
                for (int j = 0; j < v->arraysize; ++j) {
                    free(v->array.data[j]);
                }
                free(v->array.data);
                free(v->array.len);
            } else {
                free(v->single.data);
            }
        }
    }
    for (int i = 0; i < ss->subs.len; ++i) {
        struct scriptstatesub* s = &ss->subs.data[i];
        if (s->name) {
            free(s->name);
            if (s->copied) free(s->data);
        }
    }
    if (ss->eventtable) {
        #ifndef PSRC_NOMT
        acquireWriteAccess(&ss->eventtable->lock);
        #endif
        for (int i = 0; i < ss->eventsubs.len; ++i) {
            struct scriptstateeventsub* s = &ss->eventsubs.data[i];
            if (s->name) {
                struct scriptevent* e = &ss->eventtable->data[s->tableeventindex];
                e->subs[s->tablesubindex].script = NULL;
                --e->uses;
                free(s->name);
                if (s->copied) free(s->data);
            }
        }
        #ifndef PSRC_NOMT
        releaseWriteAccess(&ss->eventtable->lock);
        #endif
    }
    for (int i = 0; i < ss->args.len; ++i) {
        struct charbuf* a = &ss->args.data[i];
        cb_dump(a);
    }
    cb_dump(&ss->acc);
    cb_dump(&ss->in);
    cb_dump(&ss->out);
}
