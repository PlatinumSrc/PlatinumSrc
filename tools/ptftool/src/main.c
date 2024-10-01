#include <string.h>
#include <stdio.h>

int ptf_convert(char*, int, char**);
int ptf_convertback(char*, int, char**);
int ptf_info(char*, int, char**);

int main(int argc, char** argv) {
    if (argc < 2 || !strcmp(argv[1], "--help")) {
        printf("USAGE: %s <COMMAND> ...\n", argv[0]);
        putchar('\n');
        puts("COMMANDS:");
        puts("    c, convert [ARGUMENT]... <FILE>...");
        puts("    Convert from an existing image format (using stb_image) to PTF");
        puts("        -o, --overwrite     Overwrite output");
        puts("        -a, --force-alpha   Force an alpha channel");
        puts("        -A, --remove-alpha  Remove the alpha channel");
        puts("    C, convert-back [ARGUMENT]... <FILE>...");
        puts("    Convert from PTF to an existing image format (using stb_image_write)");
        puts("        -o, --overwrite     Overwrite output");
        puts("        -f, --format        Change output format {png (default), bmp, tga, jpg}");
        puts("        -q, --quality       Change output quality (PNG and JPG only; 1-100 for JPG, default is 90)");
        puts("    i, info <FILE>...");
        puts("    Show info about a PTF file");
        return 0;
    } else if (!strcmp(argv[1], "c") || !strcmp(argv[1], "convert")) {
        if (argc == 2) {
            fprintf(stderr, "%s: No files provided\n", argv[0]);
            return 1;
        }
        return ptf_convert(argv[0], argc - 2, &argv[2]);
    } else if (!strcmp(argv[1], "C") || !strcmp(argv[1], "convert-back")) {
        if (argc == 2) {
            fprintf(stderr, "%s: No files provided\n", argv[0]);
            return 1;
        }
        return ptf_convertback(argv[0], argc - 2, &argv[2]);
    } else if (!strcmp(argv[1], "i") || !strcmp(argv[1], "info")) {
        if (argc == 2) {
            fprintf(stderr, "%s: No files provided\n", argv[0]);
            return 1;
        }
        return ptf_info(argv[0], argc - 2, &argv[2]);
    }
    fprintf(stderr, "%s: Unknown command '%s'\n", argv[0], argv[1]);
    return 1;
}
