#include "../rcmgralloc.h"
#include "pbasic.h"
#include "../crc.h"
#include "../debug.h"

#include <string.h>
#if DEBUG(1)
    #include <stdio.h>
#endif

#include "../glue.h"

static enum pb_error pb__evalpreprocbuiltin(struct pb_compiler* pbc, const char* name, uint32_t namecrc, struct pb_compiler_tok* tcdata, size_t tclen, struct charbuf* tcstr, bool func, unsigned depth, struct pb_preproc_data* d) {
    struct pb_compiler_srcloc* el;

    switch (namecrc & 0xFF) {
        case 0x83:
            if (!func) {
                if (namecrc == 0x0E2F1883 && !strcasecmp(name, "let")) {
                    if (!tclen || (tclen && tcdata[0].type == PB_COMPILER_TOK_TYPE_SYM && tcdata[0].subtype == PB_COMPILER_TOK_SUBTYPE_SYM_COMMA)) {
                        pb_compitf_puterr(pbc, PB_ERROR_SYNTAX, "Expected expression after '", &tcdata[-1].loc);
                        cb_addstr(pbc->err, name);
                        cb_add(pbc->err, '\'');
                        cb_add(pbc->err, '\n');
                        return PB_ERROR_SYNTAX;
                    }
                    size_t tokct;
                    let_again:;
                    enum pb_error e = pb_compitf_evalpreprocexpr(pbc, tcdata, tclen, tcstr, 0, &tokct, NULL);
                    if (e != PB_ERROR_NONE) return e;
                    tclen -= tokct;
                    tcdata += tokct;
                    if (tclen) {
                        if (tcdata[0].type == PB_COMPILER_TOK_TYPE_SYM && tcdata[0].subtype == PB_COMPILER_TOK_SUBTYPE_SYM_COMMA) {
                            --tclen;
                            ++tcdata;
                            if (!tclen || (tclen && tcdata[0].type == PB_COMPILER_TOK_TYPE_SYM && tcdata[0].subtype == PB_COMPILER_TOK_SUBTYPE_SYM_COMMA)) {
                                pb_compitf_puterrln(pbc, PB_ERROR_SYNTAX, "Expected expression after ','", &tcdata[0].loc);
                                return PB_ERROR_SYNTAX;
                            }
                            goto let_again;
                        }
                        el = &tcdata[0].loc;
                        goto eexexprsep;
                    }
                    return PB_ERROR_NONE;
                }
            }
        default:
            break;
    }
    return -1;

    eexexprsep:;
    pb_compitf_puterrln(pbc, PB_ERROR_SYNTAX, "Expected ',' or end of statement", el);
    return PB_ERROR_SYNTAX;
}

enum pb_error pb_compitf_evalpreprocexpr(struct pb_compiler* pbc, struct pb_compiler_tok* tcdata, size_t tclen, struct charbuf* tcstr, unsigned depth, size_t* ctout, struct pb_preproc_data* d) {
    if (depth >= 32) goto emem;

    *ctout = 0;
    size_t oldtcl = tclen;
    enum pb_error e = PB_ERROR_NONE;
    enum pb_compiler_tok_type lasttype = PB_COMPILER_TOK_TYPE_SYM;

