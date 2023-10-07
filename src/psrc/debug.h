#ifndef PSRC_DEBUG_H
#define PSRC_DEBUG_H

// DBGLVL:
// <= -1: off
// 0: silent
// 1: basic
// 2: advanced
// 3: detailed
#ifndef DBGLVL
    #define DBGLVL -1
#endif

#define DEBUG(x) (DBGLVL >= (x))

#endif
