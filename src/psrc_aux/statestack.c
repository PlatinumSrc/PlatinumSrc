#include "statestack.h"

#include <stddef.h>
#include <stdlib.h>

void state_initstack(struct statestack* s) {
    s->args = NULL;
    s->ret = NULL;
    s->size = 4;
    s->index = -1;
    s->funcs = malloc(s->size * sizeof(*s->funcs));
}

void state_deinitstack(struct statestack* s) {
    free(s->funcs);
    s->funcs = NULL;
}

void state_runstack(struct statestack* s) {
    s->funcs[s->index](s);
}

void state_push(struct statestack* s, statefunc f, void* a) {
    ++s->index;
    if (s->index >= s->size) {
        //do {
            s->size *= 2;
        //} while (s->size <= s->index);
        s->funcs = realloc(s->funcs, s->size * sizeof(*s->funcs));
    }
    s->funcs[s->index] = f;
    s->args = a;
}

void state_exit(struct statestack* s, void* r) {
    s->ret = r;
    --s->index;
}
