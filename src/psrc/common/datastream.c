#include "datastream.h"

#include <string.h>

bool ds__refill_internal(struct datastream* ds) {
    if (ds->mode == DS_MODE_FILE) {
        
    }
    
}

size_t ds_read(struct datastream* ds, void* b, size_t l) {
    if (!l) return -ds->atend;
    size_t r;
    if (!ds->unget) {
        if (ds->atend) return -1;
        if (ds->pos >= ds->datasz && !ds__refill(ds)) return -ds->atend;
        r = 0;
    } else {
        ds->unget = 0;
        *((uint8_t*)b) = ds->last;
        if (l == 1 || ds->atend || (ds->pos >= ds->datasz && !ds__refill(ds))) return 1;
        --l;
        r = 1;
    }
    do {
        size_t a = ds->datasz - ds->pos;
        if (l > a) {
            memcpy(((uint8_t*)b) + r, ds->buf + ds->pos, a);
            r += a;
            l -= a;
            ds->pos += a;
        } else {
            memcpy(((uint8_t*)b) + r, ds->buf + ds->pos, l);
            r += l;
            ds->pos += l;
            break;
        }
    } while (ds__refill(ds));
    return r;
}
