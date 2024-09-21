#include "input.h"

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <inttypes.h>
#ifndef _WIN32
    #include <sys/ioctl.h>
    #include <unistd.h>
#endif

static struct {
    struct key ring[KBUF_RINGSIZE];
    int ringstart;
    int ringnext;
    int ringsize;
    char* inbuf;
    int inbufsz;
} kbuf;

bool kbuf_update(void) {
    int len;
    if (ioctl(STDIN_FILENO, FIONREAD, &len) < 0) return false;
    if (kbuf.inbufsz < len || kbuf.inbufsz - len > 4096) {
        kbuf.inbufsz = len;
        kbuf.inbuf = realloc(kbuf.inbuf, len);
    }
    read(STDIN_FILENO, kbuf.inbuf, len);
    char* s = kbuf.inbuf;
    #define kbu_add(...) do {\
        struct key k = {.t = __VA_ARGS__};\
        if ((k.t & KT_MASK) == KT_CHAR && (k.t & KF_MASK) && isupper(k.c)) {\
            k.t |= KF_SHIFT;\
            k.c = tolower(k.c);\
        }\
        kbuf.ring[kbuf.ringnext] = k;\
        kbuf.ringnext = (kbuf.ringnext + 1) % KBUF_RINGSIZE;\
        ++kbuf.ringsize;\
    } while (0)
    while (1) {
        char c = *s++;
        --len;
        switch (c) {
            default: kbu_add(KT_CHAR, .c = c); break;
            case '\x08': kbu_add(KT_SP | KF_CTRL, .sp = K_SP_BACKSPACE); break;
            case '\n': kbu_add(KT_SP, .sp = K_SP_ENTER); break;
            case '\e':
                if (!len) {
                    kbu_add(KT_CHAR, .c = '\e');
                    break;
                }
                char* olds = s;
                c = *s++;
                int oldlen = len--;
                switch (c) {
                    default: kbu_add(KT_CHAR | KF_ALT, .c = c); goto escok;
                    case '\x08': kbu_add(KT_SP | KF_CTRL | KF_ALT, .sp = K_SP_BACKSPACE); goto escok;
                    case '\n': kbu_add(KT_SP | KF_ALT, .sp = K_SP_ENTER); goto escok;
                    case '[': {
                        if (!len) break;
                        c = *s++;
                        --len;
                        switch (c) {
                            case 'A': kbu_add(KT_NAV, .nav = K_NAV_UP); goto escok;
                            case 'B': kbu_add(KT_NAV, .nav = K_NAV_DOWN); goto escok;
                            case 'C': kbu_add(KT_NAV, .nav = K_NAV_RIGHT); goto escok;
                            case 'D': kbu_add(KT_NAV, .nav = K_NAV_LEFT); goto escok;
                            case '3':
                                if (!len) break;
                                c = *s++;
                                --len;
                                if (c == '~') {
                                    kbu_add(KT_SP, .sp = K_SP_DELETE);
                                    goto escok;
                                } else if (len >= 2 && c == ';' && s[1] == '~') {
                                    enum keytype t = KT_SP;
                                    c = *s - '1';
                                    if (c & 1) t |= KF_SHIFT;
                                    if (c & 2) t |= KF_ALT;
                                    if (c & 4) t |= KF_CTRL;
                                    kbu_add(t, .sp = K_SP_DELETE);
                                    s += 2;
                                    len -= 2;
                                    goto escok;
                                }
                                break;
                            case '<': {
                                if (len < 6) break;
                                int i = 5;
                                while (1) {
                                    char c = s[i++];
                                    if (c == 'M' || c == 'm') {
                                        uint8_t b;
                                        unsigned x, y;
                                        enum keytype to = 0;
                                        sscanf(s, "%" SCNu8 ";%u;%uM", &b, &x, &y);
                                        if (b & 4) to |= KF_SHIFT;
                                        if (b & 8) to |= KF_ALT;
                                        if (b & 16) to |= KF_CTRL;
                                        kbu_add(KT_MOUSEMOVE | to, .mx = x - 1, .my = y - 1);
                                        if (!(c & 32)) {
                                            if (!(b & 32)) {
                                                if (!(b & 64)) kbu_add(KT_MOUSEBUTTON | to, .mbdn = 1, .mb = b & 3);
                                                else kbu_add(KT_MOUSESCROLL | to, .ms = (b & 1) ? 1 : -1);
                                            }
                                        } else {
                                            kbu_add(KT_MOUSEBUTTON | to, .mb = b & 3);
                                        }
                                        s += i;
                                        len -= i;
                                        goto escok;
                                    }
                                    if (i == len) break;
                                }
                            } break;
                        }
                    } break;
                    case '\e':
                        if (len >= 2 && *s == 'O' && s[1] == 'M') {
                            kbu_add(KT_SP | KF_SHIFT | KF_ALT, .sp = K_SP_ENTER);
                            s += 2;
                            len -= 2;
                            goto escok;
                        }
                        kbu_add(KT_SP | KF_ALT, .sp = K_SP_ESC);
                        goto escok;
                    case 'O':
                        if (len) {
                            c = *s++;
                            --len;
                            if (c == 'M') {
                                kbu_add(KT_SP | KF_SHIFT, .sp = K_SP_ENTER);
                                goto escok;
                            } else if (c >= 'P' && c <= 'S') {
                                kbu_add(KT_FN, .fn = c - 'P' + 1);
                                goto escok;
                            }
                        }
                        kbu_add(KT_CHAR | KF_SHIFT | KF_ALT, .c = 'o');
                        goto escok;
                    case '\x7F': kbu_add(KT_SP | KF_ALT, .sp = K_SP_BACKSPACE); goto escok;
                }
                s = olds;
                len = oldlen;
                kbu_add(KT_UNKNOWN);
                escok:;
                break;
            case '\x00' ... '\x07':
            case '\x09':
            case '\x0B' ... '\x1A':
            case '\x1C' ... '\x1F':
                kbu_add(KT_CHAR | KF_CTRL, .c = tolower(c + '@'));
                break;
            case '\x7F': kbu_add(KT_SP, .sp = K_SP_BACKSPACE); break;
        }
        if (!len) break;
    }
    #undef kbu_add
    return true;
}

bool getkey(struct key* o) {
    if (!kbuf.ringsize) return false;
    *o = kbuf.ring[kbuf.ringstart];
    kbuf.ringstart = (kbuf.ringstart + 1) % KBUF_RINGSIZE;
    --kbuf.ringsize;
    return true;
}
