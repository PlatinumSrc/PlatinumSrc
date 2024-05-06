#include "pbvm.h"

static inline bool pbc_open(const char* p, struct pbc_stream* s) {
    if (!(s->f = fopen(p, "r"))) return false;
    s->line = 1;
    s->col = 1;
    s->undo = false;
    return true;
}
static inline int pbc_getc(struct pbc_stream* s) {
    if (s->undo) {
        s->undo = false;
        return s->last;
    }
    int c = fgetc(s->f);
    s->last = c;
    s->lastline = s->line;
    s->lastcol = s->col;
    if (c == '\n') {
        ++s->line;
        s->col = 0;
    } else {
        ++s->col;
    }
    return c;
}
static inline void pbc_ungetc(struct pbc_stream* s) {
    s->undo = true;
    s->line = s->lastline;
    s->col = s->lastcol;
}

static void pbc_mkerr(const char* p, struct pbc* ctx, struct charbuf* err, const char* t) {
    char tmp[32];
    cb_addstr(err, p);
    sprintf(tmp, ":%u:%u: ", ctx->input.line, ctx->input.col);
    cb_addstr(err, tmp);
    cb_addstr(err, t);
}

bool pb_compilefile(const char* p, struct pbc_opt* o, struct pb_script* out, struct charbuf* err) {
    struct pbc* ctx = malloc(sizeof(*ctx));
    ctx->scopes.size = 4;
    ctx->ops.size = 256;
    ctx->constdata.size = 32;
    ctx->consts.size = 4;
    ctx->vars.size = 4;
    ctx->subs.size = 4;
    bool retval = false;
    if (!pbc_open(p, &ctx->input)) {retval = false; goto ret;}
    while (1) {
        int c;
        do {
            c = pbc_getc(&ctx->input);
        } while (c == ' ' || c == '\t' || c == '\n');
        if (c == '\'') {
            puts("COMMENT");
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        
        } else if (c == EOF) {
            break;
        } else {
            pbc_mkerr(p, ctx, err, "Syntax error");
            retval = false;
            goto ret;
        }
    }
    ret:;
    free(ctx);
    return retval;
}

void pb_deletescript(struct pb_script* s) {
    (void)s;
}
