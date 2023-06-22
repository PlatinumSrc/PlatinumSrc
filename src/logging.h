#ifndef ENGINE_LOG_H
#define ENGINE_LOG_H

enum loglevel {
    LOG_PLAIN,
    LOG_TASK,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_CRIT,
};

void plog(enum loglevel, char*, ...);

#endif
