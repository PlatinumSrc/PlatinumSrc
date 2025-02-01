#include "audio.h"

#include "../logging.h"

struct audiostate audiostate;

void updateAudio(float framemult) {
    (void)framemult;
}

bool startAudio(void) {
    return true;
}

void stopAudio(void) {

}

bool restartAudio(void) {
    return true;
}

bool initAudio(void) {
    #ifndef PSRC_NOMT
    if (!createAccessLock(&audiostate.lock)) return false;
    #endif
    if (SDL_Init(SDL_INIT_AUDIO)) {
        plog(LL_CRIT | LF_FUNCLN, "Failed to init audio: %s", SDL_GetError());
        return false;
    }
    return true;
}

void quitAudio(bool quick) {
    if (quick) return;
    #ifndef PSRC_NOMT
    destroyAccessLock(&audiostate.lock);
    #endif
}
