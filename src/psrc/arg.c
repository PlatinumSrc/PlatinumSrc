#include "arg.h"

#include <stdio.h>

int args_getopt(struct args* a, struct charbuf* cb, struct charbuf* err) {
    if (a->arg == a->argc) return 0;
    char* arg = a->argv[a->arg];
    if (arg[0] != '-') {
        cb_addstr(err, "Expected an option for argument ");
        char tmp[12];
        snprintf(tmp, sizeof(tmp), "%d", a->arg + 1);
        cb_addstr(err, tmp);
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
        if (c == '\\') {
            c = arg[++i];
            if (!c) {cb_add(cb, '\\'); ++a->arg; a->index = -1; return 1;}
            if (c == '\\') {cb_add(cb, '\\');}
            else if (c == '=') {cb_add(cb, '=');}
            else {cb_add(cb, '\\'); cb_add(cb, c);}
        } else {
            cb_add(cb, c);
        }
        ++i;
    }
}

int args_getoptval(struct args* a, int n, int s, struct charbuf* cb, struct charbuf* err) {
    if (n == 1) {
        if (a->index < 0) {
            if (s == 0) {
                cb_addstr(err, "Value required in the same argument (-option=value)");
                return -1;
            }
            if (a->arg == a->argc) {
                cb_addstr(err, "Option requires a value");
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
    } else if (n == 0) {
        if (a->index >= 0) {
            cb_addstr(err, "Option does not accept a value");
            return -1;
        }
        return 0;
    } else {
        if (a->index < 0) return 0;
        cb_addstr(cb, &a->argv[a->arg][a->index]);
    }
    ++a->arg;
    a->index = -1;
    return 1;
}

int args_getvar(struct args* a, struct charbuf* cb, struct charbuf* err) {
    (void)err;
    if (a->arg == a->argc) return 0;
    char* arg = a->argv[a->arg];
    if (arg[0] == '-') return 0;
    int i = 0;
    while (1) {
        char c = arg[i];
        if (!c) {++a->arg; a->index = -1; return 1;}
        if (c == '=') {a->index = i + 1; return 1;}
        if (c == '\\') {
            c = arg[++i];
            if (!c) {cb_add(cb, '\\'); ++a->arg; a->index = -1; return 1;}
            if (c == '\\') {cb_add(cb, '\\');}
            else if (c == '-') {cb_add(cb, '-');}
            else if (c == '=') {cb_add(cb, '=');}
            else {cb_add(cb, '\\'); cb_add(cb, c);}
        } else {
            cb_add(cb, c);
        }
        ++i;
    }
}

int args_getvarval(struct args* a, int n, struct charbuf* cb, struct charbuf* err) {
    if (a->index < 0) {
        if (n == 1) {
            cb_addstr(err, "Variable requires a value");
            return -1;
        } else {
            return 0;
        }
    } else if (n == 0) {
        cb_addstr(err, "Variable does not accept a value");
        return -1;
    }
    cb_addstr(cb, &a->argv[a->arg][a->index]);
    ++a->arg;
    a->index = -1;
    return 1;
}

void args_init(struct args* a, int c, char** v) {
    a->arg = 0;
    a->index = -1;
    a->argc = c;
    a->argv = v;
}
