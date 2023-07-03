#ifndef AUXLIB_LOGGING_H
#define AUXLIB_LOGGING_H

enum loglevel {
    LOGLVL_PLAIN,
    LOGLVL_TASK,
    LOGLVL_INFO,
    LOGLVL_WARN,
    LOGLVL_ERROR,
    LOGLVL_CRIT,
};

void bplog(const char*, const char*, unsigned, enum loglevel, char*, ...);
#define plog(lvl, ...) bplog(__func__, __FILE__, __LINE__, lvl, __VA_ARGS__)

#endif
