#ifndef PSRC_COMMON_ARG_H
#define PSRC_COMMON_ARG_H

#include "string.h"

struct args {
    int arg;
    int index;
    int argc;
    char** argv;
};

void args_init(struct args*, int argc, char** argv);
int args_getarg(struct args*, struct charbuf*, struct charbuf* err);
int args_getargval(struct args*, bool required, int split, struct charbuf*, struct charbuf* err);
int args_getvar(struct args*, struct charbuf*, struct charbuf* err);
int args_getvarval(struct args*, bool required, struct charbuf*, struct charbuf* err);

#endif
