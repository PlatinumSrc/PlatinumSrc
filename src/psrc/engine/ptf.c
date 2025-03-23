#include "../rcmgralloc.h"

#include "ptf.h"

#include "../../lz4/lz4cb.h"

#include <stdlib.h>
#include <stdint.h>

void* ptf_load(PSRC_DATASTREAM_T ds, unsigned* wo, unsigned* ho, unsigned* cho) {
    if (ds_getc(ds) != 'P') return NULL;
    if (ds_getc(ds) != 'T') return NULL;
    if (ds_getc(ds) != 'F') return NULL;
    if (ds_getc(ds) != PTF_REV) return NULL;
    int f = ds_getc(ds);
    if (f == DS_END) return NULL;
    int r = ds_getc(ds);
    if (r == DS_END) return NULL;
    unsigned sz;
    {
        unsigned w = r & 0xF, h = r >> 4;
        *wo = w = 1 << w;
        *ho = h = 1 << h;
        sz = w * h;
        unsigned ch = (f & 1) + 3;
        *cho = ch;
        sz *= ch;
    }
    void* data = malloc(sz);
    if (!data) return NULL;
    LZ4_readCB_t* rcb;
    if (LZ4F_isError(LZ4CB_readOpen(&rcb, (LZ4CB_readcb)ds_read, ds))) {
        free(data);
        return NULL;
    }
    if (LZ4F_isError(LZ4CB_read(rcb, data, sz))) {
        LZ4CB_readClose(rcb);
        free(data);
        return NULL;
    }
    LZ4CB_readClose(rcb);
    return data;
}
