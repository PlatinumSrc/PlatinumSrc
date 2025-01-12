#ifndef PSRC_ARG_H
#define PSRC_ARG_H

#include "string.h"

struct args {
    int arg;
    int index;
    int argc;
    char** argv;
};

void args_init(struct args*, int argc, char** argv);
int args_getopt(struct args*, struct charbuf*, struct charbuf* err);
int args_getoptval(struct args*, int needed, int split, struct charbuf*, struct charbuf* err);
int args_getvar(struct args*, struct charbuf*, struct charbuf* err);
int args_getvarval(struct args*, int needed, struct charbuf*, struct charbuf* err);

#endif
