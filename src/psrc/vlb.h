#ifndef PSRC_VLB_H
#define PSRC_VLB_H

#include <stddef.h>

#define VLB(T) {\
    T* data;\
    uintptr_t len;\
    uintptr_t size;\
}

#define VLB_OOM_NOP

#define VLB__EXP(b, en, ed, onoom) do {\
    if ((b).len == (b).size) {\
        {\
            register uintptr_t VLB__EXP__t = (b).size * en / ed;\
            if (VLB__EXP__t != (b).size) (b).size = VLB__EXP__t;\
            else ++(b).size;\
        }\
        void* VLB__EXP__t = realloc((b).data, (b).size * sizeof(*(b).data));\
        if (!VLB__EXP__t) {onoom}\
        (b).data = VLB__EXP__t;\
    }\
} while (0)

#define VLB_INIT(b, sz, onoom) do {\
    (b).size = (sz);\
    (b).len = 0;\
    void* VLB_INIT__t = malloc((b).size * sizeof(*(b).data));\
    if (!VLB_INIT__t) {onoom}\
    (b).data = VLB_INIT__t;\
} while (0)
#define VLB_FREE(b) free((b).data)

#define VLB_ADD(b, d, en, ed, onoom) do {\
    VLB__EXP((b), en, ed, onoom);\
    (b).data[(b).len++] = (d);\
} while (0)
#define VLB_NEXTPTR(b, o, en, ed, onoom) do {\
    VLB__EXP((b), en, ed, onoom);\
    o = &(b).data[(b).len++];\
} while (0)

#define VLB_EXP(b, a, en, ed, onoom) do {\
    (b).len += (a);\
    if ((b).len > (b).size) {\
        do {\
            register uintptr_t VLB_EXP__t = (b).size * en / ed;\
            if (VLB_EXP__t != (b).size) (b).size = VLB_EXP__t;\
            else ++(b).size;\
        } while ((b).len > (b).size);\
        void* VLB_EXP__p = realloc((b).data, (b).size * sizeof(*(b).data));\
        if (!VLB_EXP__p) {onoom}\
        (b).data = VLB_EXP__p;\
    }\
} while (0)
#define VLB_SHRINK(b, onoom) do {\
    if ((b).len != (b).size) {\
        (b).size = (b).len;\
        void* VLB_SHRINK__p = realloc((b).data, (b).size * sizeof(*(b).data));\
        if ((b).size && !VLB_SHRINK__p) {onoom}\
        (b).data = VLB_SHRINK__p;\
    }\
} while (0)

#endif
