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
    if (pbc->stream.unget) return pbc->stream.last;
    int c = ds_text_getc_fullinline(pbc->stream.ds);
    if (c != DS_END && c) return c;
    if (!pbc->prevstreams.len) return PB_COMPITF_GETC_END;
    ds_close(pbc->stream.ds);
    pbc->stream = pbc->prevstreams.data[--pbc->prevstreams.len];
    goto again;
}

enum pb_error pb_prog_compile(struct pbasic* pb, struct datastream* ds, const struct pb_compiler_opt* opt, uint32_t* progidout, struct charbuf* err) {
    if (!opt) opt = &defaultcompopt;
    struct pb_compiler pbc = {.pb = pb, .opt = opt, .err = err};
    pb_compitf_puterr(&pbc, PB_ERROR_NONE, "Compiler is under development", NULL);

    VLB_INIT(pbc.prevstreams, 1, goto memerr;);
    pbc.stream = (struct pb_compiler_stream){.ds = ds, .type = (ds->type == DS_TYPE_FILE) ? "file" : "stream"};

    VLB_FREE(pbc.prevstreams);

    return PB_ERROR_NONE;

    memerr:;
    pb_compitf_puterr(&pbc, PB_ERROR_MEMORY, NULL, NULL);
    return PB_ERROR_MEMORY;
}

void pb_prog_destroy(struct pbasic* pb, uint32_t progid) {

}
