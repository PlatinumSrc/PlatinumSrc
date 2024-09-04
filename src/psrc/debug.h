#ifndef PSRC_DEBUG_H
#define PSRC_DEBUG_H

// PSRC_DBGLVL:
// undef: off
// 0: silent
// 1: basic
// 2: advanced
// 3: detailed
#ifndef PSRC_DBGLVL
    #define DEBUG(x) 0
#else
    #define DEBUG(x) (PSRC_DBGLVL >= (x))
#endif

#endif
