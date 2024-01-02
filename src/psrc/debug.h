#ifndef PSRC_DEBUG_H
#define PSRC_DEBUG_H

// PSRC_DBGLVL:
// <= -1: off
// 0: silent
// 1: basic
// 2: advanced
// 3: detailed
#ifndef PSRC_DBGLVL
    #define PSRC_DBGLVL -1
#endif

#define DEBUG(x) (PSRC_DBGLVL >= (x))

#endif
