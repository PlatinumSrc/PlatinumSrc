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
    } else if (c == EOF || !c) {
        return EOF;
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
            s->lastline = s->line++;
            s->lastcol = s->col;
            s->col = 0;
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

static void pbc_mkerr(struct pbc* ctx, struct charbuf* err, unsigned l, unsigned c, const char* n, const char* d) {
    char tmp[64];
    cb_addstr(err, n);
    if (!l) {
        l = ctx->input.line;
        c = ctx->input.col;
    }
    if (c) sprintf(tmp, " at line %u column %u", l, c);
    else sprintf(tmp, " at end of line %u", l - 1);
    cb_addstr(err, tmp);
    if (d) {
        cb_add(err, ':');
        cb_add(err, ' ');
        cb_addstr(err, d);
    }
}

static FORCEINLINE bool pbc_iskwchar(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}
static FORCEINLINE bool pbc_iskwletter(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static FORCEINLINE bool pbc_isspace(int c) {
    return c == ' ' || c == '\t' /* || c == '\v' */;
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

PACKEDENUM(pbc_peop {
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
});
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
    //printf("OPPUSH: %d\n", o);
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
            if (o2 == PBC_PEOP_LP) break;
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
        while (pbc_isspace(c)) {
            c = pbc_getc_escnl(&ctx->input, 0);
        }
        if (c >= '0' && c <= '9') {
            if (c == '0') {
                c = pbc_getc_escnl(&ctx->input, ' ');
                if (c == 'x' || c == 'X') {
                    int d;
                    c = pbc_getc_escnl(&ctx->input, ' ');
                    if (c >= '0' && c <= '9') d = c - '0';
                    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
                    else goto readhex_skip;
                    while (1) {
                        c = pbc_getc_escnl(&ctx->input, ' ');
                        if (c >= '0' && c <= '9') {d <<= 4; d |= c - '0';}
                        else if (c >= 'a' && c <= 'f') {d <<= 4; d |= c - 'a' + 10;}
                        else if (c >= 'A' && c <= 'F') {d <<= 4; d |= c - 'A' + 10;}
                        else break;
                    }
                    pbc_pexpr_data(&expr, d);
                    readhex_skip:;
                } else if (c == 'b' || c == 'B') {
                    c = pbc_getc_escnl(&ctx->input, ' ');
                    if (c == '0' || c == '1') {
                        int d = c - '0';
                        while (1) {
                            c = pbc_getc_escnl(&ctx->input, ' ');
                            if (c == '0') {d <<= 4;}
                            else if (c == '1') {d <<= 4; d |= 1;}
                            else break;
                        }
                        pbc_pexpr_data(&expr, d);
                    }
                } else if (c >= '0' && c <= '7') {
                    int d = c - '0';
                    while (1) {
                        c = pbc_getc_escnl(&ctx->input, ' ');
                        if (c < '0' || c > '7') break;
                        d <<= 3;
                        d |= c - '0';
                    }
                    pbc_pexpr_data(&expr, d);
                } else {
                    pbc_pexpr_data(&expr, 0);
                }
            } else {
                int d = c - '0';
                while (1) {
                    c = pbc_getc_escnl(&ctx->input, ' ');
                    if (c < '0' || c > '9') break;
                    d *= 10;
                    d += c - '0';
                }
                pbc_pexpr_data(&expr, d);
            }
            while (pbc_isspace(c)) {
                c = pbc_getc_escnl(&ctx->input, 0);
            }
        } else if (pbc_iskwletter(c)) {
            unsigned nl = ctx->input.line, nc = ctx->input.col;
            int d;
            do {
                cb_add(&cb, c);
                c = pbc_getc_escnl(&ctx->input, ' ');
            } while (pbc_iskwchar(c));
            while (pbc_isspace(c)) {
                c = pbc_getc_escnl(&ctx->input, 0);
            }
            if (c == '(') {
                cb_nullterm(&cb);
                //printf("func: {%s}\n", cb.data);
                // TODO: decrust
                if (!strcasecmp(cb.data, "defined")) {
                    do {
                        c = pbc_getc_escnl(&ctx->input, 0);
                    } while (pbc_isspace(c));
                    nl = ctx->input.line;
                    nc = ctx->input.col;
                    cb_clear(&cb);
                    if (c >= '0' && c <= '9') {
                        if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error", "Expected name, given number");
                        retval = -1;
                        goto ret;
                    } else if (!pbc_iskwletter(c)) {
                        if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error", "Expected name");
                        retval = -1;
                        goto ret;
                    }
                    do {
                        cb_add(&cb, c);
                        c = pbc_getc_escnl(&ctx->input, ' ');
                    } while (pbc_iskwchar(c));
                    while (pbc_isspace(c)) {
                        c = pbc_getc_escnl(&ctx->input, 0);
                    }
                    if (c != ')') {
                        if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Unexpected character");
                        retval = -1;
                        goto ret;
                    }
                    cb_nullterm(&cb);
                    //printf("defined: {%s}\n", cb.data);
                    d = (ctx->opt->findpv && ctx->opt->findpv(cb.data, &d));
                } else {
                    if (err) pbc_mkerr(ctx, err, nl, nc, "Preprocessor error", "Unrecognized function '");
                    cb_addstr(err, cb.data);
                    cb_add(err, '\'');
                    retval = -1;
                    goto ret;
                }
                do {
                    c = pbc_getc_escnl(&ctx->input, 0);
                } while (pbc_isspace(c));
            } else {
                //printf("var: {%s}\n", cb_peek(&cb));
                cb_nullterm(&cb);
                if (!ctx->opt->findpv || !ctx->opt->findpv(cb.data, &d)) {
                    if (err) pbc_mkerr(ctx, err, nl, nc, "Preprocessor error", "Variable '");
                    cb_addstr(err, cb.data);
                    cb_addstr(err, "' is not defined");
                    retval = -1;
                    goto ret;
                }
            }
            cb_clear(&cb);
            pbc_pexpr_data(&expr, d);
        } else {
            switch (c) {
                case '(': pbc_pexpr_op(&expr, PBC_PEOP_LP); ++parenct; break;
                case '!': pbc_pexpr_op(&expr, PBC_PEOP_NOT); break;
                case '-': pbc_pexpr_op(&expr, PBC_PEOP_NEG); break;
                case '~': pbc_pexpr_op(&expr, PBC_PEOP_BNOT); break;
                default: {
                    if (err) {
                        if (c == '\n' || c == '\'' || c == EOF)  pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Expected name or value");
                        else pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Unexpected character");
                    }
                    retval = -1;
                    goto ret;
                };
            }
            c = pbc_getc_escnl(&ctx->input, 0);
            continue;
        }
        opagain:;
        switch (c) {
            case '\n':
            case '\'':
            case EOF:
                pbc_ungetc(&ctx->input);
                goto longbreak;
            case ')':
                if (!parenct) {
                    if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "')' without matching '('");
                    retval = -1;
                    goto ret;
                }
                pbc_pexpr_op(&expr, PBC_PEOP_RP);
                --parenct;
                do {
                    c = pbc_getc_escnl(&ctx->input, 0);
                } while (pbc_isspace(c));
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
                    if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Expected '='");
                    retval = -1;
                    goto ret;
                }
                break;
            case '!':
                c = pbc_getc_escnl(&ctx->input, ' ');
                if (c == '=') {
                    pbc_pexpr_op(&expr, PBC_PEOP_NE);
                } else {
                    if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Expected '='");
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
                if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Unexpected character");
                retval = -1;
                goto ret;
                break;
        }
        c = pbc_getc_escnl(&ctx->input, 0);
    }
    longbreak:;
    if (parenct) {
        if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Expected ')'");
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
    #if 0
    for (int i = 0; i < expr.out.index; ++i) {
        struct pbc_pedata d = expr.out.data[i];
        printf("OUT %d: %d\n", d.isop, d.data);
    }
    #endif
    for (int i = 0; i < expr.out.index; ++i) {
        struct pbc_pedata d = expr.out.data[i];
        //printf("READ %d: %d\n", d.isop, d.data);
        if (d.isop) {
            --stack.index;
            switch (d.data) {
                case PBC_PEOP_NEG: stack.data[stack.index] *= -stack.data[stack.index]; break;
                case PBC_PEOP_NOT: stack.data[stack.index] = !stack.data[stack.index]; break;
                case PBC_PEOP_BNOT: stack.data[stack.index] = ~stack.data[stack.index]; break;
                default: {
                    int tmp = stack.data[stack.index];
                    --stack.index;
                    switch (d.data) {
                        case PBC_PEOP_MUL: stack.data[stack.index] *= tmp; break;
                        case PBC_PEOP_DIV: stack.data[stack.index] /= tmp; break;
                        case PBC_PEOP_REM: stack.data[stack.index] %= tmp; break;
                        case PBC_PEOP_ADD: stack.data[stack.index] += tmp; break;
                        case PBC_PEOP_SUB: stack.data[stack.index] -= tmp; break;
                        case PBC_PEOP_BLS: stack.data[stack.index] <<= tmp; break;
                        case PBC_PEOP_BRS: stack.data[stack.index] >>= tmp; break;
                        case PBC_PEOP_GT: stack.data[stack.index] = (stack.data[stack.index] > tmp); break;
                        case PBC_PEOP_GE: stack.data[stack.index] = (stack.data[stack.index] >= tmp); break;
                        case PBC_PEOP_LE: stack.data[stack.index] = (stack.data[stack.index] <= tmp); break;
                        case PBC_PEOP_LT: stack.data[stack.index] = (stack.data[stack.index] < tmp); break;
                        case PBC_PEOP_EQ: stack.data[stack.index] = (stack.data[stack.index] == tmp); break;
                        case PBC_PEOP_NE: stack.data[stack.index] = (stack.data[stack.index] != tmp); break;
                        case PBC_PEOP_BAND: stack.data[stack.index] &= tmp; break;
                        case PBC_PEOP_BXOR: stack.data[stack.index] ^= tmp; break;
                        case PBC_PEOP_BOR: stack.data[stack.index] |= tmp; break;
                        case PBC_PEOP_AND: stack.data[stack.index] = (stack.data[stack.index] && tmp); break;
                        case PBC_PEOP_OR: stack.data[stack.index] = (stack.data[stack.index] || tmp); break;
                    }
                } break;
            }
            ++stack.index;
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
    retval = (stack.data[0] != 0);
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
            } while (c != '\n' && c != '\'' && c != EOF);
            pbc_ungetc(&ctx->input);
            ++pctx->fakedepth;
        }
        return true;
    } else if (!strcasecmp(name, "elif")) {
        if (pctx->fakedepth) {
            int c;
            do {
                c = pbc_getc_escnl(&ctx->input, 0);
            } while (c != '\n' && c != '\'' && c != EOF);
            pbc_ungetc(&ctx->input);
        } else {
            if (pctx->index < 1) {
                if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error", "#else without #if");
                return false;
            }
            if (pctx->data[pctx->index].iselse) {
                if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error", "#elif after #else");
                return false;
            }
            if (pctx->data[pctx->index].done) {
                pctx->data[pctx->index].read = 0;
                int c;
                do {
                    c = pbc_getc_escnl(&ctx->input, 0);
                } while (c != '\n' && c != '\'' && c != EOF);
                pbc_ungetc(&ctx->input);
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
        } while (pbc_isspace(c));
        if (c != '\n' && c != '\'' && c != EOF) {
            if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Unexpected character");
            return false;
        }
        pbc_ungetc(&ctx->input);
        if (!pctx->fakedepth) {
            if (pctx->index < 1) {
                if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error", "#else without #if");
                return false;
            }
            if (pctx->data[pctx->index].iselse) {
                if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error", "Unexpected #else");
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
        } while (pbc_isspace(c));
        if (c != '\n' && c != '\'' && c != EOF) {
            if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Unexpected character");
            return false;
        }
        pbc_ungetc(&ctx->input);
        if (pctx->fakedepth) {
            --pctx->fakedepth;
        } else {
            --pctx->index;
            if (pctx->index < 0) {
                if (err) pbc_mkerr(ctx, err, nl, nc, "Syntax error", "#endif without matching #if");
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
        pbc_mkerr(ctx, err, nl, nc, "Preprocessor error", "Unrecognized directive '");
        cb_addstr(err, name);
        cb_add(err, '\'');
    }
    return false;
}

static inline bool pbc_getname_inline(struct pbc* ctx, struct charbuf* cb, struct charbuf* err) {
    int c;
    do {
        c = pbc_getc_escnl(&ctx->input, 0);
    } while (pbc_isspace(c));
    if (c >= '0' && c <= '9') {
        if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Expected name, given number");
        return false;
    } else if (!pbc_iskwletter(c)) {
        if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Expected name");
        return false;
    }
    do {
        cb_add(cb, c);
        c = pbc_getc_escnl(&ctx->input, ' ');
    } while (pbc_iskwletter(c));
    pbc_ungetc(&ctx->input);
    cb_nullterm(cb);
    return true;
}
static inline bool pbc_getname_loc_inline(struct pbc* ctx, struct charbuf* cb, unsigned* nl, unsigned* nc, struct charbuf* err) {
    int c;
    do {
        c = pbc_getc_escnl(&ctx->input, 0);
    } while (pbc_isspace(c));
    if (c >= '0' && c <= '9') {
        if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Expected name, given number");
        return false;
    } else if (!pbc_iskwletter(c)) {
        if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Expected name");
        return false;
    }
    *nl = ctx->input.line;
    *nc = ctx->input.col;
    do {
        cb_add(cb, c);
        c = pbc_getc_escnl(&ctx->input, ' ');
    } while (pbc_iskwletter(c));
    pbc_ungetc(&ctx->input);
    cb_nullterm(cb);
    return true;
}
static inline enum pbtype pbc_gettype_cb_inline(struct pbc* ctx, bool pdim, unsigned* dim, unsigned* nl, unsigned* nc, struct charbuf* cb, struct charbuf* err) {
    unsigned nl2, nc2;
    if (!pbc_getname_loc_inline(ctx, cb, &nl2, &nc2, err)) return -1;
    if (nl) *nl = nl2;
    if (nc) *nc = nc2;
    char* name = cb->data + 1;
    switch (*cb->data) {
        case 'v':
        case 'V':
            if (!strcasecmp(name, "oid")) return PBTYPE_VOID;
            else if (!strcasecmp(name, "ec")) return PBTYPE_VEC;
            break;
        case 'b':
        case 'B':
            if (!strcasecmp(name, "ool")) return PBTYPE_BOOL;
            break;
        case 'i':
        case 'I':
            if (!strcasecmp(name, "8")) return PBTYPE_I8;
            else if (!strcasecmp(name, "16")) return PBTYPE_I16;
            else if (!strcasecmp(name, "32")) return PBTYPE_I32;
            else if (!strcasecmp(name, "64")) return PBTYPE_I64;
            break;
        case 'u':
        case 'U':
            if (!strcasecmp(name, "8")) return PBTYPE_U8;
            else if (!strcasecmp(name, "16")) return PBTYPE_U16;
            else if (!strcasecmp(name, "32")) return PBTYPE_U32;
            else if (!strcasecmp(name, "64")) return PBTYPE_U64;
            break;
        case 'f':
        case 'F':
            if (!strcasecmp(name, "32")) return PBTYPE_F32;
            else if (!strcasecmp(name, "64")) return PBTYPE_F64;
            break;
        case 's':
        case 'S':
            if (!strcasecmp(name, "tr")) return PBTYPE_STR;
            break;
    }
    if (pdim) {
        //int c;
        //c = pbc_getc_escnl(&ctx->input, ' ');
        *dim = 0;
    }
    if (err) pbc_mkerr(ctx, err, nl2, nc2, "Syntax error", "Unrecognized type name");
    return -1;
}
static inline int pbc_gettype_inline(struct pbc* ctx, bool pdim, unsigned* dim, unsigned* nl, unsigned* nc, struct charbuf* err) {
    struct charbuf cb;
    cb_init(&cb, 16);
    int r = pbc_gettype_cb_inline(ctx, pdim, dim, nl, nc, &cb, err);
    cb_dump(&cb);
    return r;
}
static inline int pbc_getsep_inline(struct pbc* ctx, bool needed, struct charbuf* err) {
    int c;
    do {
        c = pbc_getc_escnl(&ctx->input, 0);
    } while (pbc_isspace(c));
    if (c == ',') return 1;
    if (c == '\n' || c == ':' || c == '\'' || c == EOF) {
        if (needed) {
            if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Unexpected end of statement");
            return -1;
        }
        pbc_ungetc(&ctx->input);
        return 0;
    } else {
        if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Unexpected character");
        return -1;
    }
    
}
static inline int pbc_getanysep_inline(struct pbc* ctx, bool needed, const char* chars, struct charbuf* err) {
    int c;
    do {
        c = pbc_getc_escnl(&ctx->input, 0);
    } while (pbc_isspace(c));
    int c2;
    while ((c2 = *chars)) {
        if (c == c2) return c2;
        ++chars;
    }
    if (c == '\n' || c == ':' || c == '\'' || c == EOF) {
        if (needed) {
            if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Unexpected end of statement");
            return -1;
        }
        pbc_ungetc(&ctx->input);
        return 0;
    } else {
        if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Unexpected character");
        return -1;
    }
    
}
static inline bool pbc_geteos_inline(struct pbc* ctx, bool needed, struct charbuf* err) {
    int c;
    do {
        c = pbc_getc_escnl(&ctx->input, 0);
    } while (pbc_isspace(c));
    if (c == '\n' || c == ':' || c == '\'' || c == EOF) {
        pbc_ungetc(&ctx->input);
        return true;
    } else {
        if (needed) {
            if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error", "Expected end of statement");
        } else {
            pbc_ungetc(&ctx->input);
        }
        return false;
    }
}

static bool pbc_getcmd(struct pbc*, struct charbuf*, const char*, unsigned, unsigned, bool, bool, struct membuf*);

static bool pbc_getexpr(struct pbc* ctx, struct membuf* ops, struct charbuf* err) {
    if (!ops) ops = &ctx->ops;
}

static int pbc_getcond(struct pbc*, struct charbuf*);
static inline int pbc_getcond_inline(struct pbc* ctx, struct charbuf* err) {
    
}
static int pbc_getcond(struct pbc* ctx, struct charbuf* err) {
    pbc_getcond_inline(ctx, err);
}

#include "pbasic/pbc_getcmd.c"

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
    ctx.vars.size = 4;
    ctx.subs.size = 4;
    struct pbc_preproc pctx;
    pctx.size = 4;
    pctx.index = 0;
    pctx.fakedepth = 0;
    pctx.data = malloc(pctx.size * sizeof(*pctx.data));
    pctx.data[0] = (struct pbc_preproc_cond){.read = 1};
    bool retval = true;
    bool preproc = true;
    struct charbuf cb;
    cb_init(&cb, 256);
    while (1) {
        int c;
        nextc:;
        c = pbc_getc(&ctx.input);
        if (pbc_isspace(c)) goto nextc;
        if (preproc) {
            if (c == '\n') goto nextc;
            if (c == ':') {preproc = false; goto nextc;}
        } else {
            if (c == '\n') {preproc = true; goto nextc;}
            if (c == ':') goto nextc;
        }
        if (c == '\'') {
            do {
                c = pbc_getc(&ctx.input);
            } while (c != '\n' && c != EOF);
            pbc_ungetc(&ctx.input);
        } else if (preproc && c == '#') {
            do {
                c = pbc_getc_escnl(&ctx.input, 0);
            } while (pbc_isspace(c));
            if (!pbc_iskwchar(c)) {
                if (err) pbc_mkerr(&ctx, err, 0, 0, "Syntax error", "Unexpected character");
                retval = false;
                goto ret;
            }
            unsigned nl = ctx.input.line, nc = ctx.input.col;
            while (pbc_iskwchar(c)) {
                cb_add(&cb, c);
                c = pbc_getc_escnl(&ctx.input, ' ');
            }
            pbc_ungetc(&ctx.input);
            if (!pbc_getpcmd(&ctx, &pctx, err, cb_peek(&cb), nl, nc)) {
                retval = false;
                goto ret;
            }
            cb_clear(&cb);
        } else if (!pctx.data[pctx.index].read) {
            if (c == EOF) break;
            do {
                c = pbc_getc_escnl(&ctx.input, 0);
            } while (c != '\n' && c != EOF);
            if (c == EOF) break;
        } else if (pbc_iskwchar(c)) {
            unsigned nl = ctx.input.line, nc = ctx.input.col;
            do {
                cb_add(&cb, c);
                c = pbc_getc_escnl(&ctx.input, ' ');
            } while (pbc_iskwchar(c));
            bool cmd;
            if (pbc_isspace(c)) {
                cmd = true;
                do {
                    c = pbc_getc_escnl(&ctx.input, 0);
                } while (pbc_isspace(c));
            } else if (c == '\n' || c == ':' || c == '\'' || c == EOF) {
                cmd = true;
            } else {
                cmd = false;
            }
            if (c == '=') {
                do {
                    c = pbc_getc_escnl(&ctx.input, 0);
                } while (pbc_isspace(c));
                pbc_ungetc(&ctx.input);
                if (!pbc_getexpr(&ctx, NULL, err)) {
                    retval = false;
                    goto ret;
                }
                //pbc_db_put8(&ctx, PBVM_OP_SET);
                //pbc_db_put32(&ctx, pbc_getvar(&ctx, cb_peek(&cb)));
                cb_clear(&cb);
            } else if (cmd) {
                pbc_ungetc(&ctx.input);
                if (!pbc_getcmd(&ctx, err, cb_peek(&cb), nl, nc, false, false, &ctx.ops)) {
                    retval = false;
                    goto ret;
                }
                cb_clear(&cb);
            } else {
                if (err) pbc_mkerr(&ctx, err, 0, 0, "Syntax error", "Unexpected character");
                retval = false;
                goto ret;
            }
        } else if (c == EOF) {
            break;
        } else {
            if (err) pbc_mkerr(&ctx, err, 0, 0, "Syntax error", "Unexpected character");
            retval = false;
            goto ret;
        }
    }
    //longbreak:;
    if (pctx.index) {
        if (err) pbc_mkerr(&ctx, err, pctx.data[pctx.index].l, pctx.data[pctx.index].c, "Syntax error", "#if without #endif");
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
