#ifndef PSRC_ENGINE_PTF_H
#define PSRC_ENGINE_PTF_H

#define PTF_REV 0

#include "../datastream.h"

enum ptf_frmt {
    PTF_FRMT_I,
    PTF_FRMT_IA,
    PTF_FRMT_RGB,
    PTF_FRMT_RGBA
};

void* ptf_load(PSRC_DATASTREAM_T, unsigned* w, unsigned* h, unsigned* ch);

#endif
