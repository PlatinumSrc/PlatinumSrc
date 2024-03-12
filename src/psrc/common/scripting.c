#include "scripting.h"
#include "crc.h"
#include "filesystem.h"
#include "logging.h"
#include "time.h"

#include <stdio.h>
#include <limits.h>
#include <inttypes.h>

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
    return false;
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

static inline int ss_readop(struct script s, int pc, struct scriptop* op) {
    return -1;
}

static inline void ss_pusharg(struct scriptstatedata* state) {

}
static inline void ss_prepargs(struct scriptstatedata* state) {
    
}
static inline void ss_clearargs(struct scriptstatedata* state) {
    
}

static inline struct scriptstatedata* ss_pushstate(struct scriptstate* ss, struct scriptstatesub* sub) {
    return NULL;
}
static inline struct scriptstatedata* ss_popstate(struct scriptstate* ss) {
    return NULL;
}
static inline struct scriptstatedata* ss_fireevent(struct scriptstate* ss, char* name, int namelen, int argc, struct scriptarg* argv) {
    return NULL;
}

static inline struct scriptstatevar* ss_newvar(struct scriptstate* ss, char* name, int namelen, uint32_t namecrc, int size) {
    return NULL;
}
static inline struct scriptstatevar* ss_findvar(struct scriptstate* ss, char* name, int namelen, uint32_t namecrc) {
    return NULL;
}
static inline void ss_delvar(struct scriptstatevar* v) {
    
}

static inline int ss_findsub(char* name, int namelen) {
    return -1;
}
static inline void ss_setsub(struct scriptstate* ss, char* name, int namelen, struct script s, int newpc) {
    
}

static inline void ss_setevsub(struct scriptstate* ss, char* name, int namelen, struct script s, int newpc) {
    
}

