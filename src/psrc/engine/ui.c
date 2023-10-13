#include "ui.h"

#include "input.h"

struct uistate uistate;

int newUIElem(enum uielemtype etype, enum uiattr attr, ...) {
    
}

void editUIElem(int e, enum uiattr attr, ...) {
    
}

void deleteUIElem(int e) {
    
}

void clearUIElems(void) {
    
}

void doUIEvents(void) {
    
}

bool initUI(void) {
    if (!createAccessLock(&uistate.lock)) return false;
    uistate.elems.size = 16;
    uistate.elems.len = 0;
    uistate.elems.data = malloc(uistate.elems.size * sizeof(*uistate.elems.data));
    uistate.orphans.size = 4;
    uistate.orphans.len = 0;
    uistate.orphans.data = malloc(uistate.orphans.size * sizeof(*uistate.orphans.data));
    return true;
}

void termUI(void) {
    destroyAccessLock(&uistate.lock);
    free(uistate.elems.data);
    free(uistate.orphans.data);
}
