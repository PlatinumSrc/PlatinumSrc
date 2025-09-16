#include "../rcmgralloc.h"
#include "pbasic.h"
#include "../debug.h"
#include "../util.h"

#include <stdio.h>
#include <inttypes.h>

static const struct pb_compiler_opt defaultcompopt = PB_COMPILER_OPT_DEFAULTS;

void pb_compitf_puterr(struct pb_compiler* pbc, enum pb_error e, const char* msg, const struct pb_compiler_srcloc* el) {
    if (pbc->opt->errprefix) cb_addstr(pbc->err, pbc->opt->errprefix);
    if (e == PB_ERROR_NONE) cb_addstr(pbc->err, "Warning");
    else cb_addstr(pbc->err, pb__error_str[e]);
    if (el) {
        struct pb_compiler_source* s = &pbc->sources.data[el->src];
        uint32_t incline = s->from.line, inccol = s->from.col;
        char tmp[12];
        cb_addstr(pbc->err, " at line ");
        snprintf(tmp, 12, "%"PRIu32, el->line);
        cb_addstr(pbc->err, tmp);
        cb_addstr(pbc->err, " column ");
        snprintf(tmp, 12, "%"PRIu32, el->col);
        cb_addstr(pbc->err, tmp);
        cb_addstr(pbc->err, " of ");
        cb_addstr(pbc->err, s->type);
        cb_add(pbc->err, ' ');
        cb_add(pbc->err, '\'');
        cb_addstr(pbc->err, pbc->sources.names.data + s->name);
        cb_add(pbc->err, '\'');
        while (incline) {
            s = &pbc->sources.data[s->from.src];
            cb_addstr(pbc->err, " from line ");
            snprintf(tmp, 12, "%"PRIu32, incline);
            cb_addstr(pbc->err, tmp);
            cb_addstr(pbc->err, " column ");
            snprintf(tmp, 12, "%"PRIu32, inccol);
            cb_addstr(pbc->err, tmp);
            cb_addstr(pbc->err, " of ");
            cb_addstr(pbc->err, s->type);
            cb_add(pbc->err, ' ');
            cb_add(pbc->err, '\'');
            cb_addstr(pbc->err, pbc->sources.names.data + s->name);
            cb_add(pbc->err, '\'');
            incline = s->from.line;
            inccol = s->from.col;
        }
    }
    if (msg) {
        cb_add(pbc->err, ':');
        cb_add(pbc->err, ' ');
        if (*msg) cb_addstr(pbc->err, msg);
    }
}

static int pb__compiler_getc(struct pb_compiler* pbc) {
    return pb__compiler_getc_inline(pbc);
}

bool pb_compitf_pushsource(struct pb_compiler* pbc, struct datastream* ds, const char* type, const struct pb_compiler_srcloc* l) {
    size_t srcnamepos = pbc->sources.names.len;
    if (!cb_addstr(&pbc->sources.names, ds->name) || !cb_add(&pbc->sources.names, 0)) return false;
    size_t srcslen = pbc->sources.len;
    struct pb_compiler_source src = {.name = srcnamepos, .type = (type) ? type : "file", .from = *l};
    VLB_ADD(
        pbc->sources,
        src,
        3, 2,
        pbc->sources.names.len = srcnamepos;
        return false;
    );
    VLB_ADD(
        pbc->prevstreams,
        pbc->stream,
        3, 2,
        pbc->sources.names.len = srcnamepos;
        --pbc->sources.len;
        return false;
    );
    pbc->stream = (struct pb_compiler_stream){.ds = ds, .line = 1, .col = 1, .src = srcslen};
    return true;
}

