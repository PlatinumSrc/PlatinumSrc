#include "ui.h"

#include "input.h"

struct uistate uistate;

bool newUIElem(struct uielemptr*, enum uielemtype etype, enum uiattr attr, ...) {
    
}

void editUIElem(struct uielemptr* e, enum uiattr attr, ...) {
    
}

void deleteUIElem(struct uielemptr* e) {
    
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
