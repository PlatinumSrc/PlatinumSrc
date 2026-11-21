#ifndef PSRC_ENGINE_ENT_H
#define PSRC_ENGINE_ENT_H

#include "../common/ent.h"

PACKEDENUM clent_comptype {
    CLENT_COMP_CAMERA = 0x80,

    CLENT_COMP_LISTENER,

    CLENT_COMP___COUNT,
    CLENT_COMP__COUNT = CLENT_COMP___COUNT - 0x80
}

#endif
