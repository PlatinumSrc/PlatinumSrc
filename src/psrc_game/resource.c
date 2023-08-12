#include "resource.h"
#include "../psrc_aux/threading.h"

static int rccount = 0;
static void** rcdata = NULL;
static mutex_t rclock;

union resource loadResource(enum rctype t, char* p, union rcopt* o) {
    
}

void freeResource_internal(union resource r) {
    
}

bool initResource(void) {
    if (!createMutex(&rclock)) return false;
    return true;
}
