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
            len += sizeof(op->add);
            break;
        case SCRIPTOPCODE_READVAR:
        case SCRIPTOPCODE_READVARSEP:
            len += sizeof(op->readvar);
            break;
        case SCRIPTOPCODE_READARRAY:
        case SCRIPTOPCODE_READARRAYSEP:
            len += sizeof(op->readarray);
            break;
        case SCRIPTOPCODE_CMD:
            len += sizeof(op->cmd);
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

struct scriptstate* newScriptState(struct script* s, struct scripteventtable* t, int argc, struct charbuf* argv) {
    struct scriptstate* ss = malloc(sizeof(*ss));
    #ifndef PSRC_NOMT
    if (!createMutex(&ss->lock)) {
        free(ss);
        return NULL;
    }
    #endif
    ss->script = s;
    ss->eventtable = t;
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
            if (v->dim >= 0) {
                for (int j = 0; j < v->dim; ++j) {
                    free(v->array.data[j]);
                }
                free(v->array.data);
                free(v->array.len);
            } else {
                free(v->single.data);
            }
        }
    }
    if (ss->eventtable) {
        #ifndef PSRC_NOMT
        acquireWriteAccess(&ss->eventtable->lock);
        #endif
        for (int i = 0; i < ss->eventsubs.len; ++i) {
            struct scriptstateeventsub* s = &ss->eventsubs.data[i];
            if (s->sub.name) {
                struct scriptevent* e = &ss->eventtable->data[s->tableeventindex];
                e->subs[s->tablesubindex].script = NULL;
                --e->uses;
                free(s->sub.name);
                if (s->sub.copied) {
                    free(s->sub.data.ops);
                    free(s->sub.data.strings);
                }
            }
        }
        #ifndef PSRC_NOMT
        releaseWriteAccess(&ss->eventtable->lock);
        #endif
    }
    cb_dump(&ss->acc);
    cb_dump(&ss->in);
    cb_dump(&ss->out);
}
