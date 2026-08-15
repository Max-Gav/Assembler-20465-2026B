/* Command-line validation and processing of each requested source file. */
#include "main.h"
#include "assembler.h"
#include "files.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    int i;
    int all_success = 1;
    if (argc < 2) {
        printf("usage: assembler file1.as [file2.as ...]\n");
        return 1;
    }
    for (i = 1; i < argc; ++i) {
        if (!has_as_extension(argv[i])) {
            printf("%s: error: input filename must end in .as\n", argv[i]);
            all_success = 0;
            continue;
        }
        if (!assemble_source(argv[i]))
            all_success = 0;
    }
    return all_success ? 0 : 1;
}
