#include "pbvm.h"

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
        return c;
    }
    c = fgetc(s->f);
    s->last = c;
    if (c == '\n') {
        s->lastline = s->line++;
        s->lastcol = s->col;
        s->col = 0;
    } else {
        s->lastcol = s->col++;
    }
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

static void pbc_mkerr(struct pbc* ctx, struct charbuf* err, const char* t) {
    char tmp[32];
    sprintf(tmp, "(%u:%u): ", ctx->input.line, ctx->input.col);
    cb_addstr(err, tmp);
    cb_addstr(err, t);
}

static inline int pbc_getkw_inline(struct pbc* ctx, struct charbuf* cb) {
    
}
static inline int pbc_getsep_inline(struct pbc* ctx, const char* chars) {
    
}
static bool pbc_getcmd(struct pbc*, struct charbuf*, char*, bool, bool);
static int pbc_getexpr(struct pbc*);
static inline int pbc_getexpr_inline(struct pbc* ctx) {
    
}
static int pbc_getexpr(struct pbc* ctx) {
    pbc_getexpr_inline(ctx);
}
static int pbc_getcond(struct pbc*);
static inline int pbc_getcond_inline(struct pbc* ctx) {
    
}
static int pbc_getcond(struct pbc* ctx) {
    pbc_getcond_inline(ctx);
}
static bool pbc_getcmd(struct pbc* ctx, struct charbuf* err, char* name, bool func, bool useret) {
    if (func) {
    
    } else {
        
    }
}

bool pb_compilefile(const char* p, struct pbc_opt* o, struct pb_script* out, struct charbuf* err) {
    struct pbc ctx;
    ctx.scopes.size = 4;
    ctx.ops.size = 256;
    ctx.constdata.size = 32;
    ctx.consts.size = 4;
    ctx.vars.size = 4;
    ctx.subs.size = 4;
    bool retval = false;
    if (!pbc_open(p, &ctx.input)) {retval = false; goto ret;}
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
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
            do {
                cb_add(&cb, c);
                c = pbc_getc_escnl(&ctx.input, ' ');
            } while ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
            while (c == ' ' || c == '\t') {
                c = pbc_getc_escnl(&ctx.input, 0);
            }
            if (c == '=') {
                do {
                    c = pbc_getc_escnl(&ctx.input, 0);
                } while (c == ' ' || c == '\t');
                //pbc_getexpr(&ctx);
                //pbc_put8(&ctx, PBVM_OP_SET);
                //pbc_put32(&ctx, pbc_getvar(&ctx, cb_peek(&cb)));
                c = pbc_getc(&ctx.input);
                if (c == EOF) {
                    break;
                } else if (c != '\n' && c != ':') {
                    pbc_mkerr(&ctx, err, "Syntax error (expected ':' or '\\n')");
                    retval = false;
                    goto ret;
                }
            } else {
                bool func = (c == '(');
                if (!func) pbc_ungetc(&ctx.input);
                //if (!pbc_getcmd(&ctx, err, cb_peek(&cb), func, false)) {
                //    retval = false;
                //    goto ret;
                //}
                c = pbc_getc(&ctx.input);
                if (c == EOF) {
                    break;
                } else if (c != '\n' && c != ':') {
                    pbc_mkerr(&ctx, err, "Syntax error (expected ':' or '\\n')");
                    retval = false;
                    goto ret;
                }
            }
        } else if (c == EOF) {
            break;
        } else {
            pbc_mkerr(&ctx, err, "Syntax error (unexpected char)");
            retval = false;
            goto ret;
        }
    }
    //longbreak:;
    ret:;
    cb_dump(&cb);
    return retval;
}

void pb_deletescript(struct pb_script* s) {
    (void)s;
}
