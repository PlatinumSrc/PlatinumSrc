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
            } else {
                return pbc_getc(s);
            }
            return f;
        } else {
            ungetc(c, s->f);
        }
    }
    return c;
}

static void pbc_mkerr(struct pbc* ctx, struct charbuf* err, int l, int c, const char* t) {
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

static int pbc_getcmd(struct pbc*, struct charbuf*, const char*, int, int, bool, bool);

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

static int pbc_getcmd(struct pbc* ctx, struct charbuf* err, const char* name, int nl, int nc, bool func, bool useret) {
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
    uint8_t read : 1;
    uint8_t iselse : 1;
};
struct pbc_preproc {
    struct pbc_preproc_cond* data;
    int index;
    int fakedepth;
    int size;
};
static inline void pbc_preproc_init(struct pbc_preproc* pctx) {
    pctx->size = 4;
    pctx->index = 0;
    pctx->fakedepth = 0;
    pctx->data = malloc(pctx->size * sizeof(*pctx->data));
    pctx->data[0] = (struct pbc_preproc_cond){.read = 1};
}
static inline void pbc_preproc_free(struct pbc_preproc* pctx) {
    free(pctx->data);
}
bool pbc_getpcmd(struct pbc* ctx, struct pbc_preproc* pctx, struct charbuf* err, const char* name, int nl, int nc) {
    int c;
    printf("pbc_getpcmd: {%s}\n", name);
    if (!strcasecmp(name, "if")) {
        if (pctx->data[pctx->index].read) {
            // ...
        } else {
            do {
                c = pbc_getc_escnl(&ctx->input, 0);
            } while (c != '\n' && c != EOF);
            if (c == EOF) pbc_ungetc(&ctx->input);
            ++pctx->fakedepth;
        }
        return true;
    } else if (!strcasecmp(name, "elif")) {
        if (pctx->fakedepth) {
            do {
                c = pbc_getc_escnl(&ctx->input, 0);
            } while (c != '\n' && c != EOF);
            if (c == EOF) pbc_ungetc(&ctx->input);
        } else {
            // ...
        }
        return true;
    } else if (!strcasecmp(name, "else")) {
        do {
            c = pbc_getc_escnl(&ctx->input, 0);
        } while (c == ' ' || c == '\t');
        if (c != '\n' && c != EOF) {
            if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error (unexpected character) 1");
            return false;
        }
        if (!pctx->fakedepth) {
            // ...
        }
        return true;
    } else if (!strcasecmp(name, "endif")) {
        do {
            c = pbc_getc_escnl(&ctx->input, 0);
        } while (c == ' ' || c == '\t');
        if (c != '\n' && c != EOF) {
            if (err) pbc_mkerr(ctx, err, 0, 0, "Syntax error (unexpected character) 2");
            return false;
        }
        if (pctx->fakedepth) {
            --pctx->fakedepth;
        } else {
            // ...
        }
        return true;
    } else if (!pctx->data[pctx->index].read) {
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
    ctx.scopes.size = 4;
    ctx.ops.size = 256;
    ctx.constdata.size = 32;
    ctx.consts.size = 4;
    ctx.vars.size = 4;
    ctx.subs.size = 4;
    struct pbc_preproc pctx;
    pbc_preproc_init(&pctx);
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
    ret:;
    pbc_preproc_free(&pctx);
    cb_dump(&cb);
    return retval;
}

void pb_deletescript(struct pb_script* s) {
    (void)s;
}
