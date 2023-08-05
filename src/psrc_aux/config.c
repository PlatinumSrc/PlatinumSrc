#include "config.h"
#include "fs.h"
#include "string.h"
#include "logging.h"
#include "../debug.h"

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "../glue.h"

static int fgetc_skip(FILE* f) {
    int c;
    do {
        c = fgetc(f);
    } while ((c < ' ' || c >= 127) && (c != '\n' && c != '\t' && c != EOF));
    return c;
}

static inline int gethex(int c) {
    if (c >= 'a' && c <= 'f') c -= 32;
    return (c >= '0' && c <= '9') ? c - 48 : ((c >= 'A' && c <= 'F') ? c - 55 : -1);
}

static inline int interpesc(FILE* f, int c, char* out) {
    switch (c) {
        case EOF:;
            return -1;
        case 'a':;
            *out++ = '\a'; *out = 0; return 1;
        case 'b':;
            *out++ = '\b'; *out = 0; return 1;
        case 'e':;
            *out++ = '\e'; *out = 0; return 1;
        case 'f':;
            *out++ = '\f'; *out = 0; return 1;
        case 'n':;
            *out++ = '\n'; *out = 0; return 1;
        case 'r':;
            *out++ = '\r'; *out = 0; return 1;
        case 't':;
            *out++ = '\t'; *out = 0; return 1;
        case 'v':;
            *out++ = '\v'; *out = 0; return 1;
        case 'x':; {
            int c1 = fgetc_skip(f);
            if (c1 == EOF) return -1;
            int c2 = fgetc_skip(f);
            if (c2 == EOF) return -1;
            int h1, h2;
            if ((h1 = gethex(c1)) == -1 || (h2 = gethex(c2)) == -1) {
                *out++ = 'x'; *out++ = c1; *out++ = c2; *out = 0;
                return 0;
            }
            *out++ = (h1 << 4) + h2; *out = 0;
            return 1;
        } break;
    }
    *out++ = c; *out = 0;
    return 0;
}

static void interpfinal(char* s, struct charbuf* b) {
    char c;
    bool inStr = false;
    while ((c = *s++)) {
        if (inStr) {
            if (c == '\\') {
                c = *s++;
                if (!c) {
                    cb_add(b, '\\');
                    return;
                } else if (c == '"') {
                    cb_add(b, '"');
                } else {
                    cb_add(b, '\\');
                    cb_add(b, c);
                }
            } else {
                if (c == '"') inStr = false;
                else cb_add(b, c);
            }
        } else {
            if (c == '"') inStr = true;
            else cb_add(b, c);
        }
    }
}

