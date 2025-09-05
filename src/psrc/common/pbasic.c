#include "../rcmgralloc.h"

#include "pbasic.h"

#include <stdio.h>
#include <inttypes.h>

const char* pb__error_str[PB_ERROR__COUNT] = {
    "No error",
    "Syntax error",
    "Type error",
    "Index error",
    "Argument error",
    "Memory error",
    "Internal error"
};

static const struct pb_compiler_opt defaultcompopt = PB_COMPILER_OPT_DEFAULTS;

void pb_compitf_puterr(struct pb_compiler* pbc, enum pb_error e, const char* msg, struct pb_compiler_errloc* el) {
    if (pbc->opt->errprefix) cb_addstr(pbc->err, pbc->opt->errprefix);
    if (e == PB_ERROR_NONE) cb_addstr(pbc->err, "Warning");
    else cb_addstr(pbc->err, pb__error_str[e]);
    if (el) {
        size_t i = el->index;
        struct pb_compiler_stream* s = (i == pbc->prevstreams.len) ? &pbc->stream : &pbc->prevstreams.data[i];
        while (!s->type) {
            s = &pbc->prevstreams.data[--i];
        }
        uint32_t incline = s->incline, inccol = s->inccol;
        char tmp[12];
        cb_addstr(pbc->err, " at line ");
        snprintf(tmp, 12, "%"PRIu32, el->line);
        cb_addstr(pbc->err, tmp);
        cb_addstr(pbc->err, " column ");
        snprintf(tmp, 12, "%"PRIu32, el->col);
        cb_addstr(pbc->err, tmp);
        cb_addstr(pbc->err, " of ");
        cb_addstr(pbc->err, s->type);
        if (s->ds->name) {
            cb_add(pbc->err, ' ');
            cb_add(pbc->err, '\'');
            cb_addstr(pbc->err, s->ds->name);
            cb_add(pbc->err, '\'');
        }
        while (i) {
            s = &pbc->prevstreams.data[--i];
            if (s->type) {
                cb_addstr(pbc->err, " from line ");
                snprintf(tmp, 12, "%"PRIu32, incline);
                cb_addstr(pbc->err, tmp);
                cb_addstr(pbc->err, " column ");
                snprintf(tmp, 12, "%"PRIu32, inccol);
                cb_addstr(pbc->err, tmp);
                cb_addstr(pbc->err, " of ");
                cb_addstr(pbc->err, s->type);
                if (s->ds->name) {
                    cb_add(pbc->err, ' ');
                    cb_add(pbc->err, '\'');
                    cb_addstr(pbc->err, s->ds->name);
                    cb_add(pbc->err, '\'');
                }
                incline = s->incline;
                inccol = s->inccol;
            }
        }
    }
    if (msg) {
        cb_add(pbc->err, ':');
        cb_add(pbc->err, ' ');
        cb_addstr(pbc->err, msg);
    }
    cb_add(pbc->err, '\n');
}

int pb_compitf_getc(struct pb_compiler* pbc) {
    again:;
    if (pbc->stream.unget) {
        pbc->stream.unget = 0;
        return pbc->stream.last;
    }
    int c = ds_text_getc_fullinline(pbc->stream.ds);
    if (c != DS_END && c) {
        if (c != '\n') {
            ++pbc->stream.col;
        } else {
            ++pbc->stream.line;
            pbc->stream.oldcol = pbc->stream.col;
            pbc->stream.col = 1;
        }
        return c;
    }
    if (!pbc->prevstreams.len) return -1;
    ds_close(pbc->stream.ds);
    pbc->stream = pbc->prevstreams.data[--pbc->prevstreams.len];
    goto again;
}

static enum pb_error pb_preproc_parseline(struct pb_compiler* pbc) {
    pb_compitf_readlinecomment(pbc);
    return PB_ERROR_NONE;
}

static enum pb_error pb_compiler_parseline(struct pb_compiler* pbc) {
    pb_compitf_readlinecomment(pbc);
    return PB_ERROR_NONE;
}

