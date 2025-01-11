#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <common/paf.h>
#include <common/byteorder.h>

const char* paftool_create_help =
   "<OUTPUT> [PATH]... - Create an archive out of the provided PATHs"
;

int paftool_create(char* argv0, int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "%s: Missing 'OUTPUT' param for 'create' command\n", argv0);
        return 1;
    }
    FILE* f = fopen(argv[0], "w+b");
    if (!f) {
        fprintf(stderr, "%s: Could not open output: %s\n", argv0, strerror(errno));
        return 1;
    }
    fputc('P', f);
    fputc('A', f);
    fputc('F', f);
    fputc(PAF_VER, f);
    if (argc == 1) {
        static const unsigned freespaceslots = 16;
        static const unsigned rootdirentries = 16;
        uint32_t tmpu32 = swaple32(24 + freespaceslots * 8);
        fwrite(&tmpu32, 4, 1, f);
        tmpu32 = swaple32(freespaceslots);
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        tmpu32 = 0;
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        for (unsigned i = 0; i < freespaceslots; ++i) {
            fwrite(&tmpu32, 4, 1, f);
            fwrite(&tmpu32, 4, 1, f);
        }

        tmpu32 = swaple32(rootdirentries);
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        tmpu32 = 0;
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        for (unsigned i = 0; i < rootdirentries; ++i) {
            fputc(0, f);
            fwrite(&tmpu32, 4, 1, f);
            fwrite(&tmpu32, 4, 1, f);
            fwrite(&tmpu32, 4, 1, f);
        }
    } else {
        uint32_t tmpu32 = swaple32(24);
        fwrite(&tmpu32, 4, 1, f);
        tmpu32 = 0;
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
    }
    fclose(f);
    return 0;
}