static enum pb_error pb__evalcmd(struct pb_compiler* pbc, struct pb_compiler_tok* tcdata, size_t tclen, struct charbuf* tcstr) {
    again:;
    if (tcdata[0].type != PB_COMPILER_TOK_TYPE_ID) {
        pb_compitf_puterrln(pbc, PB_ERROR_SYNTAX, "Expected identifier", &tcdata[0].loc);
        return PB_ERROR_SYNTAX;
    }
    if (tclen > 1) {
        if (tcdata[1].type == PB_COMPILER_TOK_TYPE_SYM && tcdata[1].subtype == PB_COMPILER_TOK_SUBTYPE_SYM_COLON) {
            // TODO: create label
            tclen -= 2;
            if (!tclen) return PB_ERROR_NONE;
            tcdata += 2;
            goto again;
        }
        size_t i = 1;
        while (1) {
            struct pb_compiler_tok* t = &tcdata[i];
            if (t->type == PB_COMPILER_TOK_TYPE_SYM) {
                if (t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_DOLLAR) goto exprdetectcont;
                if (t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_DOT) {
                    if (i == tclen - 1) break;
                    ++i;
                    ++t;
                    if (t->type == PB_COMPILER_TOK_TYPE_ID) goto exprdetectcont;
                }
                if (t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_PERCEQ ||
                    t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_AMPEQ ||
                    t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_ASTEQ ||
                    t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_PLUSEQ ||
                    t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_DASHEQ ||
                    t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_LTLTEQ ||
                    t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_LTLTLTEQ ||
                    t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_EQ ||
                    t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_GTGTEQ ||
                    t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_GTGTGTEQ ||
                    t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_CARETEQ ||
                    t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_BAREQ) goto isexpr;
            }
            break;
            exprdetectcont:;
            ++i;
            if (i == tclen) break;
        }
    }
    char* id = tcstr->data + tcdata[0].id;
    // TODO: detect '.'
    if (id[0] != ':') {
        char* ns;
        char* name = NULL;
        for (char* tmp = id + 1; *tmp; ++tmp) {
            if (*tmp == ':') name = tmp;
        }
        if (!name) {
            ns = NULL;
            name = id;
            // TODO: search builtins
        } else {
            ns = id;
            *name++ = 0;
        }
        // TODO: search in curns for cmd then sub
        // TODO: search in using for cmd then sub
        // TODO: search in parent namespaces for cmd then sub
        if (ns) *--name = ':';
    } else {
        ++id;
        // TODO: search from root for cmd then sub
    }
    pb_compitf_puterr(pbc, PB_ERROR_DEF, "Command or subroutine '", &tcdata[0].loc); // TODO: detect '.' and generate correct error
    cb_addstr(pbc->err, id);
    cb_addstr(pbc->err, "' not found\n");
    return PB_ERROR_DEF;

    isexpr:;
    return PB_ERROR_NONE;
}

