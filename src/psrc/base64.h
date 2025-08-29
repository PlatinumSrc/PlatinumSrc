#ifndef PSRC_BASE64_H
#define PSRC_BASE64_H

#include "string.h"

#include <stdbool.h>

#define BASE64ENC_ALLOCSZ(BASE64ENC_ALLOCSZ__sz) ((BASE64ENC_ALLOCSZ__sz + 2) / 3 * 4)
#define BASE64DEC_ALLOCSZ(BASE64DEC_ALLOCSZ__sz) ((BASE64DEC_ALLOCSZ__sz) * 3 / 4)

struct base64encstate {
    char tmp[3];
    char tmplen;
};
struct base64decstate {
    char tmp[4];
    char tmplen;
};

#define BASE64ENCPART_FLAG_FIRST (1U << 0)
#define BASE64ENCPART_FLAG_LAST (1U << 1)
#define BASE64DECPART_FLAG_FIRST (1U << 0)

size_t base64enc(const void*, size_t, char* out);
size_t base64dec(const void*, size_t, char* out);
size_t base64enccb(const void*, size_t, struct charbuf* out);
size_t base64deccb(const void*, size_t, struct charbuf* out);
size_t base64encpart(struct base64encstate*, unsigned flags, const void*, size_t, char* out);
size_t base64decpart(struct base64decstate*, unsigned flags, const void*, size_t, char* out);
size_t base64encpartcb(struct base64encstate*, unsigned flags, const void*, size_t, struct charbuf* out);
size_t base64decpartcb(struct base64decstate*, unsigned flags, const void*, size_t, struct charbuf* out);

#endif
