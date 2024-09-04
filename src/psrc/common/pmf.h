#ifndef PSRC_COMMON_PMF_H
#define PSRC_COMMON_PMF_H

#include "attribs.h"

#include <stdint.h>

PACKEDENUM pmf_blocktype {
    PMF_BLOCKTYPE_SHDATA,
    PMF_BLOCKTYPE_CLDATA,
    PMF_BLOCKTYPE_SVDATA,
    PMF_BLOCKTYPE_EMBEDRC,
    PMF_BLOCKTYPE_EXT
};

#pragma pack(push, 1)
struct pmf_blockinfo {
    enum pmf_blocktype type;
    uint32_t pos;
    uint32_t size;
}
#pragma pack(pop)
struct pmf {
    uint32_t blockcount;
    struct pmf_blockinfo* blockinfo;
}

#endif
