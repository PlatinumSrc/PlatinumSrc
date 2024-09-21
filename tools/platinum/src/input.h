#ifndef PLATINUM_INPUT_H
#define PLATINUM_INPUT_H

#include <stdint.h>
#include <stdbool.h>

enum __attribute__((packed)) keytype {
    KT_UNKNOWN,
    KT_CHAR,
    KT_SP,
    KT_NAV,
    KT_FN,
    KT_MOUSEMOVE,
    KT_MOUSEBUTTON,
    KT_MOUSESCROLL,
    KF_SHIFT = (1 << 5),
    KF_CTRL = (1 << 6),
    KF_ALT = (1 << 7),
};
#define KT_MASK 0x1F
#define KF_MASK 0xE0

enum __attribute__((packed)) key_sp {
    K_SP_BACKSPACE,
    K_SP_ENTER,
    K_SP_ESC,
    K_SP_DELETE
};
enum __attribute__((packed)) key_nav {
    K_NAV_UP,
    K_NAV_DOWN,
    K_NAV_LEFT,
    K_NAV_RIGHT,
    K_NAV_HOME,
    K_NAV_END,
    K_NAV_PGUP,
    K_NAV_PGDN
};

struct key {
    enum keytype t;
    union {
        uint32_t c;
        enum key_sp sp;
        enum key_nav nav;
        uint8_t fn;
        struct {
            uint16_t mx;
            uint16_t my;
        };
        struct {
            uint8_t mbdn : 1;
            uint8_t mb : 7;
        };
        int8_t ms;
    };
};

#define KBUF_RINGSIZE 256

bool kbuf_update(void);
bool getkey(struct key*);

#endif
