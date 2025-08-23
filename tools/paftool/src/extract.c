#include <stdio.h>

const char* paftool_extract_help =
   "INPUT [OPTION]... [--] [PATH]... - Extract all or a list of PATHs in the INPUT archive\n"
   "        -f, --force         - Overwrite\n"
   "        -o, --output OUT    - Extract the file or files to OUT\n"
   "            OUT     - Extracts to OUT if extracting a single file and a file path is provided,\n"
   "                      extracts into OUT if extracting any number of files and a directory path is provided\n"
   "        -v, --verbose       - For each extraction, give details about it after that extraction"
;

int paftool_extract(char* argv0, int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "%s: Missing 'INPUT' param for 'extract' command\n", argv0);
        return 1;
    }
    if (argc < 2) {
        fprintf(stderr, "%s: Missing 'OUTPUT' param for 'extract' command\n", argv0);
        return 1;
    }
    return 0;
}