#if DEBUG(1)
static void dbg_printtok(struct pb_compiler_tokcoll* tc) {
    if (!tc->len) {
        puts("---");
        return;
    }
    static const char* const symstr[PB_COMPILER_TOK_SUBTYPE_SYM__COUNT] = {
        "!", "!=",
        "$",
        "%", "%=",
        "&", "&&", "&=",
        "(",
        ")",
        "*", "*=",
        "+", "+=",
        ",",
        "-", "-=",
        ".", "...",
        "/",
        ":",
        "<", "<<", "<=", "<>", "<<<", "<<=", "<<<=",
        "=", "==",
        ">", ">>", ">=", ">>>", ">>=", ">>>=",
        "?",
        "@",
        "[",
        "]",
        "^", "^=",
        "{",
        "|", "||", "|=",
        "}",
        "~", "~="
    };
    static const char* const datastr[PB_COMPILER_TOK_SUBTYPE_DATA__COUNT] = {
        "STR",
        "BOOL",
        "I8", "I16", "I32", "I64",
        "U8", "U16", "U32", "U64",
        "F32", "F64",
        "I8:RAW", "I16:RAW", "I32:RAW", "I64:RAW",
        "U8:RAW", "U16:RAW", "U32:RAW", "U64:RAW",
        "F32:RAW", "F64:RAW"
    };
    size_t i = 0;
    while (1) {
        struct pb_compiler_tok* t = &tc->data[i];
        switch (t->type) {
            case PB_COMPILER_TOK_TYPE_ID:
                printf("ID(%u:%u){%s}", t->loc.line, t->loc.col, tc->strings.data + t->id);
                break;
            case PB_COMPILER_TOK_TYPE_SYM:
                printf("SYM(%u:%u){%s}", t->loc.line, t->loc.col, symstr[t->subtype]);
                break;
            case PB_COMPILER_TOK_TYPE_DATA:
                if (t->subtype == PB_COMPILER_TOK_SUBTYPE_DATA_STR) {
                    char* s = tc->strings.data + t->data.str.off;
                    printf("DATA(%u:%u):%s{", t->loc.line, t->loc.col, datastr[t->subtype]);
                    for (size_t i = 0; i < t->data.str.len; ++i) {
                        char c = s[i];
                        if (!c) {
                            putchar('}');
                            putchar('\\');
                            putchar('0');
                            putchar('{');
                        } else if (c == '\n') {
                            putchar('}');
                            putchar('\\');
                            putchar('n');
                            putchar('{');
                        } else if (c == '\r') {
                            putchar('}');
                            putchar('\\');
                            putchar('r');
                            putchar('{');
                        } else {
                            putchar(s[i]);
                        }
                    }
                    putchar('}');
                } else if (t->subtype >= PB_COMPILER_TOK_SUBTYPE_DATA_I8_RAW && t->subtype <= PB_COMPILER_TOK_SUBTYPE_DATA_F64_RAW) {
                    printf("DATA(%u:%u):%s{%s}", t->loc.line, t->loc.col, datastr[t->subtype], tc->strings.data + t->data_raw);
                } else {
                    printf("DATA(%u:%u):%s{", t->loc.line, t->loc.col, datastr[t->subtype]);
                    switch (t->subtype) {
                        DEFAULTCASE(PB_COMPILER_TOK_SUBTYPE_DATA_BOOL): fputs(t->data.b ? "TRUE" : "FALSE", stdout); break;
                        case PB_COMPILER_TOK_SUBTYPE_DATA_I8: printf("%d", (int)t->data.i8); break;
                        case PB_COMPILER_TOK_SUBTYPE_DATA_I16: printf("%d", (int)t->data.i16); break;
                        case PB_COMPILER_TOK_SUBTYPE_DATA_I32: printf("%d", (int)t->data.i32); break;
                        case PB_COMPILER_TOK_SUBTYPE_DATA_I64: printf("%lld", (long long int)t->data.i64); break;
                        case PB_COMPILER_TOK_SUBTYPE_DATA_U8: printf("%u", (unsigned)t->data.u8); break;
                        case PB_COMPILER_TOK_SUBTYPE_DATA_U16: printf("%u", (unsigned)t->data.u16); break;
                        case PB_COMPILER_TOK_SUBTYPE_DATA_U32: printf("%u", (unsigned)t->data.u32); break;
                        case PB_COMPILER_TOK_SUBTYPE_DATA_U64: printf("%llu", (long long unsigned)t->data.u64); break;
                        case PB_COMPILER_TOK_SUBTYPE_DATA_F32: printf("%f", (double)t->data.f32); break;
                        case PB_COMPILER_TOK_SUBTYPE_DATA_F64: printf("%f", (double)t->data.f64); break;
                    }
                    putchar('}');
                }
                break;
        }
        ++i;
        if (i == tc->len) break;
        putchar(' ');
    }
    putchar('\n');
}
#endif
enum pb_error pb_prog_compile(struct pbasic* pb, struct datastream* ds, const char* type, const struct pb_compiler_opt* opt, uint32_t* progidout, struct charbuf* err) {
    if (!opt) opt = &defaultcompopt;
    struct pb_compiler pbc = {.pb = pb, .opt = opt, .err = err};
    enum pb_error e = PB_ERROR_NONE;
    //e = PB_ERROR_INTERNAL;
    pb_compitf_puterrln(&pbc, PB_ERROR_NONE, "Compiler is under development", NULL);

    VLB_INIT(pbc.prevstreams, 1, goto emem;);

