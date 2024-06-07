#include "pbvm.h"

#include <string.h>

#include "../glue.h"

static inline bool pbc_open(const char* p, struct pbc_stream* s) {
    if (!(s->f = fopen(p, "r"))) return false;
    s->line = 1;
    s->col = 1;
    s->lastline = 1;
    s->lastcol = 1;
    s->undo = false;
    return true;
}
static inline int pbc_getc(struct pbc_stream* s) {
    int c;
    if (s->undo) {
        c = s->last;
        s->undo = false;
    } else {
        c = fgetc(s->f);
        s->last = c;
    }
    if (c == '\n') {
        s->lastline = s->line++;
        s->lastcol = s->col;
        s->col = 0;
    } else {
        s->lastline = s->line;
        s->lastcol = s->col++;
    }
    //printf("(%d:%d): %d\n", s->line, s->col, c);
    return c;
}
static inline void pbc_ungetc(struct pbc_stream* s) {
    s->undo = true;
    s->line = s->lastline;
    s->col = s->lastcol;
}
static inline int pbc_getc_escnl(struct pbc_stream* s, int f) {
    int c = pbc_getc(s);
    if (c == '\\') {
        c = fgetc(s->f);
        if (c == '\n') {
            if (f) {
                s->last = f;
                return f;
            }
            return pbc_getc(s);
        } else {
            ungetc(c, s->f);
        }
    }
    return c;
}

static void pbc_mkerr(struct pbc* ctx, struct charbuf* err, unsigned l, unsigned c, const char* t) {
    char tmp[32];
    sprintf(tmp, "(%u:%u): ", (l) ? l : ctx->input.line, (c) ? c : ctx->input.col);
    cb_addstr(err, tmp);
    cb_addstr(err, t);
}

