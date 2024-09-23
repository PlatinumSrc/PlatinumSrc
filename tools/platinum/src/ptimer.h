#ifndef PLATINUM_PTIMER_H
#define PLATINUM_PTIMER_H

#include <stdint.h>

void ptimer_init(void);

int ptimer_new(uint64_t a, void* d);
void ptimer_delete(int i);

int ptimer_getnext(uint64_t* u, void** d);

#endif
