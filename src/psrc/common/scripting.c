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
    struct scriptstatedata* state = &ss->state.data[ss->state.index];
    int ret = -1;
    while (1) {
        struct scriptop op;
        int newpc = 0; //ss_readop(state->script, state->pc, &op);
        switch (op.opcode & 0x3F) {
            case SCRIPTOPCODE_TRUE: {
                state->ret = 0;
            } break;
            case SCRIPTOPCODE_FALSE: {
                state->ret = 1;
            } break;
            case SCRIPTOPCODE_JMP: {
                newpc += op.jmp.offset;
            } break;
            case SCRIPTOPCODE_JMPTRUE: {
                if (!state->ret) newpc += op.jmp.offset;
            } break;
            case SCRIPTOPCODE_JMPFALSE: {
                if (state->ret) newpc += op.jmp.offset;
            } break;
            case SCRIPTOPCODE_ADD: {
                cb_addpartstr(&state->acc, &state->script.strings[op.add.offset], op.add.len);
            } break;
            case SCRIPTOPCODE_READVAR: {
                
            } break;
            case SCRIPTOPCODE_READVARSEP: {
                
            } break;
            case SCRIPTOPCODE_READARRAY: {
                
            } break;
            case SCRIPTOPCODE_READARRAYSEP: {
                
            } break;
            case SCRIPTOPCODE_READARG: {
                
            } break;
            case SCRIPTOPCODE_READARGSEP: {
                
            } break;
            case SCRIPTOPCODE_READRET: {
                char tmp[16];
                sprintf(tmp, "%d", state->ret);
                cb_addstr(&state->acc, tmp);
            } break;
            case SCRIPTOPCODE_PUSH: {
                
            } break;
            case SCRIPTOPCODE_DEF: {
                ss_prepargs(state);
                if (state->args.len == 0) {
                    cb_addstr(&state->out, "def: too few arguments\n");
                    ret = 0;
                } else if (state->args.len == 1) {
                    ss_setsub(ss, state->args.data[0].data, state->args.data[0].len, state->script, newpc);
                    newpc = ss_skipdef(state->script, newpc);
                } else {
                    cb_addstr(&state->out, "def: too many arguments\n");
                    ret = 0;
                }
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_ON: {
                ss_prepargs(state);
                if (state->args.len == 0) {
                    cb_addstr(&state->out, "on: too few arguments\n");
                    ret = 0;
                } else if (state->args.len == 1) {
                    ss_setevsub(ss, state->args.data[0].data, state->args.data[0].len, state->script, newpc);
                    newpc = ss_skipon(state->script, newpc);
                } else {
                    cb_addstr(&state->out, "on: too many arguments\n");
                    ret = 0;
                }
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_SUB: {
                ss_prepargs(state);
                if (state->args.len == 0) {
                    cb_addstr(&state->out, "sub: too few arguments\n");
                    state->ret = 1;
                } else {
                    int s = ss_findsub(state->args.data[0].data, state->args.data[0].len);
                    if (s >= 0) {
                        state->pc = newpc;
                        if (state->args.len == 1) {
                            ss_clearargs(state);
                            state = ss_pushstate(ss, &ss->subs.data[s]);
                        } else {
                            struct scriptstatedata* newstate = ss_pushstate(ss, &ss->subs.data[s]);
                            // TODO: clone args[1...] from state to inargs[0...] of newstate
                            ss_clearargs(state);
                            state = newstate;
                        }
                        continue;
                    } else {
                        cb_addstr(&state->out, "sub: cannot find '");
                        cb_addpartstr(&state->out, state->args.data[0].data, state->args.data[0].len);
                        cb_add(&state->out, '\'');
                        cb_add(&state->out, '\n');
                        state->ret = 1;
                    }
                }
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_RET: {
                ss_prepargs(state);
                if (state->args.len == 0) {
                    state = ss_popstate(ss);
                    state->ret = 0;
                    ss_clearargs(state);
                    continue;
                } else if (state->args.len == 1) {
                    state = ss_popstate(ss);
                    state->ret = atoi(state->args.data[0].data);
                    ss_clearargs(state);
                    continue;
                } else if (state->args.len == 2) {
                    state = ss_popstate(ss);
                    state->ret = atoi(state->args.data[0].data);
                    cb_addpartstr(&state->out, state->args.data[1].data, state->args.data[1].len);
                    ss_clearargs(state);
                    continue;
                } else {
                    cb_addstr(&state->out, "ret: too many arguments\n");
                    state->ret = 0;
                    ret = 0;
                }
            } break;
            case SCRIPTOPCODE_UNDEF: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_UNON: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_SET: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_GET: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_SETARRAY: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_GETARRAY: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_UNSET: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_TEXT: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_FIRE: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_TEST: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_MATH: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_SLEEP: {
                ss_prepargs(state);
                if (state->args.len == 0) {
                    cb_addstr(&state->out, "sleep: too few arguments\n");
                    state->ret = 1;
                } else if (state->args.len == 1) {
                    bool negative;
                    char* arg = state->args.data[0].data;
                    if (*arg == '-') {
                        ++arg;
                        negative = true;
                    } else {
                        negative = false;
                    }
                    if (!*arg) {
                        cb_addstr(&state->out, "sleep: syntax error\n");
                        state->ret = 1;
                        goto longbreak;
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
                                        cb_addstr(&state->out, "sleep: syntax error\n");
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
                                cb_addstr(&state->out, "sleep: syntax error\n");
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
                    cb_addstr(&state->out, "sleep: too many arguments\n");
                    state->ret = 1;
                }
                longbreak:;
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_CMD: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_EXIT: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
        }
        if (op.opcode & SCRIPTOPFLAG_PIPE) {
            union {
                char* c;
                int i;
            } tmp;
            tmp.c = state->in.data;
            state->in.data = state->out.data;
            state->out.data = tmp.c;
            tmp.i = state->in.size;
            state->in.size = state->out.size;
            state->out.size = tmp.i;
            state->in.len = state->out.len;
            state->out.len = 0;
            cb_nullterm(&state->in);
        } else {
            if (state->in.len > 0) cb_clear(&state->in);
            if (state->out.len > 0) {
                if (out) cb_addpartstr(out, state->out.data, state->out.len);
                cb_clear(&state->out);
            }
        }
        if (ret == 0) {
            ss->waitstate = SCRIPTWAIT_EXIT;
            if (ec) *ec = state->ret;
            return false;
        }
        state->pc = newpc;
        if (ret == 1) {
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
        ss_popstate(ss);
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
}
