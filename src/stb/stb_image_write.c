#ifndef NXDK
#include <zlib.h>
#include <stdlib.h>
#define STBIW_ZLIB_COMPRESS compress2_wrapper
static unsigned char* compress2_wrapper(unsigned char* data, int data_len, int* out_len, int quality) {
    unsigned long zsize = compressBound(data_len);
    unsigned char* zdata = malloc(zsize);
    int r = compress2(zdata, &zsize, data, data_len, quality);
    if (r == Z_OK) {
        zdata = realloc(zdata, zsize);
        *out_len = zsize;
        return zdata;
    }
    free(zdata);
    return NULL;
}
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
