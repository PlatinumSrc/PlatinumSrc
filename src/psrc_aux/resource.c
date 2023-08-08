#include "resource.h"
#include "../psrc_aux/threading.h"

static int rc_count = 0;
static void** rc_data = NULL;
static mutex_t rc_lock;

union resource loadResource(enum rctype t, char* p, union rcopt* o) {
    
}

void freeResource_internal(union resource r) {
    
}

void resourceReaper(enum rcreap l) {
    
}
