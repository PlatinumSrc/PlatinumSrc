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
    PBC_PEOP__INVAL = -1,
    PBC_PEOP_LP, // 0
    PBC_PEOP_RP,
    PBC_PEOP_NEG, // 1
    PBC_PEOP_NOT,
    PBC_PEOP_BNOT,
    PBC_PEOP_MUL, // 2
    PBC_PEOP_DIV,
    PBC_PEOP_REM,
    PBC_PEOP_ADD, // 3
    PBC_PEOP_SUB,
    PBC_PEOP_BLS, // 4
    PBC_PEOP_BRS,
    PBC_PEOP_GT, // 5
    PBC_PEOP_GE,
    PBC_PEOP_LE,
    PBC_PEOP_LT,
    PBC_PEOP_EQ, // 6
    PBC_PEOP_NE,
    PBC_PEOP_BAND, // 7
    PBC_PEOP_BXOR, // 8
    PBC_PEOP_BOR, // 9
    PBC_PEOP_AND, // 10
    PBC_PEOP_OR, // 11
    PBC_PEOP__COUNT
};
static const struct pbc_peopinfo {
    uint8_t r : 1; // is right-to-left
    uint8_t p : 7; // precedence (lower val = higher place)
} peop_info[PBC_PEOP__COUNT] = {
    {.p = 0},
    {.p = 0},
    {.p = 1, .r = 1},
    {.p = 1, .r = 1},
    {.p = 1, .r = 1},
    {.p = 2},
    {.p = 2},
    {.p = 2},
    {.p = 3},
    {.p = 3},
    {.p = 4},
    {.p = 4},
    {.p = 5},
    {.p = 5},
    {.p = 5},
    {.p = 5},
    {.p = 6},
    {.p = 6},
    {.p = 7},
    {.p = 8},
    {.p = 9},
    {.p = 10},
    {.p = 11}
};
struct pbc_pedata {
    bool isop;
    int data;
};
struct pbc_pexpr {
    struct {
        enum pbc_peop* data;
        int index;
        int size;
    } ops;
    struct {
        struct pbc_pedata* data;
        int index;
        int size;
    } out;
};
static inline void pbc_pexpr_pushout(struct pbc_pexpr* e, bool isop, int v) {
    //printf("PUSH %d: %d\n", isop, v);
    if (e->out.index == e->out.size) {
        e->out.size = e->out.size * 3 / 2;
        //if (e->out.size < 2) e->out.size = 2;
        e->out.data = realloc(e->out.data, e->out.size * sizeof(*e->out.data));
    }
    e->out.data[e->out.index++] = (struct pbc_pedata){.isop = isop, .data = v};
}
static inline void pbc_pexpr_pushop(struct pbc_pexpr* e, enum pbc_peop o) {
    if (e->ops.index == e->ops.size) {
        e->ops.size = e->ops.size * 3 / 2;
        //if (e->ops.size < 2) e->ops.size = 2;
        e->ops.data = realloc(e->ops.data, e->ops.size * sizeof(*e->ops.data));
    }
    e->ops.data[e->ops.index++] = o;
}
static void pbc_pexpr_op(struct pbc_pexpr* e, enum pbc_peop o) {
    if (o == PBC_PEOP_LP) {
        pbc_pexpr_pushop(e, PBC_PEOP_LP);
    } else if (o == PBC_PEOP_RP) {
        while (e->ops.index) {
            --e->ops.index;
            enum pbc_peop tmp = e->ops.data[e->ops.index];
            if (tmp == PBC_PEOP_LP) break;
            pbc_pexpr_pushout(e, true, tmp);
        }
    } else {
        struct pbc_peopinfo inf = peop_info[o];
        while (e->ops.index) {
            int i = e->ops.index - 1;
            enum pbc_peop o2 = e->ops.data[i];
            struct pbc_peopinfo inf2 = peop_info[o2];
            if (inf2.p < inf.p || (!inf.r && inf2.p == inf.p)) {
                pbc_pexpr_pushout(e, true, o2);
                e->ops.index = i;
            } else {
                break;
            }
        }
        pbc_pexpr_pushop(e, o);
    }
}
#define pbc_pexpr_data(e, d) pbc_pexpr_pushout(e, false, d)
static int pbc_getpexpr(struct pbc* ctx, struct charbuf* err) {
    int retval;
    struct pbc_pexpr expr;
    expr.out.size = 16;
    expr.out.index = 0;
    expr.out.data = malloc(expr.out.size * sizeof(*expr.out.data));
    expr.ops.size = 16;
    expr.ops.index = 0;
    expr.ops.data = malloc(expr.ops.size * sizeof(*expr.ops.data));
    int parenct = 0;
    struct charbuf cb;
    cb_init(&cb, 256);
    int c = pbc_getc_escnl(&ctx->input, 0);
    while (1) {
        while (c == ' ' || c == '\t') {
            c = pbc_getc_escnl(&ctx->input, 0);
        }
        bool isnum;
        if (c >= '0' && c <= '9') {
            isnum = true;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            isnum = false;
        } else {
            switch (c) {
                case '(': pbc_pexpr_op(&expr, PBC_PEOP_LP); ++parenct; break;
                case '!': pbc_pexpr_op(&expr, PBC_PEOP_NOT); break;
                case '-': pbc_pexpr_op(&expr, PBC_PEOP_NEG); break;
                case '~': pbc_pexpr_op(&expr, PBC_PEOP_BNOT); break;
                default: {
                    if (err) {
                        if (c == '\n' || c == EOF) pbc_mkerr(ctx, err, ctx->input.lastline, ctx->input.lastcol + 1, "Syntax error (expected name or value)");
                        else pbc_mkerr(ctx, err, 0, 0, "Syntax error (unexpected char)");
                    }
                    retval = -1;
                    goto ret;
                };
            }
            c = pbc_getc_escnl(&ctx->input, 0);
            continue;
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
            //printf("number: {%s}\n", cb_peek(&cb));
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
                //printf("func: {%s}\n", cb.data);
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
                    //printf("defined: {%s}\n", cb.data);
                    d = (ctx->opt->findpv && ctx->opt->findpv(cb.data, &d));
                } else {
                    if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error (unknown preprocessor function '");
                    cb_addstr(err, cb.data);
                    cb_addstr(err, "')");
                    retval = -1;
                    goto ret;
                }
                c = pbc_getc_escnl(&ctx->input, 0);
            } else {
                //printf("var: {%s}\n", cb_peek(&cb));
                cb_nullterm(&cb);
                if (!ctx->opt->findpv || !ctx->opt->findpv(cb.data, &d)) {
                    if (err) pbc_mkerr(ctx, err, nl, nc, "Preprocessor error ('");
                    cb_addstr(err, cb.data);
                    cb_addstr(err, "' is not defined)");
                    retval = -1;
                    goto ret;
                }
            }
        }
        pbc_pexpr_data(&expr, d);
        cb_clear(&cb);
        opagain:;
        switch (c) {
            case '\n':
                goto longbreak;
            case ')':
                if (!parenct) {
                    if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error (')' without matching '(')");
                    retval = -1;
                    goto ret;
                }
                pbc_pexpr_op(&expr, PBC_PEOP_RP);
                --parenct;
                do {
                    c = pbc_getc_escnl(&ctx->input, 0);
                } while (c == ' ' || c == '\t');
                goto opagain;
                break;
            case '+': pbc_pexpr_op(&expr, PBC_PEOP_ADD); break;
            case '-': pbc_pexpr_op(&expr, PBC_PEOP_SUB); break;
            case '*': pbc_pexpr_op(&expr, PBC_PEOP_MUL); break;
            case '/': pbc_pexpr_op(&expr, PBC_PEOP_DIV); break;
            case '%': pbc_pexpr_op(&expr, PBC_PEOP_REM); break;
            case '^': pbc_pexpr_op(&expr, PBC_PEOP_BXOR); break;
            case '=':
                c = pbc_getc_escnl(&ctx->input, ' ');
                if (c == '=') {
                    pbc_pexpr_op(&expr, PBC_PEOP_EQ);
                } else {
                    if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error (expected '=')");
                    retval = -1;
                    goto ret;
                }
                break;
            case '!':
                c = pbc_getc_escnl(&ctx->input, ' ');
                if (c == '=') {
                    pbc_pexpr_op(&expr, PBC_PEOP_NE);
                } else {
                    if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error (expected '=')");
                    retval = -1;
                    goto ret;
                }
                break;
            case '>':
                c = pbc_getc_escnl(&ctx->input, ' ');
                if (c == '=') pbc_pexpr_op(&expr, PBC_PEOP_GE);
                else if (c == '>') pbc_pexpr_op(&expr, PBC_PEOP_BRS);
                else {pbc_pexpr_op(&expr, PBC_PEOP_GT); continue;}
                break;
            case '<':
                c = pbc_getc_escnl(&ctx->input, ' ');
                if (c == '=') pbc_pexpr_op(&expr, PBC_PEOP_LE);
                else if (c == '<') pbc_pexpr_op(&expr, PBC_PEOP_BLS);
                else {pbc_pexpr_op(&expr, PBC_PEOP_LT); continue;}
                break;
            case '&':
                c = pbc_getc_escnl(&ctx->input, ' ');
                if (c == '&') pbc_pexpr_op(&expr, PBC_PEOP_AND);
                else {pbc_pexpr_op(&expr, PBC_PEOP_BAND); continue;}
                break;
            case '|':
                c = pbc_getc_escnl(&ctx->input, ' ');
                if (c == '|') pbc_pexpr_op(&expr, PBC_PEOP_OR);
                else {pbc_pexpr_op(&expr, PBC_PEOP_BOR); continue;}
                break;
            default:
                if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error (unexpected char)");
                retval = -1;
                goto ret;
                break;
        }
        c = pbc_getc_escnl(&ctx->input, 0);
    }
    longbreak:;
    if (parenct) {
        if (err) pbc_mkerr(ctx, err, ctx->input.lastline, ctx->input.lastcol + 1, "Syntax error (unterminated '(')");
        retval = -1;
        goto ret;
    }
    while (expr.ops.index) pbc_pexpr_pushout(&expr, true, expr.ops.data[--expr.ops.index]);
    free(expr.ops.data);
    struct {
        int* data;
        int index;
        int size;
    } stack;
    stack.size = 16;
    stack.index = 0;
    stack.data = malloc(stack.size * sizeof(*stack.data));
    for (int i = 0; i < expr.out.index; ++i) {
        struct pbc_pedata d = expr.out.data[i];
        //printf("READ %d: %d\n", d.isop, d.data);
        if (d.isop) {
            switch (d.data) {
                case PBC_PEOP_NEG: stack.data[stack.index - 1] *= -1; break;
                case PBC_PEOP_NOT: stack.data[stack.index - 1] = !stack.data[stack.index - 1]; break;
                case PBC_PEOP_BNOT: stack.data[stack.index - 1] = ~stack.data[stack.index - 1]; break;
                case PBC_PEOP_MUL: --stack.index; stack.data[stack.index - 1] *= stack.data[stack.index]; break;
                case PBC_PEOP_DIV: --stack.index; stack.data[stack.index - 1] /= stack.data[stack.index]; break;
                case PBC_PEOP_REM: --stack.index; stack.data[stack.index - 1] %= stack.data[stack.index]; break;
                case PBC_PEOP_ADD: --stack.index; stack.data[stack.index - 1] += stack.data[stack.index]; break;
                case PBC_PEOP_SUB: --stack.index; stack.data[stack.index - 1] -= stack.data[stack.index]; break;
                case PBC_PEOP_BLS: --stack.index; stack.data[stack.index - 1] <<= stack.data[stack.index]; break;
                case PBC_PEOP_BRS: --stack.index; stack.data[stack.index - 1] >>= stack.data[stack.index]; break;
                case PBC_PEOP_GT: --stack.index; stack.data[stack.index - 1] = (stack.data[stack.index - 1] > stack.data[stack.index]); break;
                case PBC_PEOP_GE: --stack.index; stack.data[stack.index - 1] = (stack.data[stack.index - 1] >= stack.data[stack.index]); break;
                case PBC_PEOP_LE: --stack.index; stack.data[stack.index - 1] = (stack.data[stack.index - 1] <= stack.data[stack.index]); break;
                case PBC_PEOP_LT: --stack.index; stack.data[stack.index - 1] = (stack.data[stack.index - 1] < stack.data[stack.index]); break;
                case PBC_PEOP_EQ: --stack.index; stack.data[stack.index - 1] = (stack.data[stack.index - 1] == stack.data[stack.index]); break;
                case PBC_PEOP_NE: --stack.index; stack.data[stack.index - 1] = (stack.data[stack.index - 1] != stack.data[stack.index]); break;
                case PBC_PEOP_BAND: --stack.index; stack.data[stack.index - 1] &= stack.data[stack.index]; break;
                case PBC_PEOP_BXOR: --stack.index; stack.data[stack.index - 1] ^= stack.data[stack.index]; break;
                case PBC_PEOP_BOR: --stack.index; stack.data[stack.index - 1] |= stack.data[stack.index]; break;
                case PBC_PEOP_AND: --stack.index; stack.data[stack.index - 1] = (stack.data[stack.index - 1] && stack.data[stack.index]); break;
                case PBC_PEOP_OR: --stack.index; stack.data[stack.index - 1] = (stack.data[stack.index - 1] || stack.data[stack.index]); break;
            }
        } else {
            if (stack.index == stack.size) {
                stack.size *= 2;
                stack.data = realloc(stack.data, stack.size * sizeof(*stack.data));
            }
            stack.data[stack.index++] = d.data;
        }
        #if 0
        printf("STACK:");
        for (int j = 0; j < stack.index; ++j) {
            printf(" %d", stack.data[j]);
        }
        putchar('\n');
        #endif
    }
    //puts("DONE");
    retval = stack.data[0];
    free(stack.data);
    ret:;
    cb_dump(&cb);
    free(expr.out.data);
    return retval;
}

static inline bool pbc_getpcmd(struct pbc* ctx, struct pbc_preproc* pctx, struct charbuf* err, const char* name, unsigned nl, unsigned nc) {
    //printf("pbc_getpcmd: {%s}\n", name);
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