    PACKEDENUM opr {
        OPR_LPAREN = -2, OPR_RPAREN,
        OPR_UNARYPOS, OPR_UNARYNEG, OPR_LOGNOT, OPR_BITNOT, OPR_CAST,
        OPR_MUL, OPR_DIV, OPR_REM,
        OPR_ADD, OPR_SUB,
        OPR_BITAND,
        OPR_BITXOR,
        OPR_BITOR,
        OPR_LSHIFT, OPR_RSHIFT, OPR_LROT, OPR_RROT,
        OPR_LT, OPR_LE, OPR_GE, OPR_GT,
        OPR_EQ, OPR_NE, OPR_APEQ,
        OPR_LOGAND,
        OPR_LOGOR,
        OPR_ASSIGN,
        OPR_ASSIGNADD,
        OPR_ASSIGNSUB,
        OPR_ASSIGNMUL,
        OPR_ASSIGNDIV,
        OPR_ASSIGNREM,
        OPR_ASSIGNLSHIFT,
        OPR_ASSIGNRSHIFT,
        OPR_ASSIGNLROT,
        OPR_ASSIGNRROT,
        OPR_ASSIGNBITAND,
        OPR_ASSIGNBITXOR,
        OPR_ASSIGNBITOR,
        OPR__COUNT
    };
    static const struct {
        uint8_t prec : 7;
        uint8_t rtl : 1;
    } oprinfo[OPR__COUNT] = {
        {11, 1}, {11, 1}, {11, 1}, {11, 1}, {11, 1},
        {10, 0}, {10, 0}, {10, 0},
        {9, 0}, {9, 0},
        {8, 0},
        {7, 0},
        {6, 0},
        {5, 0}, {5, 0}, {5, 0}, {5, 0},
        {4, 0}, {4, 0}, {4, 0}, {4, 0},
        {3, 0}, {3, 0}, {3, 0},
        {2, 0},
        {1, 0},
        {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
        {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
        {0, 1}, {0, 1}, {0, 1},
    };
    struct oprstackelem {
        enum opr opr;
        struct {
            enum pb_preproc_type cast;
            size_t markerid;
        };
    };
    PACKEDENUM outtype {
        OUTTYPE_OPR,
        OUTTYPE_DATA,
        OUTTYPE_FUNC,
        OUTTYPE_VAR,
        OUTTYPE_LOGANDMARKER,
        OUTTYPE_LOGORMARKER
    };
    struct outqueueelem {
        enum outtype type;
        union {
            struct oprstackelem opr;
            size_t tok;
            size_t markerid;
        };
    };

    struct VLB(struct oprstackelem) oprstack;
    struct VLB(struct outqueueelem) outqueue;
    struct VLB(struct pb_preproc_data) datastack;

    VLB_INIT(oprstack, 4, oprstack.data = NULL; outqueue.data = NULL; datastack.data = NULL; goto emem;);
    VLB_INIT(outqueue, 4, outqueue.data = NULL; datastack.data = NULL; goto emem;);
    VLB_INIT(datastack, 4, datastack.data = NULL; goto emem;);

    int_fast8_t unarypn = -1;
    int_fast8_t lognot = -1;
    int_fast8_t bitnot = -1;
    size_t markerid = 1;
    for (size_t i = 0; i < tclen; ++i) {
        if (!tclen) break;
        if (tcdata[i].type == PB_COMPILER_TOK_TYPE_ID) {
            
        } else if (tcdata[i].type == PB_COMPILER_TOK_TYPE_DATA) {
            
        } else {
            enum opr opr;
            switch (tcdata[i].subtype) {
                case PB_COMPILER_TOK_SUBTYPE_SYM_COMMA: {
                    // TODO: bracket check
                } goto finish;
                default: {
                    pb_compitf_puterrln(pbc, (e = PB_ERROR_SYNTAX), "Symbol not understood by preprocessor", &tcdata[i].loc);
                } goto reterr;
            }
        }
    }
    finish:;
    goto retnoerr;

    retnoerr:;
    *ctout = oldtcl - tclen;
    goto ret;

    emem:;
    pb_compitf_puterrln(pbc, PB_ERROR_MEMORY, NULL, NULL);
    reterr:;

    ret:;
    VLB_FREE(oprstack);
    VLB_FREE(outqueue);
    VLB_FREE(datastack);
    return e;
}

static inline enum pb_error pb__evalpreprocaddoncmds(struct pb_compiler* pbc, const char* ns, uint32_t nsc, const char* n, uint32_t nc, struct pb_compiler_tok* tcd, size_t tcl, struct charbuf* tcs, bool func, unsigned depth, struct pb_preproc_data* d) {
    for (size_t ai = pbc->opt->addonct - 1; ai != (size_t)-1; --ai) {
        const struct pb_compiler_addon* a = &pbc->opt->addons[ai];
        if (a->preproccmd) {
            enum pb_error e = a->preproccmd(pbc, ns, nsc, n, nc, tcd, tcl, tcs, func, depth, d);
            if (e >= 0) return e;
        }
    }
    return -1;
}
enum pb_error pb__evalpreproccmd(struct pb_compiler* pbc, char* id, struct pb_compiler_tok* tcdata, size_t tclen, struct charbuf* tcstr, bool func, unsigned depth, struct pb_preproc_data* d) {
    if (!func) {
        if (tcdata[0].type != PB_COMPILER_TOK_TYPE_ID) {
            pb_compitf_puterrln(pbc, PB_ERROR_SYNTAX, "Expected identifier", &tcdata[0].loc);
            return PB_ERROR_SYNTAX;
        }
        id = tcstr->data + tcdata[0].id;
        --tclen;
        ++tcdata;
    }

    enum pb_error e;
    char* name = NULL;
    uint32_t namecrc;
    if (id[0] != ':') {
        for (char* tmp = id + 1; *tmp; ++tmp) {
            if (*tmp == ':') name = tmp;
        }
        if (!name) {
            namecrc = strcrc32(id);
            e = pb__evalpreprocbuiltin(pbc, id, namecrc, tcdata, tclen, tcstr, func, depth, d);
            if (e >= 0) return e;
            for (size_t si = pbc->scopes.len - 1; si != (size_t)-1; --si) {
                const struct pb_compiler_scope* scope = &pbc->scopes.data[si];
                for (size_t ui = scope->using.len - 1; ui != (size_t)-1; --ui) {
                    const struct pb_compiler_ns* nsptr = &pbc->namespaces.data[scope->using.data[ui]];
                    const char* nsstr = pbc->namespaces.strings.data + nsptr->fullnameoff;
                    e = pb__evalpreprocaddoncmds(pbc, nsstr, nsptr->fullnamecrc, id, namecrc, tcdata, tclen, tcstr, func, depth, d);
                    if (e >= 0) return e;
                }
            }
        } else {
            size_t nslen = strlen(id);
            struct charbuf cb;
            if (!cb_init(&cb, 16)) goto emem;
            *name++ = 0;
            namecrc = strcrc32(name);
            for (size_t si = pbc->scopes.len - 1; si != (size_t)-1; --si) {
                const struct pb_compiler_scope* scope = &pbc->scopes.data[si];
                for (size_t ui = scope->using.len - 1; ui != (size_t)-1; --ui) {
                    const struct pb_compiler_ns* nsptr = &pbc->namespaces.data[scope->using.data[ui]];
                    const char* nsstr = pbc->namespaces.strings.data + nsptr->fullnameoff;
                    if (!cb_addstr(&cb, nsstr) || !cb_add(&cb, ':') || !cb_addpartstr(&cb, id, nslen) || !cb_nullterm(&cb)) {
                        *--name = ':';
                        cb_dump(&cb);
                        goto emem;
                    }
                    e = pb__evalpreprocaddoncmds(pbc, cb.data, strcasecrc32(cb.data), name, namecrc, tcdata, tclen, tcstr, func, depth, d);
                    if (e >= 0) {
                        *--name = ':';
                        cb_dump(&cb);
                        return e;
                    }
                    cb_clear(&cb);
                }
            }
            *--name = ':';
            cb_dump(&cb);
        }
    } else {
        ++id;
        for (char* tmp = id + 1; *tmp; ++tmp) {
            if (*tmp == ':') name = tmp;
        }
        if (!name) {
            e = pb__evalpreprocaddoncmds(pbc, NULL, 0, id, strcrc32(id), tcdata, tclen, tcstr, func, depth, d);
            if (e >= 0) return e;
        } else {
            *name++ = 0;
            e = pb__evalpreprocaddoncmds(pbc, id, strcasecrc32(id), name, strcrc32(name), tcdata, tclen, tcstr, func, depth, d);
            if (e >= 0) {
                *--name = ':';
                return e;
            }
            *--name = ':';
        }
    }

    pb_compitf_puterr(pbc, PB_ERROR_DEF, "Preprocessor ", &tcdata[0].loc);
    cb_addstr(pbc->err, (!func) ? "command" : "function");
    cb_add(pbc->err, ' ');
    cb_add(pbc->err, '\'');
    cb_addstr(pbc->err, id);
    cb_addstr(pbc->err, "' not found\n");
    return PB_ERROR_DEF;

    emem:;
    pb_compitf_puterrln(pbc, PB_ERROR_DEF, NULL, NULL);
    return PB_ERROR_MEMORY;
}
