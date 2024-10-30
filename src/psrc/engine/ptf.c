#include "../rcmgralloc.h"

#include "ptf.h"

#include "../../lz4/lz4file.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void* ptf_loadfile(const char* p, unsigned* res, unsigned* ch) {
    FILE* f = fopen(p, "rb");
    if (fgetc(f) != 'P') return NULL;
    if (fgetc(f) != 'T') return NULL;
    if (fgetc(f) != 'F') return NULL;
    if (fgetc(f) != PTF_REV) return NULL;
    int info = fgetc(f);
    if (info == EOF) {
        fclose(f);
        return NULL;
    }
    int sz = (info & 0xF);
    sz = 1 << sz;
    *res = sz;
    sz *= sz;
    unsigned tmpch = ((info >> 4) & 1) + 3;
    *ch = tmpch;
    sz *= tmpch;
    void* data = malloc(sz);
    LZ4_readFile_t* rf;
    if (LZ4F_isError(LZ4F_readOpen(&rf, f))) {
        free(data);
        fclose(f);
        return NULL;
    }
    if (LZ4F_isError(LZ4F_read(rf, data, sz))) {
        LZ4F_readClose(rf);
        free(data);
        fclose(f);
        return NULL;
    }
    LZ4F_readClose(rf);
    fclose(f);
    return data;
}