    VLB_INIT(pbc.sources, 2, goto emem;);
    if (!cb_init(&pbc.sources.names, 64)) goto emem;

    VLB_INIT(pbc.comptok, 64, goto emem;);
    if (!cb_init(&pbc.comptok.strings, 256)) goto emem;
    VLB_INIT(pbc.comptok.brackets, 8, goto emem;);

    VLB_INIT(pbc.preproctok, 32, goto emem;);
    if (!cb_init(&pbc.preproctok.strings, 256)) goto emem;
    VLB_INIT(pbc.preproctok.brackets, 8, goto emem;);

    if (!cb_addstr(&pbc.sources.names, ds->name) || !cb_add(&pbc.sources.names, 0)) goto emem;
    VLB_ADD(pbc.sources, (struct pb_compiler_source){.type = (type) ? type : "file"}, 3, 2, goto emem;);
    pbc.stream = (struct pb_compiler_stream){.ds = ds, .line = 1, .col = 1};

    while (1) {
        int c;
        //struct pb_compiler_srcloc el;

        do {
            c = pb__compiler_getc(&pbc);
        } while (c == ' ' || c == '\t');
        //printf("{%d %c}\n", c, c);
        if (c == -1) break;

        if (c == '#') {
            pb_compitf_mksrcloc(&pbc, 0, &pbc.comptok.preprocinsloc);
            do {
                c = pb__compiler_getc(&pbc);
            } while (c == ' ' || c == '\t');
            if (c == '\n') continue;
            if (c == -1) break;
            if (c == '(') {
                pbc.comptok.preprocinsstage = '(';
                goto notpreproc;
            }
            int r = pb__tokenize(&pbc, &pbc.preproctok, true, &c, &e);
            if (r == -1) goto reterr;
            //if (pbc.comptok.preprocinsstage) goto ebadins;
            #if DEBUG(1)
            printf("PREPROC[%zu]: ", pbc.preproctok.len);
            dbg_printtok(&pbc.preproctok);
            #endif
            if (pbc.preproctok.len) {
                e = pb__evalpreproccmd(&pbc, &pbc.preproctok);
                if (e) goto reterr;
                pbc.preproctok.len = 0;
                pbc.preproctok.strings.len = 0;
            }
        } else if (c == '\n' || c == ';' || c == -1) {
            endstmt:;
            if (pbc.comptok.preprocinsstage) {
                //ebadins:;
                pb_compitf_puterrln(&pbc, (e = PB_ERROR_SYNTAX), "Incomplete preprocessor variable insertion", &pbc.comptok.preprocinsloc);
                goto reterr;
            }
            #if DEBUG(1)
            printf("COMPILER[%zu]: ", pbc.comptok.len);
            dbg_printtok(&pbc.comptok);
            #endif
            if (pbc.comptok.len) {
                e = pb__evalcmd(&pbc, pbc.comptok.data, pbc.comptok.len, &pbc.comptok.strings);
                if (e) goto reterr;
                pbc.comptok.len = 0;
                pbc.comptok.strings.len = 0;
            }
        } else {
            notpreproc:;
            int r = pb__tokenize(&pbc, &pbc.comptok, false, &c, &e);
            if (r == 1) goto endstmt;
            if (r == -1) goto reterr;
        }
    }

    goto ret;

    emem:;
    pb_compitf_puterrln(&pbc, (e = PB_ERROR_MEMORY), NULL, NULL);
    reterr:;

    ret:;

    VLB_FREE(pbc.preproctok.brackets);
    cb_dump(&pbc.preproctok.strings);
    VLB_FREE(pbc.preproctok);

    VLB_FREE(pbc.comptok.brackets);
    cb_dump(&pbc.comptok.strings);
    VLB_FREE(pbc.comptok);

    VLB_FREE(pbc.sources.names);
    VLB_FREE(pbc.sources);

    VLB_FREE(pbc.prevstreams);

    return e;
}

void pb_prog_destroy(struct pbasic* pb, uint32_t progid) {

}
