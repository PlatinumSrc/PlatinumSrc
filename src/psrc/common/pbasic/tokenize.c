static int tokenize(struct pb_compiler* pbc, struct pb_compiler_tokcoll* tc, bool preproc, int* cptr, enum pb_error* e) {
    struct pb_compiler_srcloc el;
    int c = *cptr;
    while (1) {
        bool int_skipfloat = false;
        if (c == '\'') {
            while (1) {
                c = pb_compitf_getc_local(pbc);
                if (c == '\n' || c == -1) goto endstmt;
                if (!c) {
                    pb_compitf_mksrcloc(pbc, 0, &el);
                    goto ebadchar;
                }
            }
        } else if (c == '`') {
            pb_compitf_mksrcloc(pbc, 0, &el);
            while (1) {
                int c = pb_compitf_getc_local(pbc);
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
            c = pb_compitf_getc_local(pbc);
            if (c == ' ' || c == '\t') {
                pb_compitf_mksrcloc(pbc, 0, &el);
                pb_compitf_puterrln(pbc, PB_ERROR_NONE, "Whitespace after line continuation", &el);
                do {
                    c = pb_compitf_getc_local(pbc);
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
            struct pb_compiler_tok* t;
            VLB_NEXTPTR(*tc, t, 3, 2, goto emem;);
            pb_compitf_mksrcloc(pbc, 0, &t->loc);
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == ':') {
                t->type = PB_COMPILER_TOK_TYPE_NAME;
                t->name = tc->strings.len;
                if (!cb_add(&tc->strings, c)) {
                    --tc->len;
                    goto emem;
                }
                while (1) {
                    c = pb_compitf_getc_local(pbc);
                    if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && c != '_') {
                        if (tc->strings.data[tc->strings.len - 1] == ':') {
                            --tc->len;
                            pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Invalid identifier", &t->loc);
                            goto reterr;
                        }
                        if (c != ':' && (c < '0' || c > '9')) break;
                    }
                    if (!cb_add(&tc->strings, c)) {
                        --tc->len;
                        goto emem;
                    }
                }
                if (!cb_add(&tc->strings, 0)) {
                    --tc->len;
                    goto emem;
                }
                goto skipgetc;
            } else if (c >= '0' && c <= '9') {
                t->type = PB_COMPILER_TOK_TYPE_DATA;
                t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_I32;
                t->data = tc->strings.len;
                if (!cb_add(&tc->strings, c)) {
                    --tc->len;
                    goto emem;
                }
                c = pb_compitf_getc_local(pbc);
                if (c == 'x' || c == 'X') {
                    int_skipfloat = true;
                    tc->strings.data[tc->strings.len - 1] = 'x';
                    while (1) {
                        c = pb_compitf_getc_local(pbc);
                        if ((c < '0' || c > '9') && (c < 'A' || c > 'F') && (c < 'a' || c > 'f')) goto ichksuf;
                        if (!cb_add(&tc->strings, c)) {
                            --tc->len;
                            goto emem;
                        }
                    }
                } else if (c == 'b' || c == 'B') {
                    int_skipfloat = true;
                    tc->strings.data[tc->strings.len - 1] = 'b';
                    while (1) {
                        c = pb_compitf_getc_local(pbc);
                        if (c != '0' && c != '1') goto ichksuf;
                        if (!cb_add(&tc->strings, c)) {
                            --tc->len;
                            goto emem;
                        }
                    }
                } else {
                    while (1) {
                        if (c < '0' || c > '9') {
                            ichksuf:;
                            el = t->loc;
                            if (c == 'i' || c == 'I') {
                                t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_I8;
                                ievalsuf:;
                                char suf[2];
                                c = pb_compitf_getc_local(pbc);
                                if (c < '0' || c > '9') goto iebadsuf;
                                suf[0] = c;
                                c = pb_compitf_getc_local(pbc);
                                suf[1] = (c >= '0' && c <= '9') ? c : 0;
                                c = pb_compitf_getc_local(pbc);
                                if (c >= '0' && c <= '9') goto iebadsuf;
                                if (suf[0] == '1' && suf[1] == '6') {
                                    t->subtype += 1;
                                } else if (suf[0] == '3' && suf[1] == '2') {
                                    t->subtype += 2;
                                } else if (suf[0] == '6' && suf[1] == '4') {
                                    t->subtype += 3;
                                } else if (suf[0] != '8' || suf[1]) {
                                    goto iebadsuf;
                                }
                                goto ichkaftersuf;
                            } else if (c == 'u' || c == 'U') {
                                t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_U8;
                                goto ievalsuf;
                            }
                            if (!int_skipfloat) {
                                if (c == '.') {
                                    t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_F32;
                                    goto isfloat;
                                }
                                if (c == 'e' || c == 'E') {
                                    t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_F32;
                                    goto isfloat_e;
                                }
                                if (c == 'f' || c == 'F') {
                                    t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_F32;
                                    goto isfloat_f;
                                }
                            } else {
                                if (c == '.') {
                                    --tc->len;
                                    pb_compitf_mksrcloc(pbc, 0, &el);
                                    goto ebadchar;
                                }
                            }
                            ichkaftersuf:;
                            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                                iebadsuf:;
                                --tc->len;
                                goto ebadisuf;
                            } else if (c == '_' || c == ':') {
                                --tc->len;
                                pb_compitf_mksrcloc(pbc, 0, &el);
                                goto ebadchar;
                            }
                            break;
                        }
                        if (!cb_add(&tc->strings, c)) {
                            --tc->len;
                            goto emem;
                        }
                        c = pb_compitf_getc_local(pbc);
                    }
                }
                if (!cb_add(&tc->strings, 0)) {
                    --tc->len;
                    goto emem;
                }
                goto skipgetc;
            } else if (c == '.') {
                c = pb_compitf_getc_local(pbc);
                if (c >= '0' && c <= '9') {
                    t->type = PB_COMPILER_TOK_TYPE_DATA;
                    t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_F32;
                    t->data = tc->strings.len;
                    if (!cb_add(&tc->strings, c)) {
                        --tc->len;
                        goto emem;
                    }
                    while (1) {
                        c = pb_compitf_getc_local(pbc);
                        if (c < '0' || c > '9') {
                            el = t->loc;
                            if (c == 'e' || c == 'E') {
                                isfloat_e:;
                                if (!cb_add(&tc->strings, 'e')) {
                                    --tc->len;
                                    goto emem;
                                }
                                c = pb_compitf_getc_local(pbc);
                                if (c == '+' || c == '-') {
                                    if (!cb_add(&tc->strings, c)) {
                                        --tc->len;
                                        goto emem;
                                    }
                                    c = pb_compitf_getc_local(pbc);
                                }
                                if (c >= '0' && c <= '9') {
                                    do {
                                        if (!cb_add(&tc->strings, c)) {
                                            --tc->len;
                                            goto emem;
                                        }
                                        c = pb_compitf_getc_local(pbc);
                                    } while (c >= '0' && c <= '9');
                                    if (c == 'f' || c == 'F') goto isfloat_f;
                                } else {
                                    --tc->len;
                                    pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "No exponent after 'e' in floating-point number", &t->loc);
                                    goto reterr;
                                }
                            } else if (c == 'f' || c == 'F') {
                                isfloat_f:;
                                char suf[2];
                                c = pb_compitf_getc_local(pbc);
                                if (c < '0' || c > '9') goto febadsuf;
                                suf[0] = c;
                                c = pb_compitf_getc_local(pbc);
                                if (c < '0' || c > '9') goto febadsuf;
                                suf[1] = c;
                                c = pb_compitf_getc_local(pbc);
                                if (c >= '0' && c <= '9') goto febadsuf;
                                if (suf[0] == '6' && suf[1] == '4') {
                                    t->subtype = PB_COMPILER_TOK_SUBTYPE_DATA_F64;
                                } else if (suf[0] != '3' || suf[1] != '2') {
                                    goto febadsuf;
                                }
                            } else if (c == '.') {
                                --tc->len;
                                goto ebaddot;
                            }
                            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                                febadsuf:;
                                --tc->len;
                                goto ebadfsuf;
                            } else if (c == '_' || c == ':') {
                                --tc->len;
                                pb_compitf_mksrcloc(pbc, 0, &el);
                                goto ebadchar;
                            }
                            break;
                        }
                        isfloat:;
                        if (!cb_add(&tc->strings, c)) {
                            --tc->len;
                            goto emem;
                        }
                    }
                    if (!cb_add(&tc->strings, 0)) {
                        --tc->len;
                        goto emem;
                    }
                } else {
                    t->type = PB_COMPILER_TOK_TYPE_SYM;
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
                t->data = tc->strings.len;
                sskipsetup:;
                while (1) {
                    c = pb_compitf_getc_local(pbc);
                    if (c == '"') {
                        t->data_strlen = tc->strings.len - t->data;
                        break;
                    } else if (c == '\\') {
                        pb_compitf_mksrcloc(pbc, 0, &el);
                        c = pb_compitf_getc_local(pbc);
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
                                c = pb_compitf_getc_local(pbc);
                                if (c >= '0' && c <= '9') {
                                    tmpc = c - '0';
                                } else if (c >= 'A' || c <= 'F') {
                                    tmpc = c - ('a' - 10);
                                } else if (c >= 'a' || c <= 'f') {
                                    tmpc = c - ('a' - 10);
                                } else {
                                    if (!cb_add(&tc->strings, '\\') || !cb_add(&tc->strings, 'x')) {
                                        --tc->len;
                                        goto emem;
                                    }
                                    goto swbadseq;
                                }
                                tmpc <<= 4;
                                c = pb_compitf_getc_local(pbc);
                                if (c >= '0' && c <= '9') {
                                    tmpc |= c - '0';
                                } else if (c >= 'A' || c <= 'F') {
                                    tmpc |= c - ('a' - 10);
                                } else if (c >= 'a' || c <= 'f') {
                                    tmpc |= c - ('a' - 10);
                                } else {
                                    tmpc >>= 4;
                                    goto sskipgetc;
                                }
                                c = pb_compitf_getc_local(pbc);
                                sskipgetc:;
                                if (!cb_add(&tc->strings, tmpc)) {
                                    --tc->len;
                                    goto emem;
                                }
                            } continue;
                            case '\n': {
                                --tc->len;
                                pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Line continuation is invalid in strings", &t->loc);
                            } goto reterr;
                            default: {
                                if (!cb_add(&tc->strings, '\\')) {
                                    --tc->len;
                                    goto emem;
                                }
                                swbadseq:;
                                pb_compitf_puterrln(pbc, PB_ERROR_NONE, "Invalid escape sequence", &t->loc);
                            } break;
                        }
                    } else if (c == '\n' || c == -1) {
                        seunterm:;
                        --tc->len;
                        pb_compitf_puterrln(pbc, (*e = PB_ERROR_SYNTAX), "Unterminated string", &t->loc);
                        goto reterr;
                    } else if (!c) {
                        --tc->len;
                        pb_compitf_mksrcloc(pbc, 0, &el);
                        goto ebadchar;
                    }
                    if (!cb_add(&tc->strings, c)) {
                        --tc->len;
                        goto emem;
                    }
                }
            } else {
                t->type = PB_COMPILER_TOK_TYPE_SYM;
                switch ((char)c) {
                    case '!': {
                        c = pb_compitf_getc_local(pbc);
                        if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_EXCLEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_EXCL;
                            goto skipgetc;
                        }
                    } break;
                    case '$': t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_DOLLAR; break;
                    case '%': {
                        c = pb_compitf_getc_local(pbc);
                        if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_PERCEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_PERC;
                            goto skipgetc;
                        }
                    } break;
                    case '&': {
                        c = pb_compitf_getc_local(pbc);
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
                        VLB_ADD(tc->brackets, tmp, 3, 2, --tc->len; goto emem;);
                    } break;
                    case ')': {
                        if (!tc->brackets.len || tc->data[tc->brackets.data[tc->brackets.len - 1]].subtype != PB_COMPILER_TOK_SUBTYPE_SYM_OPENPAREN) {
                            --tc->len;
                            pb_compitf_mksrcloc(pbc, 0, &el);
                            goto eparen;
                        }
                        t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_CLOSEPAREN;
                        --tc->brackets.len;
                    } break;
                    case '*': {
                        c = pb_compitf_getc_local(pbc);
                        if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_ASTEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_AST;
                            goto skipgetc;
                        }
                    } break;
                    case '+': {
                        c = pb_compitf_getc_local(pbc);
                        if (c == '+') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_PLUSPLUS;
                        } else if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_PLUSEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_PLUS;
                            goto skipgetc;
                        }
                    } break;
                    case ',': t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_COMMA; break;
                    case '-': {
                        c = pb_compitf_getc_local(pbc);
                        if (c == '-') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_DASHDASH;
                        } else if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_DASHEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_DASH;
                            goto skipgetc;
                        }
                    } break;
                    case '/': t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_SLASH; break;
                    case '<': {
                        c = pb_compitf_getc_local(pbc);
                        if (c == '<') {
                            c = pb_compitf_getc_local(pbc);
                            if (c == '<') {
                                c = pb_compitf_getc_local(pbc);
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
                        c = pb_compitf_getc_local(pbc);
                        if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_EQEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_EQ;
                            goto skipgetc;
                        }
                    } break;
                    case '>': {
                        c = pb_compitf_getc_local(pbc);
                        if (c == '>') {
                            c = pb_compitf_getc_local(pbc);
                            if (c == '>') {
                                c = pb_compitf_getc_local(pbc);
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
                    case '@': t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_AT; break;
                    case '[': {
                        t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_OPENSQB;
                        size_t tmp = tc->len - 1;
                        VLB_ADD(tc->brackets, tmp, 3, 2, --tc->len; goto emem;);
                    } break;
                    case ']': {
                        if (!tc->brackets.len || tc->data[tc->brackets.data[tc->brackets.len - 1]].subtype != PB_COMPILER_TOK_SUBTYPE_SYM_OPENSQB) {
                            --tc->len;
                            pb_compitf_mksrcloc(pbc, 0, &el);
                            goto esqb;
                        }
                        t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_CLOSESQB;
                        --tc->brackets.len;
                    } break;
                    case '^': {
                        c = pb_compitf_getc_local(pbc);
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
                        VLB_ADD(tc->brackets, tmp, 3, 2, --tc->len; goto emem;);
                    } break;
                    case '|': {
                        c = pb_compitf_getc_local(pbc);
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
                            --tc->len;
                            pb_compitf_mksrcloc(pbc, 0, &el);
                            goto ecurb;
                        }
                        t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_CLOSECURB;
                        --tc->brackets.len;
                    } break;
                    case '~': {
                        c = pb_compitf_getc_local(pbc);
                        if (c == '=') {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_TILDEEQ;
                        } else {
                            t->subtype = PB_COMPILER_TOK_SUBTYPE_SYM_TILDE;
                            goto skipgetc;
                        }
                    } break;
                    default: {
                        --tc->len;
                        pb_compitf_mksrcloc(pbc, 0, &el);
                    } goto ebadchar;
                }
            }
        }
        getcandcont:;
        c = pb_compitf_getc_local(pbc);
        skipgetc:;
        while (c == ' ' || c == '\t') {
            c = pb_compitf_getc_local(pbc);
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

#if DEBUG(1)
static void dbg_printtok(struct pb_compiler_tokcoll* tc) {
    if (!tc->len) {
        puts("---");
        return;
    }
    static const char* const symstr[] = {
        "!", "!=",
        "$",
        "%", "%=",
        "&", "&&", "&=",
        "(",
        ")",
        "*", "*=",
        "+", "++", "+=",
        ",",
        "-", "--", "-=",
        ".",
        "/",
        "<", "<<", "<=", "<>", "<<<", "<<=", "<<<=",
        "=", "==",
        ">", ">>", ">=", ">>>", ">>=", ">>>=",
        "@",
        "[",
        "]",
        "^", "^=",
        "{",
        "|", "||", "|=",
        "}",
        "~", "~="
    };
    static const char* const datastr[] = {
        "STR",
        "I8", "I16", "I32", "I64",
        "U8", "U16", "U32", "U64",
        "F32", "F64"
    };
    size_t i = 0;
    while (1) {
        struct pb_compiler_tok* t = &tc->data[i];
        switch (t->type) {
            case PB_COMPILER_TOK_TYPE_NAME:
                printf("NAME{%s}", tc->strings.data + t->name);
                break;
            case PB_COMPILER_TOK_TYPE_SYM:
                printf("SYM{%s}", symstr[t->subtype]);
                break;
            case PB_COMPILER_TOK_TYPE_DATA:
                if (t->subtype == PB_COMPILER_TOK_SUBTYPE_DATA_STR) {
                    char* s = tc->strings.data + t->data;
                    printf("DATA:%s{", datastr[t->subtype]);
                    for (size_t i = 0; i < t->data_strlen; ++i) {
                        char c = s[i];
                        if (!c) {
                            putchar('}');
                            putchar('\\');
                            putchar('0');
                            putchar('{');
                        } else if (c == '\n') {
                            putchar('}');
                            putchar('\\');
                            putchar('n');
                            putchar('{');
                        } else if (c == '\r') {
                            putchar('}');
                            putchar('\\');
                            putchar('r');
                            putchar('{');
                        } else {
                            putchar(s[i]);
                        }
                    }
                    putchar('}');
                } else {
                    printf("DATA:%s{%s}", datastr[t->subtype], tc->strings.data + t->data);
                }
                break;
        }
        ++i;
        if (i == tc->len) break;
        putchar(' ');
    }
    putchar('\n');
}
#endif