enum pb_error pb_prog_compile(struct pbasic* pb, struct datastream* ds, const struct pb_compiler_opt* opt, uint32_t* progidout, struct charbuf* err) {
    if (!opt) opt = &defaultcompopt;
    struct pb_compiler pbc = {.pb = pb, .opt = opt, .err = err};
    enum pb_error e = PB_ERROR_NONE;
    pb_compitf_puterr(&pbc, PB_ERROR_NONE, "Compiler is under development", NULL);

    VLB_INIT(pbc.prevstreams, 1, goto emem;);
    pbc.stream = (struct pb_compiler_stream){.ds = ds, .line = 1, .col = 1, .type = (ds->type == DS_TYPE_FILE) ? "file" : "stream"};

    while (1) {
        int c, lc;
        struct pb_compiler_errloc el;

        do {
            c = pb_compitf_getc(&pbc);
        } while (c == ' ' || c == '\t');
        if (c == -1) break;

        lc = tolower(c);
        if ((lc >= 'a' && lc <= 'z') || c == '_') {
            pb_compitf_ungetc(&pbc, c);
            e = pb_compiler_parseline(&pbc);
            if (e != PB_ERROR_NONE) goto reterr;
        } else if (c == '\'') {
            pb_compitf_readlinecomment(&pbc);
        } else if (c != '\n') {
            if (c == '#') {
                ppreprocagain:;
                do {
                    c = pb_compitf_getc(&pbc);
                } while (c == ' ' || c == '\t');
                lc = tolower(c);
                if ((lc >= 'a' && lc <= 'z') || c == '_') {
                    pb_compitf_ungetc(&pbc, c);
                    e = pb_preproc_parseline(&pbc);
                    if (e != PB_ERROR_NONE) goto reterr;
                } else if (c == '\'') {
                    pb_compitf_readlinecomment(&pbc);
                } else if (c != '\n') {
                    pb_compitf_mkerrloc(&pbc, c, &el);
                    if (c == '`') {
                        if (!pb_compitf_readblockcomment(&pbc)) goto pecomment;
                        goto ppreprocagain;
                    }
                    if (c >= '0' && c <= '9') goto pebadid;
                    if (c == -1) goto pebadeof;
                    goto pebadchar;
                }
            } else if (c == '`') {
                pb_compitf_mkerrloc(&pbc, 0, &el);
                if (!pb_compitf_readblockcomment(&pbc)) goto pecomment;
            } else if (c >= '0' && c <= '9') {
                pb_compitf_mkerrloc(&pbc, 0, &el);
                goto pebadid;
            } else {
                pb_compitf_mkerrloc(&pbc, 0, &el);
                goto pebadchar;
            }
        }

        continue;
        pebadid:;
        pb_compitf_puterr(&pbc, (e = PB_ERROR_SYNTAX), "Invalid identifier", &el);
        goto reterr;
        //pewantid:;
        //pb_compitf_puterr(&pbc, (e = PB_ERROR_SYNTAX), "Expected identifier", &el);
        //goto reterr;
        pebadchar:;
        pb_compitf_puterr(&pbc, (e = PB_ERROR_SYNTAX), "Unexpected character", &el);
        goto reterr;
        //pebadeol:;
        //pb_compitf_puterr(&pbc, (e = PB_ERROR_SYNTAX), "Unexpected end of line", &el);
        //goto reterr;
        pebadeof:;
        pb_compitf_puterr(&pbc, (e = PB_ERROR_SYNTAX), "Unexpected end of line", &el);
        goto reterr;
        pecomment:;
        pb_compitf_puterr(&pbc, (e = PB_ERROR_SYNTAX), "Unterminated comment", &el);
        goto reterr;
    }
    //pbreak:;

    goto ret;

    emem:;
    pb_compitf_puterr(&pbc, (e = PB_ERROR_MEMORY), NULL, NULL);
    reterr:;

    ret:;
    VLB_FREE(pbc.prevstreams);

    return e;
}

void pb_prog_destroy(struct pbasic* pb, uint32_t progid) {

}