static inline int ss_skipdef(struct script s, int newpc) {
    return -1;
}
static inline int ss_skipon(struct script s, int newpc) {
    return -1;
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
    int counter = 0;
    while (1) {
        ++counter;
        struct scriptop op;
        int newpc = ss_readop(state->script, state->pc, &op);
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
                struct scriptstatevar* v = ss_findvar(ss, &state->script.strings[op.readvar.offset], op.readvar.len, op.readvar.crc);
                if (v) {
                    int vsz = v->size;
                    if (vsz) {
                        --vsz;
                        int i = 0;
                        while (1) {
                            cb_addpartstr(&state->acc, v->array.data[i], v->array.len[i]);
                            if (i == vsz) break;
                            ++i;
                            ss_pusharg(state);
                        }
                    } else if (vsz < 0) {
                        cb_addpartstr(&state->acc, v->single.data, v->single.len);
                    }
                }
            } break;
            case SCRIPTOPCODE_READVARSEP: {
                struct scriptstatevar* v = ss_findvar(ss, &state->script.strings[op.readvar.offset], op.readvar.len, op.readvar.crc);
                int vsz = v->size;
                if (vsz) {
                    int space = -1;
                    for (int j = 0; j < vsz; ++j) {
                        char* data = v->array.data[j];
                        int len = v->array.len[j];
                        for (int i = 0; i < len; ++i) {
                            char c = data[i];
                            if (c == ' ' || c == '\t' || c == '\n' || !c) {
                                if (!space) space = 1;
                            } else {
                                if (space) {
                                    if (space == 1) ss_pusharg(state);
                                    space = 0;
                                }
                                cb_add(&state->acc, c);
                            }
                        }
                    }
                } else if (vsz < 0) {
                    int space = -1;
                    char* data = v->single.data;
                    int len = v->single.len;
                    for (int i = 0; i < len; ++i) {
                        char c = data[i];
                        if (c == ' ' || c == '\t' || c == '\n' || !c) {
                            if (!space) space = 1;
                        } else {
                            if (space) {
                                if (space == 1) ss_pusharg(state);
                                space = 0;
                            }
                            cb_add(&state->acc, c);
                        }
                    }
                }
            } break;
            case SCRIPTOPCODE_READARRAY: {
                struct scriptstatevar* v = ss_findvar(ss, &state->script.strings[op.readvar.offset], op.readvar.len, op.readvar.crc);
            } break;
            case SCRIPTOPCODE_READARRAYSEP: {
                struct scriptstatevar* v = ss_findvar(ss, &state->script.strings[op.readvar.offset], op.readvar.len, op.readvar.crc);
            } break;
            case SCRIPTOPCODE_READARGCT: {
                char tmp[16];
                sprintf(tmp, "%d", state->inargs.argc);
                cb_addstr(&state->acc, tmp);
            } break;
            case SCRIPTOPCODE_READARG: {
                int max = state->inargs.argc - 1;
                if (op.readarg.from < 0) op.readarg.from = max - op.readarg.from;
                if (op.readarg.to < 0) op.readarg.to = max - op.readarg.to;
                else if (op.readarg.to < max) op.readarg.to = max;
                if (op.readarg.to >= op.readarg.from) {
                    cb_addpartstr(&state->acc, state->inargs.argv[op.readarg.from].data, state->inargs.argv[op.readarg.from].len);
                    ++op.readarg.from;
                    for (; op.readarg.from <= op.readarg.to; ++op.readarg.from) {
                        ss_pusharg(state);
                        cb_addpartstr(&state->acc, state->inargs.argv[op.readarg.from].data, state->inargs.argv[op.readarg.from].len);
                    }
                }
            } break;
            case SCRIPTOPCODE_READARGSEP: {
                int max = state->inargs.argc - 1;
                if (op.readarg.from < 0) op.readarg.from = max - op.readarg.from;
                if (op.readarg.to < 0) op.readarg.to = max - op.readarg.to;
                else if (op.readarg.to < max) op.readarg.to = max;
                int space = -1;
                for (; op.readarg.from <= op.readarg.to; ++op.readarg.from) {
                    char* data = state->inargs.argv[op.readarg.from].data;
                    int len = state->inargs.argv[op.readarg.from].len;
                    for (int i = 0; i < len; ++i) {
                        char c = data[i];
                        if (c == ' ' || c == '\t' || c == '\n' || !c) {
                            if (!space) space = 1;
                        } else {
                            if (space) {
                                if (space == 1) ss_pusharg(state);
                                space = 0;
                            }
                            cb_add(&state->acc, c);
                        }
                    }
                }
            } break;
            case SCRIPTOPCODE_READRET: {
                char tmp[16];
                sprintf(tmp, "%d", state->ret);
                cb_addstr(&state->acc, tmp);
            } break;
            case SCRIPTOPCODE_PUSH: {
                ss_pusharg(state);
            } break;
            case SCRIPTOPCODE_DEF: {
                ss_prepargs(state);
                if (state->args.len == 0) {
                    cb_addstr(&state->out, "def: too few arguments\n");
                    state->ret = -1;
                    ret = 0;
                } else if (state->args.len == 1) {
                    ss_setsub(ss, state->args.data[0].data, state->args.data[0].len, state->script, newpc);
                    newpc = ss_skipdef(state->script, newpc);
                } else {
                    cb_addstr(&state->out, "def: too many arguments\n");
                    state->ret = -1;
                    ret = 0;
                }
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_ON: {
                ss_prepargs(state);
                if (state->args.len == 0) {
                    cb_addstr(&state->out, "on: too few arguments\n");
                    state->ret = -1;
                    ret = 0;
                } else if (state->args.len == 1) {
                    ss_setevsub(ss, state->args.data[0].data, state->args.data[0].len, state->script, newpc);
                    newpc = ss_skipon(state->script, newpc);
                } else {
                    cb_addstr(&state->out, "on: too many arguments\n");
                    state->ret = -1;
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
                        newpc = -1;
                        if (state->args.len == 1) {
                            ss_clearargs(state);
                            state = ss_pushstate(ss, &ss->subs.data[s]);
                        } else {
                            struct scriptstatedata* newstate = ss_pushstate(ss, &ss->subs.data[s]);
                            // TODO: clone args[1...] from state to inargs[0...] of newstate
                            ss_clearargs(state);
                            state = newstate;
                        }
                    } else {
                        cb_addstr(&state->out, "sub: cannot find '");
                        cb_addpartstr(&state->out, state->args.data[0].data, state->args.data[0].len);
                        cb_add(&state->out, '\'');
                        cb_add(&state->out, '\n');
                        state->ret = 1;
                        ss_clearargs(state);
                    }
                }
            } break;
            case SCRIPTOPCODE_RET: {
                ss_prepargs(state);
                if (state->args.len == 0) {
                    state = ss_popstate(ss);
                    state->ret = 0;
                    newpc = -1;
                } else if (state->args.len == 1) {
                    state = ss_popstate(ss);
                    state->ret = atoi(state->args.data[0].data);
                    newpc = -1;
                } else if (state->args.len == 2) {
                    state = ss_popstate(ss);
                    state->ret = atoi(state->args.data[0].data);
                    cb_addpartstr(&state->out, state->args.data[1].data, state->args.data[1].len);
                    newpc = -1;
                } else {
                    cb_addstr(&state->out, "ret: too many arguments\n");
                    state->ret = -1;
                    ret = 0;
                    break;
                }
                ss_clearargs(state);
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
                // !!! do not change return value if reading value from pipe !!!
                if (state->args.len == 0) {
                    cb_addstr(&state->out, "set: too few arguments\n");
                    state->ret = 1;
                } else if (state->args.len > 2) {
                    cb_addstr(&state->out, "set: too many arguments\n");
                    state->ret = 1;
                } else {
                    char* name = state->args.data[0].data;
                    int namelen = state->args.data[0].len;
                    if (namelen == 0) {
                        cb_addstr(&state->out, "set: empty variable name\n");
                        state->ret = 1;
                        goto set_longbreak;
                    }
                    char* data;
                    int len;
                    if (state->args.len == 2) {
                        data = state->args.data[1].data;
                        len = state->args.data[1].len;
                        state->ret = 0;
                    } else {
                        data = state->in.data;
                        len = state->in.len;
                    }
                    bool array = false;
                    int index;
                    for (int i = namelen - 1; i >= 0; --i) {
                        if (name[i] == ':') {
                            namelen = i;
                            if (i != namelen - 1) {
                                index = atoi(&name[i + 1]);
                                array = true;
                            }
                            break;
                        }
                    }
                    if (namelen == 0) {
                        cb_addstr(&state->out, "set: empty variable name\n");
                        state->ret = 1;
                        goto set_longbreak;
                    }
                    uint32_t namecrc = crc32(name, namelen);
                    struct scriptstatevar* v = ss_findvar(ss, name, namelen, namecrc);
                    if (array) {
                        if (!v) {
                            v = ss_newvar(ss, name, namelen, namecrc, index);
                            int sz = index + 1;
                            v->array.data = malloc(sz * sizeof(*v->array.data));
                            v->array.len = malloc(sz * sizeof(*v->array.len));
                            for (int i = 0; i < index; ++i) {
                                v->array.data[i] = NULL;
                                v->array.len[i] = 0;
                            }
                        } else if (v->size < 0) {
                            ss_delvar(v);
                            v = ss_newvar(ss, name, namelen, namecrc, index);
                            int sz = index + 1;
                            v->array.data = malloc(sz * sizeof(*v->array.data));
                            v->array.len = malloc(sz * sizeof(*v->array.len));
                            for (int i = 0; i < index; ++i) {
                                v->array.data[i] = NULL;
                                v->array.len[i] = 0;
                            }
                        } else {
                            int oldsz = v->size;
                            int newsz = index + 1;
                            if (newsz > oldsz) {
                                v->array.data = realloc(v->array.data, newsz * sizeof(*v->array.data));
                                v->array.len = realloc(v->array.len, newsz * sizeof(*v->array.len));
                                for (; oldsz < newsz; ++oldsz) {
                                    v->array.data[oldsz] = NULL;
                                    v->array.len[oldsz] = 0;
                                }
                            } else {
                                free(v->array.data[index]);
                            }
                        }
                        v->array.data[index] = data;
                        v->array.len[index] = len;
                    } else {
                        if (!v) {
                            v = ss_newvar(ss, name, namelen, namecrc, -1);
                        } else if (v->size < 0) {
                            ss_delvar(v);
                            v = ss_newvar(ss, name, namelen, namecrc, -1);
                        }
                        v->single.data = data;
                        v->single.len = len;
                    }
                }
                set_longbreak:;
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_GET: {
                ss_prepargs(state);
                if (state->args.len == 0) {
                    cb_addstr(&state->out, "get: too few arguments\n");
                    state->ret = 1;
                } else if (state->args.len == 1) {
                    char* name = state->args.data[0].data;
                    int namelen = state->args.data[0].len;
                    if (namelen == 0) {
                        cb_addstr(&state->out, "get: empty variable name\n");
                        state->ret = 1;
                        goto get_longbreak;
                    }
                    for (int i = namelen - 1; i >= 0; --i) {
                        if (name[i] == ':') {
                            namelen = i;
                            break;
                        }
                    }
                    if (namelen == 0) {
                        cb_addstr(&state->out, "get: empty variable name\n");
                        state->ret = 1;
                        goto get_longbreak;
                    }
                    state->ret = 0;
                    if (namelen == 1) {
                        if (name[0] == '@') {
                            
                        } else if (name[0] == '#') {
                        } else if (name[0] == '?') {
                        }
                    }
                    struct scriptstatevar* v = ss_findvar(ss, name, namelen, crc32(name, namelen));
                    if (v) {
                        if (v->size >= 0) {
                            if (name[namelen] == '#' && !name[namelen + 1]) {
                                char tmp[16];
                                sprintf(tmp, "%d", state->ret);
                                cb_addstr(&state->acc, tmp);
                            } else {
                                int max = v->size - 1;
                                int from = 0;
                                int to = max;
                                if (name[namelen]) {
                                    sscanf(&name[namelen + 1], "%d,%d", &from, &to);
                                    if (from < 0) from = max - from;
                                    if (to < 0) to = max - to;
                                    if (to < from) goto get_longbreak;
                                }
                                cb_addpartstr(&state->out, v->array.data[0], v->array.len[0]);
                                ++from;
                                for (; from <= to; ++from) {
                                    cb_add(&state->out, 0);
                                    cb_addpartstr(&state->out, v->array.data[from], v->array.len[from]);
                                }
                            }
                        } else if (name[namelen] != '#' || name[namelen + 1]) {
                            cb_addpartstr(&state->out, v->single.data, v->single.len);
                        }
                    }
                } else {
                    cb_addstr(&state->out, "get: too many arguments\n");
                    state->ret = 1;
                }
                get_longbreak:;
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_UNSET: {
                ss_prepargs(state);
                if (state->args.len == 0) {
                    cb_addstr(&state->out, "unset: too few arguments\n");
                    state->ret = 1;
                } else if (state->args.len == 1) {
                    if (state->args.data[0].len == 0) {
                        cb_addstr(&state->out, "unset: empty variable name\n");
                        state->ret = 1;
                    } else {
                        //ss_delvar
                        state->ret = 0;
                    }
                } else {
                    cb_addstr(&state->out, "unset: too many arguments\n");
                    state->ret = 1;
                }
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_TEXT: {
                ss_prepargs(state);
                if (state->args.len > 0) {
                    cb_addpartstr(&state->out, state->args.data[0].data, state->args.data[0].len);
                    for (int i = 1; i < state->args.len; ++i) {
                        cb_addpartstr(&state->out, state->args.data[i].data, state->args.data[i].len);
                    }
                }
                state->ret = 0;
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_FIRE: {
                ss_prepargs(state);
                if (state->args.len == 0) {
                    cb_addstr(&state->out, "fire: too few arguments\n");
                    state->ret = 1;
                } else {
                    state->pc = newpc;
                    newpc = -1;
                    state->ret = 0;
                    state = ss_fireevent(ss, state->args.data[0].data, state->args.data[0].len, state->args.len - 1, &state->args.data[1]);
                }
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_TEST: {
                ss_prepargs(state);
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_MATH: {
                ss_prepargs(state);
                state->ret = 0;
                union {
                    int64_t i;
                    uint64_t u;
                    double f;
                    double v2[2];
                    double v3[3];
                    double v4[4];
                } val, tmp;
                enum __attribute__((packed)) {
                    MT__INVAL = -1,
                    MT_INT, MT_UINT,
                    MT_FLOAT,
                    MT_VEC2, MT_VEC3, MT_VEC4
                } t = MT__INVAL;
                char* arg = state->args.data[0].data;
                if (!strcmp(arg, "i")) t = MT_INT;
                else if (!strcmp(arg, "u")) t = MT_UINT;
                else if (!strcmp(arg, "f")) t = MT_FLOAT;
                else if (!strcmp(arg, "v2")) t = MT_VEC2;
                else if (!strcmp(arg, "v3")) t = MT_VEC3;
                else if (!strcmp(arg, "v4")) t = MT_VEC4;
                int i;
                if (t == MT__INVAL) {t = MT_INT; i = 0;}
                else i = 1;
                bool cmd = false;
                enum __attribute__((packed)) {
                    MC_SET,
                    MC_ADD, MC_SUB, MC_MUL, MC_DIV, MC_REM, MC_EXP,
                    MC_OR, MC_AND, MC_XOR, MC_NOT, MC_LS, MC_RS
                } c = MC_SET;
                for (; i < state->args.len; ++i) {
                    arg = state->args.data[i].data;
                    if (cmd) {
                        if (!strcmp(arg, "+")) c = MC_ADD;
                        else if (!strcmp(arg, "*")) c = MC_MUL;
                        else if (!strcmp(arg, "-")) c = MC_SUB;
                        else if (!strcmp(arg, "/")) c = MC_DIV;
                        else if (!strcmp(arg, "%")) c = MC_REM;
                        else if (!strcmp(arg, "e")) c = MC_EXP;
                        else if (t == MT_INT || t == MT_UINT) {
                            if (!strcmp(arg, "|")) c = MC_OR;
                            else if (!strcmp(arg, "&")) c = MC_AND;
                            else if (!strcmp(arg, "^")) c = MC_XOR;
                            else if (!strcmp(arg, "~")) c = MC_NOT;
                            else if (!strcmp(arg, "<<")) c = MC_LS;
                            else if (!strcmp(arg, ">>")) c = MC_RS;
                            else {
                                cb_addstr(&state->out, "math: invalid command: '");
                                cb_addstr(&state->out, arg);
                                cb_add(&state->out, '\'');
                                cb_add(&state->out, '\n');
                                state->ret = 1;
                                break;
                            }
                        } else {
                            cb_addstr(&state->out, "math: invalid command: '");
                            cb_addstr(&state->out, arg);
                            cb_add(&state->out, '\'');
                            cb_add(&state->out, '\n');
                            state->ret = 1;
                            break;
                        }
                    } else {
                        switch (c) {
                            case MC_SET: {
                                switch (t) {
                                    case MT_INT: val.i = tmp.i; break;
                                    case MT_UINT: val.u = tmp.u; break;
                                    case MT_FLOAT: val.f = tmp.f; break;
                                    case MT_VEC2:
                                        val.v2[0] = tmp.v2[0];
                                        val.v2[1] = tmp.v2[1];
                                        break;
                                    case MT_VEC3:
                                        val.v3[0] = tmp.v3[0];
                                        val.v3[1] = tmp.v3[1];
                                        val.v3[2] = tmp.v3[2];
                                        break;
                                    case MT_VEC4:
                                        val.v4[0] = tmp.v4[0];
                                        val.v4[1] = tmp.v4[1];
                                        val.v4[2] = tmp.v4[2];
                                        val.v4[3] = tmp.v4[3];
                                        break;
                                    default: break;
                                }
                            } break;
                            case MC_ADD: {
                                switch (t) {
                                    case MT_INT: val.i += tmp.i; break;
                                    case MT_UINT: val.u += tmp.u; break;
                                    case MT_FLOAT: val.f += tmp.f; break;
                                    case MT_VEC2:
                                        val.v2[0] += tmp.v2[0];
                                        val.v2[1] += tmp.v2[1];
                                        break;
                                    case MT_VEC3:
                                        val.v3[0] += tmp.v3[0];
                                        val.v3[1] += tmp.v3[1];
                                        val.v3[2] += tmp.v3[2];
                                        break;
                                    case MT_VEC4:
                                        val.v4[0] += tmp.v4[0];
                                        val.v4[1] += tmp.v4[1];
                                        val.v4[2] += tmp.v4[2];
                                        val.v4[3] += tmp.v4[3];
                                        break;
                                    default: break;
                                }
                            } break;
                            default: break;
                        }
                    }
                }
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
                        goto sleep_longbreak;
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
                                        goto sleep_longbreak;
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
                                goto sleep_longbreak;
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
                sleep_longbreak:;
                ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_CMD: {
                ss_prepargs(state);
                state->ret = op.cmd.func(ss, &state->in, state->args.len, state->args.data, &state->out);
                if (state->ret < 0) ret = 0;
                else ss_clearargs(state);
            } break;
            case SCRIPTOPCODE_EXIT: {
                ss_prepargs(state);
                if (state->args.len == 0) {
                    state->ret = 0;
                } else if (state->args.len == 1) {
                    state->ret = atoi(state->args.data[0].data);
                } else {
                    ss_clearargs(state);
                    cb_addstr(&state->out, "exit: too many arguments\n");
                    state->ret = -1;
                    break;
                }
                ret = 0;
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
        if (newpc >= 0) state->pc = newpc;
        if (ret == 1 || counter == 64) return true;
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
            if (v->size >= 0) {
                for (int j = 0; j < v->size; ++j) {
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
