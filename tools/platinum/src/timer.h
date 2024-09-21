#ifndef PLATINUM_TIMER_H
#define PLATINUM_TIMER_H

#include <stdint.h>

struct timer {
    uint64_t t;
    uint64_t a;
    void* d;
};

void timer_init(void);

int timer_new(uint64_t a, void* d);
void timer_delete(int i);

int timer_getnext(uint64_t* u, void** d);

#endif
