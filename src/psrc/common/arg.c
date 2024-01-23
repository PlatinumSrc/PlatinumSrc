#include "arg.h"

int args_getarg(struct args* a, struct charbuf* cb, struct charbuf* err) {
    if (a->arg == a->argc) return 0;
    char* arg = a->argv[a->arg];
    if (arg[0] != '-') {
        cb_addstr(err, "Expected an option");
        return -1;
    }
    int i = 1 + (arg[1] == '-');
    if (!arg[i] || arg[i] == '=') {
        cb_addstr(err, "Expected an option name after -");
        if (i == 2) cb_add(err, '-');
        return -1;
    }
    while (1) {
        char c = arg[i];
        if (!c) {++a->arg; a->index = -1; return 1;}
        if (c == '=') {a->index = i + 1; return 1;}
        cb_add(cb, c);
        if (c == '\\') {
            c = arg[++i];
            if (!c) {cb_add(cb, '\\'); ++a->arg; a->index = -1; return 1;}
            if (c == '\\') {cb_add(cb, '\\');}
            else if (c == '=') {cb_add(cb, '=');}
            else {cb_add(cb, '\\'); cb_add(cb, c);}
        }
        ++i;
    }
}

int args_getargval(struct args* a, bool r, int s, struct charbuf* cb, struct charbuf* err) {
    if (r) {
        if (a->index < 0) {
            if (s == 0) {
                cb_addstr(err, "Value required in the same argument (-option=value)");
                return -1;
            }
            if (a->arg == a->argc) {
                cb_addstr(err, "A value is required");
                return -1;
            }
            cb_addstr(cb, a->argv[a->arg]);
        } else {
            if (s == 1) {
                cb_addstr(err, "Value required to be in the next argument (-option value)");
                return -1;
            }
            cb_addstr(cb, &a->argv[a->arg][a->index]);
        }
    } else {
        if (a->index < 0) return 0;
        cb_addstr(cb, &a->argv[a->arg][a->index]);
    }
    ++a->arg;
    a->index = -1;
    return 1;
}

void args_init(struct args* a, int c, char** v) {
    a->arg = 0;
    a->index = -1;
    a->argc = c - 1;
    a->argv = v + 1;
}