struct cfg* cfg_open(const char* p) {
    if (isFile(p) < 1) {
        plog(LL_WARN | LF_FUNC, LE_CANTOPEN(p));
        return NULL;
    }
    FILE* f = fopen(p, "r");
    if (!f) {
        plog(LL_WARN | LF_FUNC, LE_CANTOPEN(p));
        return NULL;
    }
    #if DEBUG(1)
    plog(LL_INFO, "Reading config %s...", p);
    #endif
    struct charbuf sect;
    struct charbuf var;
    struct charbuf data;
    cb_init(&sect, 256);
    cb_init(&var, 256);
    cb_init(&data, 256);
    bool inStr;
    while (1) {
        int c;
        newline:;
        inStr = false;
        cb_clear(&sect, 256);
        cb_clear(&var, 256);
        cb_clear(&data, 256);
        do {
            c = fgetc_skip(f);
        } while (c == ' ' || c == '\t' || c == '\n');
        if (c == EOF) goto endloop;
        if (c == '[') {
            do {
                c = fgetc_skip(f);
                if (c == '\n') goto newline;
            } while (c == ' ' || c == '\t');
            if (c == EOF) goto endloop;
            while (1) {
                if (inStr) {
                    if (c == '\\') {
                        c = fgetc_skip(f);
                        if (c == EOF) goto endloop;
                        if (c == '\n') goto newline;
                        char tmpbuf[4];
                        c = interpesc(f, c, tmpbuf);
                        if (c == -1) goto endloop;
                        if (c == 0) cb_add(&sect, '\\');
                        cb_addstr(&sect, tmpbuf);
                    } else {
                        cb_add(&sect, c);
                        if (c == '"') inStr = false;
                    }
                } else {
                    if (c == ']') {
                        do {
                            c = fgetc_skip(f);
                        } while (c == ' ' || c == '\t');
                        if (c == EOF) goto endloop;
                        if (c != '\n') {
                            do {
                                c = fgetc_skip(f);
                                if (c == EOF) goto endloop;
                            } while (c != '\n');
                            goto newline;
                        }
                        if (sect.len > 0) {
                            char tmpc = sect.data[sect.len - 1];
                            while (tmpc == ' ' || tmpc == '\t') {
                                --sect.len;
                                tmpc = sect.data[sect.len - 1];
                            }
                        }
                        char* tmp = cb_reinit(&sect, 256);
                        interpfinal(tmp, &sect);
                        tmp = cb_reinit(&sect, 256);
                        #if DEBUG(1)
                        plog(LL_INFO, "  [%s]", tmp);
                        #endif
                        free(tmp);
                        goto newline;
                    } else {
                        cb_add(&sect, c);
                        if (c == '"') inStr = true;
                    }
                }
                c = fgetc_skip(f);
                if (c == EOF) goto endloop;
                if (c == '\n') goto newline;
            }
        } else {
            while (1) {
                if (inStr) {
                    if (c == '\\') {
                        c = fgetc_skip(f);
                        if (c == EOF) goto endloop;
                        if (c == '\n') goto newline;
                        char tmpbuf[4];
                        c = interpesc(f, c, tmpbuf);
                        if (c == -1) goto endloop;
                        if (c == 0) cb_add(&var, '\\');
                        cb_addstr(&var, tmpbuf);
                    } else {
                        cb_add(&var, c);
                        if (c == '"') inStr = false;
                    }
                } else {
                    if (c == '=') {
                        do {
                            c = fgetc_skip(f);
                        } while (c == ' ' || c == '\t');
                        while (1) {
                            if (inStr) {
                                if (c == '\\') {
                                    c = fgetc_skip(f);
                                    if (c == EOF) goto endloop;
                                    if (c != '\n') {
                                        char tmpbuf[4];
                                        c = interpesc(f, c, tmpbuf);
                                        if (c == -1) goto endloop;
                                        if (c == 0) cb_add(&sect, '\\');
                                        cb_addstr(&data, tmpbuf);
                                    }
                                } else {
                                    cb_add(&data, c);
                                    if (c == '"') inStr = false;
                                }
                            } else {
                                if (c == '\n') {
                                    char* varstr;
                                    char* datastr;
                                    if (var.len > 0) {
                                        char tmpc = var.data[var.len - 1];
                                        while (tmpc == ' ' || tmpc == '\t') {
                                            --var.len;
                                            tmpc = var.data[var.len - 1];
                                        }
                                    }
                                    varstr = cb_reinit(&var, 256);
                                    interpfinal(varstr, &var);
                                    varstr = cb_reinit(&var, 256);
                                    if (data.len > 0) {
                                        char tmpc = data.data[data.len - 1];
                                        while (tmpc == ' ' || tmpc == '\t') {
                                            --data.len;
                                            tmpc = data.data[data.len - 1];
                                        }
                                    }
                                    datastr = cb_reinit(&data, 256);
                                    interpfinal(datastr, &data);
                                    datastr = cb_reinit(&data, 256);
                                    // register the var
                                    #if DEBUG(1)
                                    plog(LL_INFO, "    %s = %s", varstr, datastr);
                                    #endif
                                    free(varstr);
                                    free(datastr);
                                    goto newline;
                                } else {
                                    cb_add(&data, c);
                                    if (c == '"') inStr = true;
                                }
                            }
                            c = fgetc_skip(f);
                            if (c == EOF) goto endloop;
                        }
                    } else {
                        cb_add(&var, c);
                        if (c == '"') inStr = true;
                    }
                }
                c = fgetc_skip(f);
                if (c == EOF) goto endloop;
                if (c == '\n') goto newline;
            }
        }
    }
    endloop:;
    cb_dump(&sect);
    cb_dump(&var);
    cb_dump(&data);
    fclose(f);
    return NULL;
}
