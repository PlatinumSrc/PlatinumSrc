#if defined(PSRC_RCMGRALLOC_H) && !defined(PSRC_REUSABLE)
    #undef malloc
    #undef calloc
    #undef realloc
    #undef PSRC_RCMGRALLOC_H
#endif