static inline bool pbc_iskwchar(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static inline int pbc_getkw_inline(struct pbc* ctx, struct charbuf* cb) {
    
}
static inline int pbc_getsep_inline(struct pbc* ctx, const char* chars) {
    
}

static int pbc_getcmd(struct pbc*, struct charbuf*, const char*, unsigned, unsigned, bool, bool);

static int pbc_getexpr(struct pbc*, struct charbuf*);
static inline int pbc_getexpr_inline(struct pbc* ctx, struct charbuf* err) {
    
}
static int pbc_getexpr(struct pbc* ctx, struct charbuf* err) {
    pbc_getexpr_inline(ctx, err);
}

static int pbc_getcond(struct pbc*, struct charbuf*);
static inline int pbc_getcond_inline(struct pbc* ctx, struct charbuf* err) {
    
}
static int pbc_getcond(struct pbc* ctx, struct charbuf* err) {
    pbc_getcond_inline(ctx, err);
}

static int pbc_getcmd(struct pbc* ctx, struct charbuf* err, const char* name, unsigned nl, unsigned nc, bool func, bool useret) {
    int c = name[0];
    if (c >= 'A' || c <= 'Z') c |= 0x20;
    const char* rname = name + 1;
    if (func) {
        
    } else {
        switch (name[0]) {
            case 'i': {
                if (!strcasecmp(rname, "f")) {
                    if (pbc_getcond_inline(ctx, err)) {
                        return 1;
                    } else {
                        return 0;
                    }
                }
            } break;
        }
    }
    if (err) {
        pbc_mkerr(ctx, err, nl, nc, "Could not find ");
        cb_addstr(err, (func) ? "function" : "command");
        cb_add(err, ' ');
        cb_add(err, '\'');
        cb_addstr(err, name);
        cb_add(err, '\'');
    }
    return 0;
}

struct pbc_preproc_cond {
    int l, c;
    uint8_t read : 1;
    uint8_t done : 1;
    uint8_t iselse : 1;
};
struct pbc_preproc {
    struct pbc_preproc_cond* data;
    int index;
    int fakedepth;
    int size;
};

enum __attribute__((packed)) pbc_peop {
    PBC_PEOP_ADD,
    PBC_PEOP_SUB,
    PBC_PEOP_MUL,
    PBC_PEOP_DIV,
    PBC_PEOP_REM,
    PBC_PEOP_GT,
    PBC_PEOP_GE,
    PBC_PEOP_EQ,
    PBC_PEOP_LE,
    PBC_PEOP_LT,
    PBC_PEOP_AND,
    PBC_PEOP_OR,
    PBC_PEOP_NOT,
    PBC_PEOP_NEG,
    PBC_PEOP_BAND,
    PBC_PEOP_BOR,
    PBC_PEOP_BXOR,
    PBC_PEOP_BNOT,
    PBC_PEOP_LP,
    PBC_PEOP_RP,
    PBC_PEOP__COUNT,
};
static struct {
    uint8_t l : 1; // is left-to-right
    uint8_t p : 7; // precedence (lower val = higher place)
} pbc_peop_info[PBC_PEOP__COUNT] = {
    {.p = 0, .l = 1}
};
struct pbc_pexpr {
    struct {
        int* data;
        int index;
        int size;
    } data;
    struct {
        char* data;
        int index;
        int size;
    } ops;
    int parenct;
};
static int pbc_getpexpr(struct pbc* ctx, struct charbuf* err) {
    int retval;
    struct pbc_pexpr expr;
    expr.data.size = 16;
    expr.data.index = 0;
    expr.data.data = malloc(expr.data.size * sizeof(*expr.data.data));
    expr.ops.size = 16;
    expr.ops.index = 0;
    expr.ops.data = malloc(expr.ops.size * sizeof(*expr.ops.data));
    struct charbuf cb;
    cb_init(&cb, 256);
    int c = pbc_getc_escnl(&ctx->input, 0);
    while (1) {
        bool isnum;
        if (c >= '0' && c <= '9') {
            isnum = true;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            isnum = false;
        } else if (c == '(') {
            ++expr.parenct;
//            pbc_pexpr_pushop(&expr, '(');
            goto again;
        } else if (c == '-') {
//            pbc_pexpr_pushop(&expr, 'n');
            goto again;
        } else if (c == '!' || c == '-' || c == '~') {
//            pbc_pexpr_pushop(&expr, c);
            goto again;
        } else {
            if (err) {
                if (c == '\n' || c == EOF) pbc_mkerr(ctx, err, ctx->input.lastline, ctx->input.lastcol + 1, "Syntax error (expected name or value)");
                else pbc_mkerr(ctx, err, 0, 0, "Syntax error (unexpected char)");
            }
            retval = -1;
            goto ret;
        }
        int nl = ctx->input.line, nc = ctx->input.col;
        int d;
        if (isnum) {
            cb_add(&cb, c);
            c = pbc_getc_escnl(&ctx->input, ' ');
            while (1) {
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {isnum = false; goto change;}
                if (c >= '0' && c <= '9') {
                    cb_add(&cb, c);
                    c = pbc_getc_escnl(&ctx->input, ' ');
                } else {
                    break;
                }
            }
            while (c == ' ' || c == '\t') {
                c = pbc_getc_escnl(&ctx->input, 0);
            }
            printf("number: {%s}\n", cb_peek(&cb));
            d = atoi(cb_peek(&cb));
        } else {
            change:;
            do {
                cb_add(&cb, c);
                c = pbc_getc_escnl(&ctx->input, ' ');
            } while (pbc_iskwchar(c));
            while (c == ' ' || c == '\t') {
                c = pbc_getc_escnl(&ctx->input, 0);
            }
            if (c == '(') {
                cb_nullterm(&cb);
                printf("func: {%s}\n", cb.data);
                // TODO: decrust
                if (!strcasecmp(cb.data, "defined")) {
                    do {
                        c = pbc_getc_escnl(&ctx->input, 0);
                    } while (c == ' ' || c == '\t');
                    nl = ctx->input.line;
                    nc = ctx->input.col;
                    cb_clear(&cb);
                    isnum = true;
                    while (1) {
                        if (c >= '0' && c <= '9') {
                            cb_add(&cb, c);
                        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
                            isnum = false;
                            cb_add(&cb, c);
                        } else {
                            break;
                        }
                        c = pbc_getc_escnl(&ctx->input, ' ');
                    }
                    if (isnum) {
                        if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error (expected name)");
                        retval = -1;
                        goto ret;
                    }
                    while (c == ' ' || c == '\t') {
                        c = pbc_getc_escnl(&ctx->input, 0);
                    }
                    if (c != ')') {
                        if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error (unexpected char)");
                        retval = -1;
                        goto ret;
                    }
                    cb_nullterm(&cb);
                    printf("defined: {%s}\n", cb.data);
                    d = (ctx->opt->findpv && ctx->opt->findpv(cb.data, &d));
                } else {
                    if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error (unknown preprocessor function '");
                    cb_addstr(err, cb.data);
                    cb_addstr(err, "')");
                    retval = -1;
                    goto ret;
                }
            } else {
                printf("var: {%s}\n", cb_peek(&cb));
                cb_nullterm(&cb);
                if (!ctx->opt->findpv || !ctx->opt->findpv(cb.data, &d)) {
                    if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error ('");
                    cb_addstr(err, cb.data);
                    cb_addstr(err, "' is not defined)");
                    retval = -1;
                    goto ret;
                }
            }
        }
//        pbc_pexpr_pushdata(&expr, d);
        cb_clear(&cb);
        again:;
        c = pbc_getc_escnl(&ctx->input, 0);
        while (c == ' ' || c == '\t') {
            c = pbc_getc_escnl(&ctx->input, 0);
        }
    }
    ret:;
    cb_dump(&cb);
    free(expr.data.data);
    free(expr.ops.data);
    return retval;
}

static inline bool pbc_getpcmd(struct pbc* ctx, struct pbc_preproc* pctx, struct charbuf* err, const char* name, unsigned nl, unsigned nc) {
    printf("pbc_getpcmd: {%s}\n", name);
    if (!strcasecmp(name, "if")) {
        if (pctx->data[pctx->index].read) {
            int r = pbc_getpexpr(ctx, err);
            if (r == -1) return false;
            ++pctx->index;
            if (pctx->index == pctx->size) {
                pctx->size = pctx->size * 3 / 2;
                //if (pctx->size < 2) pctx->size = 2;
                pctx->data = realloc(pctx->data, pctx->size * sizeof(*pctx->data));
            }
            pctx->data[pctx->index] = (struct pbc_preproc_cond){.read = r, .done = r, .l = nl, .c = nc};
        } else {
            int c;
            do {
                c = pbc_getc_escnl(&ctx->input, 0);
            } while (c != '\n' && c != EOF);
            if (c == EOF) pbc_ungetc(&ctx->input);
            ++pctx->fakedepth;
        }
        return true;
    } else if (!strcasecmp(name, "elif")) {
        if (pctx->fakedepth) {
            int c;
            do {
                c = pbc_getc_escnl(&ctx->input, 0);
            } while (c != '\n' && c != EOF);
            if (c == EOF) pbc_ungetc(&ctx->input);
        } else {
            if (pctx->index < 1) {
                if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error (#else without #if)");
                return false;
            }
            if (pctx->data[pctx->index].iselse) {
                if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error (#elif after #else)");
                return false;
            }
            if (pctx->data[pctx->index].done) {
                pctx->data[pctx->index].read = 0;
                int c;
                do {
                    c = pbc_getc_escnl(&ctx->input, 0);
                } while (c != '\n' && c != EOF);
                if (c == EOF) pbc_ungetc(&ctx->input);
            } else {
                int r = pbc_getpexpr(ctx, err);
                if (r == -1) return false;
                if (r) {
                    pctx->data[pctx->index].read = 1;
                    pctx->data[pctx->index].done = 1;
                }
            }
        }
        return true;
    } else if (!strcasecmp(name, "else")) {
        int c;
        do {
            c = pbc_getc_escnl(&ctx->input, 0);
        } while (c == ' ' || c == '\t');
        if (c != '\n' && c != EOF) {
            if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error (unexpected character)");
            return false;
        }
        if (!pctx->fakedepth) {
            if (pctx->index < 1) {
                if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error (#else without #if)");
                return false;
            }
            if (pctx->data[pctx->index].iselse) {
                if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error (extra #else)");
                return false;
            }
            if (pctx->data[pctx->index].done) {
                pctx->data[pctx->index].read = 0;
            } else {
                pctx->data[pctx->index].read = !pctx->data[pctx->index].read;
            }
            pctx->data[pctx->index].iselse = 1;
        }
        return true;
    } else if (!strcasecmp(name, "endif")) {
        int c;
        do {
            c = pbc_getc_escnl(&ctx->input, 0);
        } while (c == ' ' || c == '\t');
        if (c != '\n' && c != EOF) {
            if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error (unexpected character)");
            return false;
        }
        if (pctx->fakedepth) {
            --pctx->fakedepth;
        } else {
            --pctx->index;
            if (pctx->index < 0) {
                if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error (#endif without matching #if)");
                return false;
            }
        }
        return true;
    } else if (!pctx->data[pctx->index].read) {
        int c;
        do {
            c = pbc_getc_escnl(&ctx->input, 0);
        } while (c != '\n' && c != EOF);
        return true;
    }
    if (err) {
        pbc_mkerr(ctx, err, nl, nc, "Unknown preprocessor directive '");
        cb_addstr(err, name);
        cb_add(err, '\'');
    }
    return false;
}

bool pb_compilefile(const char* p, struct pbc_opt* o, struct pb_script* out, struct charbuf* err) {
    struct pbc ctx;
    if (!pbc_open(p, &ctx.input)) {
        if (err) cb_addstr(err, "Failed to open input");
        return false;
    }
    ctx.opt = o;
    ctx.scopes.size = 4;
    ctx.ops.size = 256;
    ctx.constdata.size = 32;
    ctx.consts.size = 4;
    ctx.vars.size = 4;
    ctx.subs.size = 4;
    struct pbc_preproc pctx;
    pctx.size = 4;
    pctx.index = 0;
    pctx.fakedepth = 0;
    pctx.data = malloc(pctx.size * sizeof(*pctx.data));
    pctx.data[0] = (struct pbc_preproc_cond){.read = 1};
    bool retval = true;
    struct charbuf cb;
    cb_init(&cb, 256);
    while (1) {
        int c;
        do {
            c = pbc_getc(&ctx.input);
        } while (c == ' ' || c == '\t' || c == '\n' || c == ':');
        if (c == '\'') {
            do {
                c = pbc_getc(&ctx.input);
            } while (c != '\n');
        } else if (c == '#') {
            do {
                c = pbc_getc_escnl(&ctx.input, 0);
            } while (c == ' ' || c == '\t');
            if (!pbc_iskwchar(c)) {
                if (err) pbc_mkerr(&ctx, err, 0, 0, "Syntax error (unexpected char)");
                retval = false;
                goto ret;
            }
            int nl = ctx.input.line, nc = ctx.input.col;
            while (pbc_iskwchar(c)) {
                cb_add(&cb, c);
                c = pbc_getc_escnl(&ctx.input, ' ');
            }
            if (c == ' ' || c == '\t' || c == '\n') {
                while (c == ' ' || c == '\t') {
                    c = pbc_getc_escnl(&ctx.input, 0);
                }
                pbc_ungetc(&ctx.input);
                if (!pbc_getpcmd(&ctx, &pctx, err, cb_peek(&cb), nl, nc)) {
                    retval = false;
                    goto ret;
                }
                cb_clear(&cb);
            } else {
                if (err) {
                    pbc_mkerr(&ctx, err, 0, 0, "Syntax error (");
                    cb_addstr(err, (c == EOF) ? "expected preprocessor command" : "unexpected char");
                    cb_add(err, ')');
                }
                retval = false;
                goto ret;
            }
        } else if (!pctx.data[pctx.index].read) {
            if (c == EOF) break;
            do {
                c = pbc_getc_escnl(&ctx.input, 0);
            } while (c != '\n' && c != EOF);
            if (c == EOF) break;
        } else if (pbc_iskwchar(c)) {
            int nl = ctx.input.line, nc = ctx.input.col;
            do {
                cb_add(&cb, c);
                c = pbc_getc_escnl(&ctx.input, ' ');
            } while (pbc_iskwchar(c));
            while (c == ' ' || c == '\t') {
                c = pbc_getc_escnl(&ctx.input, 0);
            }
            if (c == '=') {
                do {
                    c = pbc_getc_escnl(&ctx.input, 0);
                } while (c == ' ' || c == '\t');
                //pbc_getexpr(&ctx, NULL);
                //pbc_put8(&ctx, PBVM_OP_SET);
                //pbc_put32(&ctx, pbc_getvar(&ctx, cb_peek(&cb)));
                cb_clear(&cb);
                c = pbc_getc(&ctx.input);
                if (c == EOF) {
                    break;
                } else if (c != '\n' && c != ':') {
                    if (err) pbc_mkerr(&ctx, err, 0, 0, "Syntax error (expected ':' or '\\n')");
                    retval = false;
                    goto ret;
                }
            } else {
                bool func = (c == '(');
                if (func) {
                    do {
                        c = pbc_getc_escnl(&ctx.input, 0);
                    } while (c == ' ' || c == '\t');
                }
                pbc_ungetc(&ctx.input);
                if (!pbc_getcmd(&ctx, err, cb_peek(&cb), nl, nc, func, false)) {
                    retval = false;
                    goto ret;
                }
                cb_clear(&cb);
                c = pbc_getc(&ctx.input);
                if (c == EOF) {
                    break;
                } else if (c != '\n' && c != ':') {
                    if (err) pbc_mkerr(&ctx, err, 0, 0, "Syntax error (expected ':' or '\\n')");
                    retval = false;
                    goto ret;
                }
            }
        } else if (c == EOF) {
            break;
        } else {
            if (err) pbc_mkerr(&ctx, err, 0, 0, "Syntax error (unexpected char)");
            retval = false;
            goto ret;
        }
    }
    //longbreak:;
    if (pctx.index) {
        if (err) pbc_mkerr(&ctx, err, pctx.data[pctx.index].l, pctx.data[pctx.index].c, "Syntax error (#if without #endif)");
        retval = false;
        //goto ret;
    }
    ret:;
    free(pctx.data);
    cb_dump(&cb);
    return retval;
}

void pb_deletescript(struct pb_script* s) {
    (void)s;
}
