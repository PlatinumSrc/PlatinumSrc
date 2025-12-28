#include "wav.h"
#include "../byteorder.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#define get8 ds_getc_noerr
static ALWAYSINLINE uint16_t get16(struct datastream* ds) {
    uint16_t v;
    v = ds_getc_noerr(ds);
    v |= ds_getc_noerr(ds) << 8;
    return v;
}
static inline uint32_t get32(struct datastream* ds) {
    uint32_t v;
    ds_read(ds, 4, &v);
    return swaple32(v);
}
static inline float getf32(struct datastream* ds) {
    float v;
    ds_read(ds, 4, &v);
    return swaplef32(v);
}

void* wav_load(PSRC_DATASTREAM_T ds, enum wav_frmt* frmtout, size_t* lenout, unsigned* chout, unsigned* freqout) {
    char tmp[4];
    if (ds_read(ds, 4, tmp) != 4) return NULL;
    if (tmp[0] != 'R' || tmp[1] != 'I' || tmp[2] != 'F' || tmp[3] != 'F') return NULL;
    ds_skip(ds, 4);
    if (ds_read(ds, 4, tmp) != 4) return NULL;
    if (tmp[0] != 'W' || tmp[1] != 'A' || tmp[2] != 'V' || tmp[3] != 'E') return NULL;
    enum wav_frmt frmt;
    uint32_t div;
    while (1) {
        if (ds_read(ds, 4, tmp) != 4) return NULL;
        uint32_t sz = get32(ds);
        if (tmp[0] == 'f' && tmp[1] == 'm' && tmp[2] == 't' && tmp[3] == ' ') {
            bool isfloat;
            {
                uint16_t tmp16 = get16(ds);
                if (tmp16 == 1) isfloat = false;
                else if (tmp16 == 3) isfloat = true;
                else return NULL;
            }
            *chout = div = get16(ds);
            *freqout = get32(ds);
            ds_skip(ds, 6);
            uint16_t bits = get16(ds);
            if (bits == 8) {
                if (isfloat) return NULL;
                frmt = WAV_FRMT_U8;
            } else if (bits == 16) {
                if (isfloat) return NULL;
                frmt = WAV_FRMT_I16;
                div *= 2;
            } else if (bits == 32) {
                frmt = (isfloat) ? WAV_FRMT_F32 : WAV_FRMT_I32;
                div *= 4;
            } else {
                return NULL;
            }
            *frmtout = frmt;
            break;
        }
        ds_skip(ds, sz);
    }
    while (1) {
        if (ds_read(ds, 4, tmp) != 4) return NULL;
        uint32_t sz = get32(ds);
        if (tmp[0] == 'd' && tmp[1] == 'a' && tmp[2] == 't' && tmp[3] == 'a') {
            if (!sz || sz % div) return NULL;
            void* data = malloc(sz);
            if (!data) return NULL;
            ds_read(ds, sz, data);
            sz /= div;
            #if BYTEORDER != BO_LE
            switch (frmt) {
                default: break;
                case WAV_FRMT_I16: {
                    uint16_t* data16 = data;
                    for (uint16_t i = 0; i < sz; ++i) {
                        data16[i] = swaple32(data16[i]);
                    }
                } break;
                case WAV_FRMT_I32:
                case WAV_FRMT_F32: {
                    uint32_t* data32 = data;
                    for (uint32_t i = 0; i < sz; ++i) {
                        data32[i] = swaple32(data32[i]);
                    }
                } break;
            }
            #endif
            *lenout = sz;
            return data;
            break;
        }
        ds_skip(ds, sz);
    }
    return NULL;
}
