#include "../rcmgralloc.h"

#include "pbasic.h"

#include "../debug.h"

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

static ALWAYSINLINE int pb_compitf_getc_inline(struct pb_compiler* pbc) {
    again:;
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
static int pb_compitf_getc_local(struct pb_compiler* pbc) {
    return pb_compitf_getc_inline(pbc);
}
int pb_compitf_getc(struct pb_compiler* pbc) {
    return pb_compitf_getc_inline(pbc);
}

#include "pbasic/tokenize.c"

enum pb_error pb_prog_compile(struct pbasic* pb, struct datastream* ds, const struct pb_compiler_opt* opt, uint32_t* progidout, struct charbuf* err) {
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
    VLB_ADD(pbc.sources, (struct pb_compiler_source){.type = "file"}, 3, 2, goto emem;);
    pbc.stream = (struct pb_compiler_stream){.ds = ds, .line = 1, .col = 1};

    while (1) {
        int c;
        //struct pb_compiler_srcloc el;

        do {
            c = pb_compitf_getc_local(&pbc);
        } while (c == ' ' || c == '\t');
        if (c == -1) break;

        if (c == '#') {
            do {
                c = pb_compitf_getc_local(&pbc);
            } while (c == ' ' || c == '\t');
            if (c == -1) break;
            int r = tokenize(&pbc, &pbc.preproctok, true, &c, &e);
            if (r == -1) goto reterr;
            #if DEBUG(1)
            printf("PREPROC[%zu]: ", pbc.preproctok.len);
            dbg_printtok(&pbc.preproctok);
            #endif
            if (pbc.preproctok.len) {
                pbc.preproctok.len = 0;
                pbc.preproctok.strings.len = 0;
            }
        } else if (c == '\n' || c == ';' || c == -1) {
            endstmt:;
            #if DEBUG(1)
            printf("COMPILER[%zu]: ", pbc.comptok.len);
            dbg_printtok(&pbc.comptok);
            #endif
            if (pbc.comptok.len) {
                pbc.comptok.len = 0;
                pbc.comptok.strings.len = 0;
            }
        } else {
            int r = tokenize(&pbc, &pbc.comptok, false, &c, &e);
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
