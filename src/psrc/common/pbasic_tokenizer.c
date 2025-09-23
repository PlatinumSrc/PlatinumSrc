#include "../rcmgralloc.h"
#include "pbasic.h"
#include "../crc.h"
#include "../debug.h"
#include "../util.h"

#include <string.h>
#if DEBUG(1)
    #include <stdio.h>
#endif

#include "../glue.h"

static int pb__compiler_getc(struct pb_compiler* pbc) {
    return pb__compiler_getc_inline(pbc);
}

static bool pb__tok_inspreprocvar(struct pb_compiler* pbc, struct pb_compiler_tokcoll* tc, bool preproc, int* cptr, enum pb_error* e);

int pb__tokenize(struct pb_compiler* pbc, struct pb_compiler_tokcoll* tc, bool preproc, int* cptr, enum pb_error* e) {
    struct pb_compiler_srcloc el;
    int c = *cptr;
    while (1) {
        bool int_skipfloat = false;
        if (c == '\'') {
            while (1) {
                c = pb__compiler_getc(pbc);
                if (c == '\n' || c == -1) goto endstmt;
                if (!c) {
                    pb_compitf_mksrcloc(pbc, 0, &el);
                    goto ebadchar;
                }
            }
        } else if (c == '`') {
            pb_compitf_mksrcloc(pbc, 0, &el);
            while (1) {
                int c = pb__compiler_getc(pbc);
                if (c == '`') break;
                if (c == -1) {
                    pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Unterminated comment", &el);
                    goto reterr;
                }
                if (!c) {
                    pb_compitf_mksrcloc(pbc, 0, &el);
                    goto ebadchar;
                }
            }
        } else if (c == '\\') {
            c = pb__compiler_getc(pbc);
            if (c == ' ' || c == '\t') {
                pb_compitf_mksrcloc(pbc, 0, &el);
                pb_compitf_puterrln(pbc, PB_ERROR_NONE, "Whitespace after line continuation", &el);
                do {
                    c = pb__compiler_getc(pbc);
                } while (c == ' ' || c == '\t');
            }
            if (c == -1) goto endstmt;
            if (c != '\n') {
                pb_compitf_mksrcloc(pbc, 0, &el);
                pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Expected newline", &el);
                goto reterr;
            }
            if (preproc) goto getcandcont;
            break;
        } else {
            if (tc->preprocinsstage) {
                if (!pb__tok_inspreprocvar(pbc, tc, preproc, &c, e)) goto reterr;
                goto skipgetc;
            }
            struct pb_compiler_tok* t;
            VLB_NEXTPTR(*tc, t, 3, 2, goto emem;);
            pb_compitf_mksrcloc(pbc, 0, &t->loc);
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == ':') {
                t->type = PB_COMPILER_TOK_TYPE_ID;
                t->id = tc->strings.len;
                if (!cb_add(&tc->strings, c)) goto emem;
                while (1) {
                    c = pb__compiler_getc(pbc);
                    if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && c != '_') {
                        if (c == ':') {
                            pb_compitf_mksrcloc(pbc, 0, &el);
                        } else if (c < '0' || c > '9') {
                            if (tc->strings.data[tc->strings.len - 1] == ':') {
                                --tc->strings.len;
                                if (tc->strings.len - t->id) {
                                    VLB_NEXTPTR(*tc, t, 3, 2, goto emem;);
                                    t->loc = el;
                                }
                                t->type = PB_COMPILER_TOK_TYPE_SYM;
                                t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_COLON;
                            }
                            break;
                        }
                        if (tc->strings.data[tc->strings.len - 1] == ':') {
                            pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Invalid identifier", &t->loc);
                            goto reterr;
                        }
                    }
                    if (!cb_add(&tc->strings, c)) goto emem;
                }
                if (!cb_add(&tc->strings, 0)) goto emem;
                goto skipgetc;
            } else if (c >= '0' && c <= '9') {
                t->type = PB_COMPILER_TOK_TYPE_DATA;
                t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_I32_RAW;
                t->data_raw = tc->strings.len;
                if (!cb_add(&tc->strings, c)) goto emem;
                c = pb__compiler_getc(pbc);
                if (c == 'x' || c == 'X') {
                    int_skipfloat = true;
                    tc->strings.data[tc->strings.len - 1] = 'x';
                    while (1) {
                        c = pb__compiler_getc(pbc);
                        if ((c < '0' || c > '9') && (c < 'A' || c > 'F') && (c < 'a' || c > 'f')) goto ichksuf;
                        if (!cb_add(&tc->strings, c)) goto emem;
                    }
                } else if (c == 'b' || c == 'B') {
                    int_skipfloat = true;
                    tc->strings.data[tc->strings.len - 1] = 'b';
                    while (1) {
                        c = pb__compiler_getc(pbc);
                        if (c != '0' && c != '1') goto ichksuf;
                        if (!cb_add(&tc->strings, c)) goto emem;
                    }
                } else {
                    while (1) {
                        if (c < '0' || c > '9') {
                            ichksuf:;
                            el = t->loc;
                            if (c == 'i' || c == 'I') {
                                t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_I8_RAW;
                                ievalsuf:;
                                char suf[2];
                                c = pb__compiler_getc(pbc);
                                if (c < '0' || c > '9') goto ebadisuf;
                                suf[0] = c;
                                c = pb__compiler_getc(pbc);
                                suf[1] = (c >= '0' && c <= '9') ? c : 0;
                                c = pb__compiler_getc(pbc);
                                if (c >= '0' && c <= '9') goto ebadisuf;
                                if (suf[0] == '1' && suf[1] == '6') {
                                    t->subtype += 1;
                                } else if (suf[0] == '3' && suf[1] == '2') {
                                    t->subtype += 2;
                                } else if (suf[0] == '6' && suf[1] == '4') {
                                    t->subtype += 3;
                                } else if (suf[0] != '8' || suf[1]) {
                                    goto ebadisuf;
                                }
                                goto ichkaftersuf;
                            } else if (c == 'u' || c == 'U') {
                                t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_U8_RAW;
                                goto ievalsuf;
                            }
                            if (!int_skipfloat) {
                                if (c == '.') {
                                    t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_F32_RAW;
                                    goto isfloat;
                                }
                                if (c == 'e' || c == 'E') {
                                    t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_F32_RAW;
                                    goto isfloat_e;
                                }
                                if (c == 'f' || c == 'F') {
                                    t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_F32_RAW;
                                    goto isfloat_f;
                                }
                            } else {
                                if (c == '.') {
                                    pb_compitf_mksrcloc(pbc, 0, &el);
                                    goto ebadchar;
                                }
                            }
                            ichkaftersuf:;
                            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                                goto ebadisuf;
                            } else if (c == '_' || c == ':') {
                                pb_compitf_mksrcloc(pbc, 0, &el);
                                goto ebadchar;
                            }
                            break;
                        }
                        if (!cb_add(&tc->strings, c)) goto emem;
                        c = pb__compiler_getc(pbc);
                    }
                }
                if (!cb_add(&tc->strings, 0)) goto emem;
                goto skipgetc;
            } else if (c == '.') {
                c = pb__compiler_getc(pbc);
                if (c >= '0' && c <= '9') {
                    t->type = PB_COMPILER_TOK_TYPE_DATA;
                    t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_F32_RAW;
                    t->data_raw = tc->strings.len;
                    if (!cb_add(&tc->strings, c)) goto emem;
                    while (1) {
                        c = pb__compiler_getc(pbc);
                        if (c < '0' || c > '9') {
                            el = t->loc;
                            if (c == 'e' || c == 'E') {
                                isfloat_e:;
                                if (!cb_add(&tc->strings, 'e')) goto emem;
                                c = pb__compiler_getc(pbc);
                                if (c == '+' || c == '-') {
                                    if (!cb_add(&tc->strings, c)) goto emem;
                                    c = pb__compiler_getc(pbc);
                                }
                                if (c >= '0' && c <= '9') {
                                    do {
                                        if (!cb_add(&tc->strings, c)) goto emem;
                                        c = pb__compiler_getc(pbc);
                                    } while (c >= '0' && c <= '9');
                                    if (c == 'f' || c == 'F') goto isfloat_f;
                                } else {
                                    pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "No exponent after 'e' in floating-point number", &t->loc);
                                    goto reterr;
                                }
                            } else if (c == 'f' || c == 'F') {
                                isfloat_f:;
                                char suf[2];
                                c = pb__compiler_getc(pbc);
                                if (c < '0' || c > '9') goto ebadfsuf;
                                suf[0] = c;
                                c = pb__compiler_getc(pbc);
                                if (c < '0' || c > '9') goto ebadfsuf;
                                suf[1] = c;
                                c = pb__compiler_getc(pbc);
                                if (c >= '0' && c <= '9') goto ebadfsuf;
                                if (suf[0] == '6' && suf[1] == '4') {
                                    t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_F64_RAW;
                                } else if (suf[0] != '3' || suf[1] != '2') {
                                    goto ebadfsuf;
                                }
                            } else if (c == '.') {
                                goto ebaddot;
                            }
                            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                                goto ebadfsuf;
                            } else if (c == '_' || c == ':') {
                                pb_compitf_mksrcloc(pbc, 0, &el);
                                goto ebadchar;
                            }
                            break;
                        }
                        isfloat:;
                        if (!cb_add(&tc->strings, c)) goto emem;
                    }
                    if (!cb_add(&tc->strings, 0)) goto emem;
                } else {
                    t->type = PB_COMPILER_TOK_TYPE_SYM;
                    pb_compitf_mksrcloc(pbc, 0, &el);
                    if (c == '.') {
                        c = pb__compiler_getc(pbc);
                        if (c == '.') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_DOTDOTDOT;
                            goto getcandcont;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_DOT;
                            VLB_NEXTPTR(*tc, t, 3, 2, goto emem;);
                            t->type = PB_COMPILER_TOK_TYPE_SYM;
                            t->loc = el;
                        }
                    }
                    t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_DOT;
                }
                goto skipgetc;
            } else if (c == '"') {
                if (tc->len >= 2) {
                    struct pb_compiler_tok* tmp = &tc->data[tc->len - 2];
                    if (tmp->type == PB_COMPILER_TOK_TYPE_DATA && tmp->subtype == PB_COMPILER_TOK_SUBTYPE_DATA_STR) {
                        --tc->len;
                        t = tmp;
                        goto sskipsetup;
                    }
                }
                t->type = PB_COMPILER_TOK_TYPE_DATA;
                t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_STR;
                t->data.str.off = tc->strings.len;
                sskipsetup:;
                while (1) {
                    c = pb__compiler_getc(pbc);
                    sskipgetc:;
                    if (c == '"') {
                        t->data.str.len = tc->strings.len - t->data.str.off;
                        break;
                    } else if (c == '\\') {
                        pb_compitf_mksrcloc(pbc, 0, &el);
                        c = pb__compiler_getc(pbc);
                        if (c == -1) goto seunterm;
                        switch ((char)c) {
                            case '0': c = 0; break;
                            case '\\': c = '\\'; break;
                            case '"': c = '"'; break;
                            case 'a': c = '\a'; break;
                            case 'b': c = '\b'; break;
                            case 'e': c = '\x1b'; break;
                            case 'f': c = '\f'; break;
                            case 'n': c = '\n'; break;
                            case 'r': c = '\r'; break;
                            case 't': c = '\t'; break;
                            case 'v': c = '\v'; break;
                            case 'x': {
                                unsigned char tmpc;
                                c = pb__compiler_getc(pbc);
                                if (c >= '0' && c <= '9') {
                                    tmpc = c - '0';
                                } else if (c >= 'A' && c <= 'F') {
                                    tmpc = c - ('A' - 10);
                                } else if (c >= 'a' && c <= 'f') {
                                    tmpc = c - ('a' - 10);
                                } else {
                                    if (!cb_add(&tc->strings, '\\') || !cb_add(&tc->strings, 'x')) goto emem;
                                    pb_compitf_puterrln(pbc, PB_ERROR_NONE, "Unfinished '\\x' escape sequence", &el);
                                    break;
                                }
                                tmpc <<= 4;
                                c = pb__compiler_getc(pbc);
                                if (c >= '0' && c <= '9') {
                                    tmpc |= c - '0';
                                } else if (c >= 'A' && c <= 'F') {
                                    tmpc |= c - ('A' - 10);
                                } else if (c >= 'a' && c <= 'f') {
                                    tmpc |= c - ('a' - 10);
                                } else {
                                    tmpc >>= 4;
                                    goto sxskipgetc;
                                }
                                c = pb__compiler_getc(pbc);
                                sxskipgetc:;
                                if (!cb_add(&tc->strings, tmpc)) goto emem;
                            } goto sskipgetc;
                            case 'u': {
                                uint32_t tmpc;
                                c = pb__compiler_getc(pbc);
                                if (c >= '0' && c <= '9') {
                                    tmpc = c - '0';
                                } else if (c >= 'A' && c <= 'F') {
                                    tmpc = c - ('A' - 10);
                                } else if (c >= 'a' && c <= 'f') {
                                    tmpc = c - ('a' - 10);
                                } else {
                                    if (!cb_add(&tc->strings, '\\') || !cb_add(&tc->strings, 'u')) goto emem;
                                    pb_compitf_puterrln(pbc, PB_ERROR_NONE, "Unfinished '\\u' escape sequence", &el);
                                    break;
                                }
                                for (uint_fast8_t i = 0; i < 7; ++i) {
                                    tmpc <<= 4;
                                    c = pb__compiler_getc(pbc);
                                    if (c >= '0' && c <= '9') {
                                        tmpc |= c - '0';
                                    } else if (c >= 'A' && c <= 'F') {
                                        tmpc |= c - ('A' - 10);
                                    } else if (c >= 'a' && c <= 'f') {
                                        tmpc |= c - ('a' - 10);
                                    } else {
                                        tmpc >>= 4;
                                        goto suskipgetc;
                                    }
                                }
                                c = pb__compiler_getc(pbc);
                                suskipgetc:;
                                unsigned char chars[4];
                                uint_fast8_t charct;
                                if (tmpc < 0x80U) {
                                    charct = 1;
                                    chars[0] = tmpc;
                                } else if (tmpc < 0x800U) {
                                    charct = 2;
                                    chars[0] = 0xC0U | ((tmpc >> 6) & 0x1FU);
                                    chars[1] = 0x80U | (tmpc & 0x3FU);
                                } else if (tmpc < 0x10000U) {
                                    charct = 3;
                                    chars[0] = 0xE0U | ((tmpc >> 12) & 0xFU);
                                    chars[1] = 0x80U | ((tmpc >> 6) & 0x3FU);
                                    chars[2] = 0x80U | (tmpc & 0x3FU);
                                } else if (tmpc < 0x110000U) {
                                    charct = 4;
                                    chars[0] = 0xF0U | ((tmpc >> 24) & 0x7U);
                                    chars[1] = 0x80U | ((tmpc >> 12) & 0x3FU);
                                    chars[2] = 0x80U | ((tmpc >> 6) & 0x3FU);
                                    chars[3] = 0x80U | (tmpc & 0x3FU);
                                } else {
                                    pb_compitf_puterrln(pbc, (*e = PB_ERROR_VALUE), "Unencodable '\\u' escape sequence", &el);
                                    goto reterr;
                                }
                                for (uint_fast8_t i = 0; i < charct; ++i) {
                                    if (!cb_add(&tc->strings, chars[i])) goto emem;
                                }
                            } goto sskipgetc;
                            case '\n': {
                                pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Line continuation is invalid in strings", &el);
                            } goto reterr;
                            default: {
                                if (!cb_add(&tc->strings, '\\')) goto emem;
                                pb_compitf_puterrln(pbc, PB_ERROR_NONE, "Invalid escape sequence", &el);
                            } break;
                        }
                    } else if (c == '\n' || c == -1) {
                        seunterm:;
                        pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Unterminated string", &t->loc);
                        goto reterr;
                    } else if (!c) {
                        pb_compitf_mksrcloc(pbc, 0, &el);
                        goto ebadchar;
                    }
                    if (!cb_add(&tc->strings, c)) goto emem;
                }
            } else {
                t->type = PB_COMPILER_TOK_TYPE_SYM;
                switch ((char)c) {
                    case '!': {
                        c = pb__compiler_getc(pbc);
                        if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_EXCLEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_EXCL;
                            goto skipgetc;
                        }
                    } break;
                    case '#': {
                        tc->preprocinsloc = t->loc;
                        tc->preprocinsstage = '#';
                        --tc->len;
                    } break;
                    case '$': t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_DOLLAR; break;
                    case '%': {
                        c = pb__compiler_getc(pbc);
                        if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_PERCEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_PERC;
                            goto skipgetc;
                        }
                    } break;
                    case '&': {
                        c = pb__compiler_getc(pbc);
                        if (c == '&') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_AMPAMP;
                        } else if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_AMPEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_AMP;
                            goto skipgetc;
                        }
                    } break;
                    case '(': {
                        t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_OPENPAREN;
                        size_t tmp = tc->len - 1;
                        VLB_ADD(tc->brackets, tmp, 3, 2, goto emem;);
                    } break;
                    case ')': {
                        if (!tc->brackets.len || tc->data[tc->brackets.data[tc->brackets.len - 1]].subtype != PB_COMPILER_TOK_SUBTYPE_SYM_OPENPAREN) {
                            el = t->loc;
                            goto eparen;
                        }
                        t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_CLOSEPAREN;
                        --tc->brackets.len;
                    } break;
                    case '*': {
                        c = pb__compiler_getc(pbc);
                        if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_ASTEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_AST;
                            goto skipgetc;
                        }
                    } break;
                    case '+': {
                        c = pb__compiler_getc(pbc);
                        if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_PLUSEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_PLUS;
                            goto skipgetc;
                        }
                    } break;
                    case ',': t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_COMMA; break;
                    case '-': {
                        c = pb__compiler_getc(pbc);
                        if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_DASHEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_DASH;
                            goto skipgetc;
                        }
                    } break;
                    case '/': t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_SLASH; break;
                    case '<': {
                        c = pb__compiler_getc(pbc);
                        if (c == '<') {
                            c = pb__compiler_getc(pbc);
                            if (c == '<') {
                                c = pb__compiler_getc(pbc);
                                if (c == '=') {
                                    t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_LTLTLTEQ;
                                } else {
                                    t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_LTLTLT;
                                    goto skipgetc;
                                }
                            } else if (c == '=') {
                                t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_LTLTEQ;
                            } else {
                                t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_LTLT;
                                goto skipgetc;
                            }
                        } else if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_LTEQ;
                        } else if (c == '>') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_LTGT;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_LT;
                            goto skipgetc;
                        }
                    } break;
                    case '=': {
                        c = pb__compiler_getc(pbc);
                        if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_EQEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_EQ;
                            goto skipgetc;
                        }
                    } break;
                    case '>': {
                        c = pb__compiler_getc(pbc);
                        if (c == '>') {
                            c = pb__compiler_getc(pbc);
                            if (c == '>') {
                                c = pb__compiler_getc(pbc);
                                if (c == '=') {
                                    t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_GTGTGTEQ;
                                } else {
                                    t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_GTGTGT;
                                    goto skipgetc;
                                }
                            } else if (c == '=') {
                                t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_GTGTEQ;
                            } else {
                                t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_GTGT;
                                goto skipgetc;
                            }
                        } else if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_GTEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_GT;
                            goto skipgetc;
                        }
                    } break;
                    case '?': t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_QUESTION; break;
                    case '@': t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_AT; break;
                    case '[': {
                        t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_OPENSQB;
                        size_t tmp = tc->len - 1;
                        VLB_ADD(tc->brackets, tmp, 3, 2, goto emem;);
                    } break;
                    case ']': {
                        if (!tc->brackets.len || tc->data[tc->brackets.data[tc->brackets.len - 1]].subtype != PB_COMPILER_TOK_SUBTYPE_SYM_OPENSQB) {
                            el = t->loc;
                            goto esqb;
                        }
                        t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_CLOSESQB;
                        --tc->brackets.len;
                    } break;
                    case '^': {
                        c = pb__compiler_getc(pbc);
                        if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_CARETEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_CARET;
                            goto skipgetc;
                        }
                    } break;
                    case '{': {
                        t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_OPENCURB;
                        size_t tmp = tc->len - 1;
                        VLB_ADD(tc->brackets, tmp, 3, 2, goto emem;);
                    } break;
                    case '|': {
                        c = pb__compiler_getc(pbc);
                        if (c == '|') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_BARBAR;
                        } else if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_BAREQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_BAR;
                            goto skipgetc;
                        }
                    } break;
                    case '}': {
                        if (!tc->brackets.len || tc->data[tc->brackets.data[tc->brackets.len - 1]].subtype != PB_COMPILER_TOK_SUBTYPE_SYM_OPENCURB) {
                            el = t->loc;
                            goto ecurb;
                        }
                        t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_CLOSECURB;
                        --tc->brackets.len;
                    } break;
                    case '~': {
                        c = pb__compiler_getc(pbc);
                        if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_TILDEEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_TILDE;
                            goto skipgetc;
                        }
                    } break;
                    default: {
                        el = t->loc;
                    } goto ebadchar;
                }
            }
        }
        getcandcont:;
        c = pb__compiler_getc(pbc);
        skipgetc:;
        while (c == ' ' || c == '\t') {
            c = pb__compiler_getc(pbc);
        }
        if (c == '\n' || (!preproc && c == ';') || c == -1) {
            endstmt:;
            if (tc->brackets.len) {
                struct pb_compiler_tok* t = &tc->data[tc->brackets.data[tc->brackets.len - 1]];
                char bc, ec;
                if (t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_OPENPAREN) {
                    bc = '(';
                    ec = ')';
                } else if (t->subtype == PB_COMPILER_TOK_SUBTYPE_SYM_OPENSQB) {
                    bc = '[';
                    ec = ']';
                } else {
                    bc = '{';
                    ec = '}';
                }
                pb_compitf_puterr(pbc, (*e = PB_ERROR_SYNTAX), "", &t->loc);
                cb_add(pbc->err, '\'');
                cb_add(pbc->err, bc);
                cb_addstr(pbc->err, "' without matching '");
                cb_add(pbc->err, ec);
                cb_add(pbc->err, '\'');
                cb_add(pbc->err, '\n');
                goto reterr;
            }
            *cptr = c;
            return 1;
        }
    }
    *cptr = c;
    return 0;

    ebadchar:;
    pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Unexpected character", &el);
    goto reterr;
    ebadisuf:;
    pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Invalid suffix on integer", &el);
    goto reterr;
    ebadfsuf:;
    pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Invalid suffix on floating-point number", &el);
    goto reterr;
    ebaddot:;
    pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Too many decimal points in floating-point number", &el);
    goto reterr;

    {
        char bc, ec;
        eparen:;
        bc = '(';
        ec = ')';
        goto ebracket;
        esqb:;
        bc = '[';
        ec = ']';
        goto ebracket;
        ecurb:;
        bc = '{';
        ec = '}';
        ebracket:;
        pb_compitf_puterr(pbc, (*e = PB_ERROR_SYNTAX), "", &el);
        cb_add(pbc->err, '\'');
        cb_add(pbc->err, ec);
        cb_addstr(pbc->err, "' without matching '");
        cb_add(pbc->err, bc);
        cb_add(pbc->err, '\'');
        cb_add(pbc->err, '\n');
        goto reterr;
    }

    emem:;
    pb_compitf_puterrln(pbc, (*e = PB_ERROR_MEMORY), NULL, NULL);

    reterr:;
    return -1;
}

