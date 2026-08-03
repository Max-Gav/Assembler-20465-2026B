/*
 * Module: main.c
 * Validates command-line .as paths, processes every source independently,
 * and returns a nonzero aggregate status if any source fails. It assumes no
 * interactive input; assembler.c performs each pipeline and files.c validates
 * the required suffix.
 */
#include "main.h"
#include "assembler.h"
#include "files.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    int i;
    int all_success = 1;
    if (argc < 2) {
        fprintf(stderr, "usage: assembler file1.as [file2.as ...]\n");
        return 1;
    }
    for (i = 1; i < argc; ++i) {
        if (!has_as_extension(argv[i])) {
            fprintf(stderr, "%s: error: input filename must end in .as\n", argv[i]);
            all_success = 0;
            continue;
        }
        if (!assemble_source(argv[i]))
            all_success = 0;
    }
    return all_success ? 0 : 1;
}
