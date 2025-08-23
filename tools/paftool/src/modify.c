#include <stdio.h>

const char* paftool_modify_help =
   "INPUT COMMAND ... - Modify the INPUT archive\n"
   "        cp, copy [OPTION]... [--] SOURCE... DEST - Copy the SOURCE path(s) to the DEST path\n"
   "            -f, --force             - Overwrite\n"
   "            -r, --recursive         - Copy directories and files recursively\n"
   "            -v, --verbose           - For each copy, give details about it after that copy\n"
   "        ln, link [OPTION]... [--] TARGET LINK - Make a LINK pointing to TARGET\n"
   "            -f, --force             - Overwrite\n"
   "            -v, --verbose           - Give details about the link\n"
   "        mv, move [OPTION]... [--] SOURCE... DEST - Move the SOURCE path(s) to the DEST path\n"
   "            -f, --force             - Overwrite\n"
   "            -v, --verbose           - For each move, give details about it after that move\n"
   "        o, optimize [OPTION]... [--] OUTPUT - Create an optimized version of the INPUT archive\n"
   "            -v, --verbose           - Give details\n"
   "            -w, --writing           - Optimize for writing instead of size\n"
   "        rm, remove [OPTION]... [--] PATH...\n"
   "            -r, --recursive         - Remove directories and files recursively\n"
   "            -v, --verbose           - For each path, display it after removal\n"
   "        u, update [OPTION]... [--] PAF_PATH FS_PATH - Update a file or directory in the archive\n"
   "            -c, --compress WHAT     - Compress the given type of files (same as create's -c option)\n"
   "            -C, --no-compress WHAT  - Do not compress the given type of files (same as create's -C option)\n"
   "            -e, --existing          - Only update files that already exist in the archive (implies -f)\n"
   "            -f, --force             - Overwrite\n"
   "            -l, --resolve-links     - Detect and attempt to turn symlinks into links in the archive\n"
   "            -L, --ignore-links      - Detect and ignore symlinks\n"
   "            -n, --new               - Do not update files that already exist in the archive\n"
   "            -v, --verbose           - For each path updated, give details about it after that update"
;

int paftool_modify(char* argv0, int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "%s: Missing 'INPUT' param for 'modify' command\n", argv0);
        return 1;
    }
    if (argc < 2) {
        fprintf(stderr, "%s: Missing 'COMMAND' param for 'modify' command\n", argv0);
        return 1;
    }
    return 0;
}
