#include <psrc/crc.h>

#include <stdio.h>
#include <inttypes.h>
#ifndef _WIN32
    #include <unistd.h>
#else
    #include <io.h>
    #define isatty _isatty
    #define fileno _fileno
#endif

int main(int argc, char** argv) {
    if (argc > 1) {
        int i = 1;
        do {
            printf(
                "argv[%d]:\n"
                "    strcrc32     - %08" PRIX32 "\n"
                "    strcasecrc32 - %08" PRIX32 "\n"
                "    strcrc64     - %016" PRIX64 "\n"
                "    strcasecrc64 - %016" PRIX64 "\n",
                i,
                strcrc32(argv[i]), strcasecrc32(argv[i]),
                strcrc64(argv[i]), strcasecrc64(argv[i])
            );
            ++i;
        } while (i < argc);
        if (isatty(fileno(stdin))) return 0;
    }
    uint32_t c32 = 0;
    uint64_t c64 = 0;
    while (1) {
        char buf[512];
        size_t r = fread(buf, 1, sizeof(buf), stdin);
        if (!r) break;
        c32 = ccrc32(c32, buf, r);
        c64 = ccrc64(c64, buf, r);
    }
    printf(
        "stdin:\n"
        "    crc32 - %08" PRIX32 "\n"
        "    crc64 - %016" PRIX64 "\n",
        c32, c64
    );
    return 0;
}
