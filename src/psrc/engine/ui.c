#include "ui.h"

#define COLOR_BG                0x434959ff
#define COLOR_LIGHTBG           0xd7d7d7ff
#define COLOR_TEXT              0xd7d7d7ff
#define COLOR_LIGHTTEXT         0x000000ff
#define COLOR_SELTEXT           0xffffffff
#define COLOR_SELTEXTBG         0x238fd3ff
#define COLOR_DISABLEDTEXT      0xd7d7d7ff
#define COLOR_DISABLEDTEXTSHDW  0x737373ff
#define COLOR_RAISEDBDR         0x7f8692ff
#define COLOR_SUNKENBDR         0x21262fff
#define COLOR_HOVERBDR          0xd7d7d7ff
#define COLOR_DISABLEDBDR       0x000000ff
#define COLOR_TOOLTIPBG         0x656b75e5
#define COLOR_TOOLTIPBDR        0x21262fe5
#define COLOR_ODDLISTROW        0x383d4bff
#define COLOR_EVENLISTROW       0x404555ff
#define COLOR_LISTSEL           0x68708aff
static struct uitheme builtintheme = {
    .icon = {.size = 16.0f},
    .tooltip = {
        .bg = {.color = COLOR_TOOLTIPBG},
        .border = {
            .top = {.color = COLOR_TOOLTIPBDR, .size = 1.0f},
            .bottom = {.color = COLOR_TOOLTIPBDR, .size = 1.0f},
            .left = {.color = COLOR_TOOLTIPBDR, .size = 1.0f},
            .right = {.color = COLOR_TOOLTIPBDR, .size = 1.0f},
            .topleft = {.color = COLOR_TOOLTIPBDR},
            .bottomleft = {.color = COLOR_TOOLTIPBDR},
            .topright = {.color = COLOR_TOOLTIPBDR},
            .bottomright = {.color = COLOR_TOOLTIPBDR}
        },
        .padding = {.top = 8.0f, .bottom = 8.0f, .left = 8.0f, .right = 8.0f},
        .text = {.frmt = {.fgc = COLOR_TEXT, .pt = 8, .hasfgc = 1}}
    },
    .label = {
        .text = {
            .normal = {.frmt = {.fgc = COLOR_TEXT, .pt = 8, .hasfgc = 1}},
            .disabled = {.frmt = {.fgc = COLOR_DISABLEDTEXT, .pt = 8, .hasfgc = 1}}
        }
    }
};
