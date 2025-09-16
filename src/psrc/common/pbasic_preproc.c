#include "../rcmgralloc.h"
#include "pbasic.h"
#include "../crc.h"
#include "../debug.h"

#include <string.h>
#if DEBUG(1)
    #include <stdio.h>
#endif

#include "../glue.h"

static enum pb_error pb__evalpreprocbuiltin(struct pb_compiler* pbc, struct pb_compiler_tokcoll* tc, const char* name, uint32_t namecrc) {
    switch (namecrc & 0xFF) {
        case 0x83:
            if (namecrc == 0x0E2F1883 && !strcasecmp(name, "let")) {
                return PB_ERROR_NONE;
            }
        default:
            break;
    }
    return PB_ERROR_DEF;
}

enum pb_error pb__evalpreproccmd(struct pb_compiler* pbc, struct pb_compiler_tokcoll* tc) {
    if (tc->data[0].type != PB_COMPILER_TOK_TYPE_ID) {
        pb_compitf_puterrln(pbc, PB_ERROR_SYNTAX, "Expected identifier", &tc->data[0].loc);
        return PB_ERROR_SYNTAX;
    }
    char* id = tc->strings.data + tc->data[0].id;
    if (id[0] != ':') {
        char* ns;
        char* name = NULL;
        for (char* tmp = id + 1; *tmp; ++tmp) {
            if (*tmp == ':') name = tmp;
        }
        if (!name) {
            ns = NULL;
            name = id;
            // TODO: search builtins
        } else {
            ns = id;
            *name++ = 0;
        }
        // TODO: search in curns for cmd
        // TODO: search in using for cmd
        // TODO: search in parent namespaces for cmd
        if (ns) *--name = ':';
    } else {
        ++id;
        // TODO: search from root for cmd
    }
    pb_compitf_puterr(pbc, PB_ERROR_DEF, "Preprocessor command '", &tc->data[0].loc);
    cb_addstr(pbc->err, id);
    cb_addstr(pbc->err, "' not found\n");
    return PB_ERROR_DEF;
}
