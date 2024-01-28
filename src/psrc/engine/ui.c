#include "ui.h"

#include "input.h"

#include <stdlib.h>

struct uistate uistate;

bool newUIElem(struct uielemptr* e, enum uielemtype etype, enum uiattr attr, ...) {
    (void)e; (void)etype; (void)attr;
    return false;
}

void editUIElem(struct uielemptr* e, enum uiattr attr, ...) {
    (void)e; (void)attr;
}

void deleteUIElem(struct uielemptr* e) {
    (void)e;
}

void clearUIElems(void) {
    
}

void doUIEvents(void) {
    
}

bool initUI(void) {
    #ifndef PSRC_NOMT
    if (!createAccessLock(&uistate.lock)) return false;
    #endif
    uistate.elems.size = 16;
    uistate.elems.len = 0;
    uistate.elems.data = malloc(uistate.elems.size * sizeof(*uistate.elems.data));
    uistate.orphans.size = 4;
    uistate.orphans.len = 0;
    uistate.orphans.data = malloc(uistate.orphans.size * sizeof(*uistate.orphans.data));
    return true;
}

void quitUI(void) {
    #ifndef PSRC_NOMT
    destroyAccessLock(&uistate.lock);
    #endif
    free(uistate.elems.data);
    free(uistate.orphans.data);
}
