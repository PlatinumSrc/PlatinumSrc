#include "client.h"

#include "../vlb.h"

#include <string.h>

struct playerdata playerdata;

void updateClient(float framemult) {
    (void)framemult;
    for (uint32_t pli = 0; pli < playerdata.len; ++pli) {
        struct player* pl = &playerdata.data[pli];
        pl->common.cameracalc.pos = pl->camera.pos;
    }
}

static inline void initPlayer(struct player* pl) {
    pl->camera = (struct playercamera){0};
    pl->screen = (struct playerscreen){.w = 1.0, .h = 1.0};
}
static inline void freePlayer(struct player* pl) {
    (void)pl;
}
bool setPlayer(uint32_t pli, const char* username) {
    if (username) {
        ++pli;
        VLB_EXPANDTO(playerdata, pli, 3, 2, return false;);
        --pli;
        char* tmp = strdup(username);
        if (!tmp) {
            playerdata.data[pli].priv.username = NULL;
            return false;
        }
        playerdata.data[pli].priv.username = tmp;
        initPlayer(&playerdata.data[pli]);
    } else {
        if (pli >= playerdata.len || !playerdata.data[pli].priv.username) return false;
        freePlayer(&playerdata.data[pli]);
        free(playerdata.data[pli].priv.username);
    }
    return true;
}

bool initClient(void) {
    #if PSRC_MTLVL >= 2
    if (!createAccessLock(&playerdata.lock)) return false;
    #endif
    VLB_INIT(playerdata, 1, VLB_OOM_NOP);
    return true;
}
void quitClient(void) {
    for (uint32_t pli = 0; pli < playerdata.len; ++pli) {
        if (!playerdata.data[pli].priv.username) continue;
        freePlayer(&playerdata.data[pli]);
        free(playerdata.data[pli].priv.username);
    }
    VLB_FREE(playerdata);
    #if PSRC_MTLVL >= 2
    destroyAccessLock(&playerdata.lock);
    #endif
}
