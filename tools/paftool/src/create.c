#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <common/paf.h>

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
    fclose(f);
    return 0;
}
