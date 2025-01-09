#include "../rcmgralloc.h"

#include "ptf.h"

#include "../../lz4/lz4cb.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void* ptf_load(PSRC_DATASTREAM_T ds, unsigned* res, unsigned* ch) {
    if (ds_getc(ds) != 'P') return NULL;
    if (ds_getc(ds) != 'T') return NULL;
    if (ds_getc(ds) != 'F') return NULL;
    if (ds_getc(ds) != PTF_REV) return NULL;
    int info = ds_getc(ds);
    if (info == DS_END) return NULL;
    int sz = (info & 0xF);
    sz = 1 << sz;
    *res = sz;
    sz *= sz;
    unsigned tmpch = ((info >> 4) & 1) + 3;
    *ch = tmpch;
    sz *= tmpch;
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
