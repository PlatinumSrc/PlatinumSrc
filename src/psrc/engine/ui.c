#include "ui.h"

struct uistate uistate;

bool initUI(void) {
    #if PSRC_MTLVL >= 2
    if (!createAccessLock(&uistate.lock)) return false;
    #endif
    return true;
}

void quitUI(void) {
    #if PSRC_MTLVL >= 2
    destroyAccessLock(&uistate.lock);
    #endif
}

// Colors
#define COLORPAL_00 0x000000FF /* Black */
#define COLORPAL_01 0x1D1F26FF /* Dark blue, sunken border */
#define COLORPAL_02 0x292C36FF /* Inner sunken border */
#define COLORPAL_03 0x343945FF /* Empty progress bar */
#define COLORPAL_04 0x404654FF /* Normal background */
#define COLORPAL_05 0x4C5263FF
#define COLORPAL_06 0x575E73FF
#define COLORPAL_07 0x636B82FF /* Filled progress bar */
#define COLORPAL_08 0x6E7891FF /* Inner raised border */
#define COLORPAL_09 0x7A84A1FF /* Raised border */
#define COLORPAL_0A 0x8691B0FF
#define COLORPAL_0B 0x919EBFFF /* Light blue */
#define COLORPAL_0C 0xEEEEEEFF /* Text */
#define COLORPAL_0D 0x238FD3FF /* Selected text background */
#define COLORPAL_0E 0x737373FF /* Disabled text shadow */
#define COLORPAL_0F 0x656A75E5 /* Tooltip background */
#define COLORPAL_10 0xEEE07AFF /* Link */
#define COLORPAL_11 0xEEECCEFF /* Hovered link */
#define COLORPAL_1F 0xFFFFFFFF /* White */
