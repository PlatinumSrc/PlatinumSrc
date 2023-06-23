#ifndef ENGINE_STATESTACK_H
#define ENGINE_STATESTACK_H

struct statestack;
typedef void (*statefunc) (struct statestack*);
struct statestack {
    void* args;
    void* ret;
    int size;
    int index;
    statefunc* funcs;
};

void state_initstack(struct statestack*);
void state_deinitstack(struct statestack*);
void state_runstack(struct statestack*);
void state_push(struct statestack*, statefunc, void*);
void state_exit(struct statestack*, void*);

#define state_return(v) state_exit((v)); return

#endif
