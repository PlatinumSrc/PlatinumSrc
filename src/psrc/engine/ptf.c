#include "../rcmgralloc.h"

#include "ptf.h"

#include "../../lz4/lz4ds.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void* ptf_load(PSRC_DATASTREAM_T ds, unsigned* res, unsigned* ch) {
    if (ds_bin_getc(ds) != 'P') return NULL;
    if (ds_bin_getc(ds) != 'T') return NULL;
    if (ds_bin_getc(ds) != 'F') return NULL;
    if (ds_bin_getc(ds) != PTF_REV) return NULL;
    int info = ds_bin_getc(ds);
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
    LZ4_readDS_t* rds;
    if (LZ4F_isError(LZ4DS_readOpen(&rds, ds))) {
        free(data);
        return NULL;
    }
    if (LZ4F_isError(LZ4DS_read(rds, data, sz))) {
        LZ4DS_readClose(rds);
        free(data);
        return NULL;
    }
    LZ4DS_readClose(rds);
    return data;
}
