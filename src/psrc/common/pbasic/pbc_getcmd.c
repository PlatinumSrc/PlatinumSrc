static bool pbc_getcmd(struct pbc* ctx, struct charbuf* err, const char* name, unsigned nl, unsigned nc, bool func, bool useret, struct pbc_databuf* ops) {
    int c = name[0];
    if (c >= 'A' || c <= 'Z') c |= 0x20;
    ++name;
    if (func) {
        
    } else {
        switch (c) {
            case 'i':
                if (!strcasecmp(name, "f")) {
                    if (pbc_getcond_inline(ctx, err)) {
                        return true;
                    } else {
                        return false;
                    }
                }
                break;
            case 's':
                if (!strcasecmp(name, "ub")) {
                    struct charbuf cb;
                    cb_init(&cb, 32);
                    unsigned dim;
                    {
                        enum pbtype rt = pbc_gettype_cb_inline(ctx, true, &dim, NULL, NULL, &cb, err);
                        if (rt < 0) {
                            cb_dump(&cb);
                            return false;
                        }
                        cb_clear(&cb);
                        if (!pbc_getname_inline(ctx, &cb, err)) {
                            cb_dump(&cb);
                            return false;
                        }
                        //printf("SUB: {%s}\n", cb.data);
                        //pbc_pushscope(ctx, PBC_SCOPE_SUB);
                        //int sub = pbc_getsub(ctx, cb.data);
                        //pbc_db_put8(ops, PBVM_OP_DEFSUB);
                        //pbc_db_put32(ops, sub);
                        //pbc_db_put8(ops, rt);
                        //pbc_db_put32(ops, dim);
                        if (pbc_geteos_inline(ctx, false, err)) {
                            //pbc_db_put32(ops, 0);
                            cb_dump(&cb);
                            return true;
                        }
                        cb_clear(&cb);
                    }
                    //unsigned argcoff = pbc_db_fake32(ops);
                    unsigned argc = 1;
                    while (1) {
                        enum pbtype t = pbc_gettype_cb_inline(ctx, true, &dim, &nl, &nc, &cb, err);
                        if (t < 0) {
                            cb_dump(&cb);
                            return false;
                        } else if (t == PBTYPE_VOID) {
                            if (err) pbc_mkerr(ctx, err, nl, nc, "Type error", "Subroutine argument cannot be 'VOID'");
                            cb_dump(&cb);
                            return false;
                        }
                        cb_clear(&cb);
                        if (!pbc_getname_inline(ctx, &cb, err)) {
                            cb_dump(&cb);
                            return false;
                        }
                        //if (!pbc_setscopearg(ctx, cb.data, err)) {
                        //    cb_dump(&cb);
                        //    return false;
                        //}
                        //printf("ARG: {%d, %s}\n", t, cb.data);
                        //pbc_db_put8(ops, t);
                        //pbc_db_put32(ops, dim);
                        int r = pbc_getsep_inline(ctx, false, err);
                        if (r == -1) {
                            cb_dump(&cb);
                            return false;
                        } else if (!r) {
                            break;
                        }
                        ++argc;
                        cb_clear(&cb);
                    }
                    cb_dump(&cb);
                    if (!pbc_geteos_inline(ctx, true, err)) return false;
                    //pbc_db_put32at(ops, argcoff, argc);
                    return true;
                }
                break;
        }
    }
    --name;
    if (err) pbc_mkerr(ctx, err, nl, nc, (func) ? "Unrecognized function" : "Unrecognized command", name);
    return false;
}
