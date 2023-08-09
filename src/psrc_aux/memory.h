#ifndef AUX_MEMORY_H
#define AUX_MEMORY_H

#include <stdint.h>
#include <stddef.h>

void getMemStats(uintptr_t* total, uintptr_t* used);
void getMemStatString(char* buf, size_t bufsize);
int getMemUsage(void); // returns 0..100

#endif
