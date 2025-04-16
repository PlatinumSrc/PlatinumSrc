#ifndef PSRC_ENGINE_WAV_H
#define PSRC_ENGINE_WAV_H

#include "../attribs.h"
#include "../datastream.h"

#include <stddef.h>

PACKEDENUM wav_frmt {
    WAV_FRMT_U8,
    WAV_FRMT_I16,
    WAV_FRMT_I32,
    WAV_FRMT_F32
};

#define WAV_FRMTSIZE(x) (((size_t[]){1, 2, 4, 4})[(x)])

void* wav_load(PSRC_DATASTREAM_T, enum wav_frmt* frmt, size_t* len, unsigned* ch, unsigned* freq);

#endif
