#ifndef PLATINUM_TTY_H
#define PLATINUM_TTY_H

#include <stdint.h>

void tty_setup(void);
void tty_cleanup(void);

int tty_waituntil(uint64_t);
int tty_pause(void);

#endif