static bool pb__tok_inspreprocvar(struct pb_compiler* pbc, struct pb_compiler_tokcoll* tc, bool preproc, int* cptr, enum pb_error* e) {
    int c = *cptr;
    struct pb_compiler_srcloc el;

    if (tc->preprocinsstage == '#') {
        while (c == ' ' || c == '\t') {
            c = pb__compiler_getc(pbc);
        }
        if (c == '(') {
            tc->preprocinsstage = '(';
            c = pb__compiler_getc(pbc);
            goto foundopenparen;
        }
        if (c == '\n' || (!preproc && c == ';') || c == -1) goto einc;
        if (c == '\\') goto ret;
        pb_compitf_mksrcloc(pbc, 0, &el);
        pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Expected '('", &el);
        goto retfalse;
    } else if (tc->preprocinsstage == '(') {
        foundopenparen:;
        while (c == ' ' || c == '\t') {
            c = pb__compiler_getc(pbc);
        }
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == ':') {
            pb_compitf_mksrcloc(pbc, 0, &tc->preprocinsidloc);
            tc->preprocinsidpos = tc->strings.len;
            if (!cb_add(&tc->strings, c)) goto emem;
            while (1) {
                c = pb__compiler_getc(pbc);
                if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && c != '_') {
                    if (tc->strings.data[tc->strings.len - 1] == ':') {
                        pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Invalid identifier", &el);
                        goto retfalse;
                    }
                    if (c != ':' && (c < '0' || c > '9')) break;
                }
                if (!cb_add(&tc->strings, c)) goto emem;
            }
            if (!cb_add(&tc->strings, 0)) goto emem;
            tc->preprocinsstage = 'I';
            goto foundid;
        }
        if (c == '\n' || (!preproc && c == ';') || c == -1) goto einc;
        if (c == '\\') goto ret;
        pb_compitf_mksrcloc(pbc, 0, &el);
        pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Expected indentifier", &tc->preprocinsidloc);
        goto retfalse;
    } else if (tc->preprocinsstage == 'I') {
        foundid:;
        while (c == ' ' || c == '\t') {
            c = pb__compiler_getc(pbc);
        }
        if (c == ')') {
            tc->preprocinsstage = 0;
            c = pb__compiler_getc(pbc);
        } else {
            if (c == '\n' || (!preproc && c == ';') || c == -1) goto einc;
            if (c == '\\') goto ret;
            pb_compitf_mksrcloc(pbc, 0, &el);
            pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Expected ')'", &el);
            goto retfalse;
        }
    }

    const char* name = tc->strings.data + tc->preprocinsidpos;
    uint32_t namecrc = strcasecrc32(name);
    tc->strings.len = tc->preprocinsidpos;
    //printf("PREPROCINS: {%s}\n", name);
    struct pb_compiler_tok* t;
    VLB_NEXTPTR(*tc, t, 3, 2, goto emem;);
    const struct pb_preproc_data* vd;
    for (size_t i = 0; i < pbc->preprocvars.len; ++i) {
        struct pb_preproc_var* v = &pbc->preprocvars.data[i];
        if (v->data.type == PB_PREPROC_TYPE_VOID) break;
        if (v->namecrc == namecrc && !strcasecmp(pbc->preprocvars.names.data + v->name, name)) {
            vd = &v->data;
            goto foundvar;
        }
    }
    for (size_t i = 0; i < pbc->opt->preprocvarct; ++i) {
        const struct pb_compiler_opt_preprocvar* v = &pbc->opt->preprocvars[i];
        if (v->namecrc == namecrc && !strcasecmp(v->name, name)) {
            vd = &v->data;
            goto foundvar;
        }
    }
    pb_compitf_puterr(pbc, (*e = PB_ERROR_DEF), "Preprocessor variable '", &tc->preprocinsidloc);
    cb_addstr(pbc->err, name);
    cb_addstr(pbc->err, "' not found\n");
    goto retfalse;
    foundvar:;
    switch (vd->type) {
        DEFAULTCASE(PB_PREPROC_TYPE_STR): {
            t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_STR;
            t->data.str.off = tc->strings.len;
            t->data.str.len = vd->str.len;
            if (!cb_addpartstr(&tc->strings, vd->str.data, vd->str.len)) goto emem;
        } break;
        case PB_PREPROC_TYPE_BOOL: {t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_BOOL; t->data.b = vd->b;} break;
        case PB_PREPROC_TYPE_I8: {t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_I8; t->data.i8 = vd->i8;} break;
        case PB_PREPROC_TYPE_I16: {t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_I16; t->data.i16 = vd->i16;} break;
        case PB_PREPROC_TYPE_I32: {t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_I32; t->data.i32 = vd->i32;} break;
        case PB_PREPROC_TYPE_I64: {t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_I64; t->data.i64 = vd->i64;} break;
        case PB_PREPROC_TYPE_U8: {t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_U8; t->data.u8 = vd->u8;} break;
        case PB_PREPROC_TYPE_U16: {t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_U16; t->data.u16 = vd->u16;} break;
        case PB_PREPROC_TYPE_U32: {t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_U32; t->data.u32 = vd->u32;} break;
        case PB_PREPROC_TYPE_U64: {t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_U64; t->data.u64 = vd->u64;} break;
        case PB_PREPROC_TYPE_F32: {t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_F32; t->data.f32 = vd->f32;} break;
        case PB_PREPROC_TYPE_F64: {t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_F64; t->data.f64 = vd->f64;} break;
    }
    t->type = PB_COMPILER_TOK_TYPE_DATA;

    ret:;
    *cptr = c;
    return true;

    einc:;
    pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Incomplete preprocessor variable insertion", &tc->preprocinsloc);
    goto retfalse;
    emem:;
    pb_compitf_puterrln(pbc, (*e = PB_ERROR_MEMORY), NULL, NULL);
    retfalse:;
    return false;
}
