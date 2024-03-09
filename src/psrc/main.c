#if defined(PSRC_MODULE_ENGINE)
    #include "main/engine.c"
#elif defined(PSRC_MODULE_SERVER)
    #include "main/server.c"
#elif defined(PSRC_MODULE_EDITOR)
    #include "main/editor.c"
#endif
