/*
 * Module: files.c
 * Owns path derivation, stale-output cleanup, and exact 2026B output formats.
 * Object bytes are already host-independent when they reach this module. It
 * consumes AssemblyContext images produced by assembler.c and assumes output
 * paths are derived from an already validated `.as` source path.
 */
#include "files.h"
#include <stdlib.h>
#include <string.h>

int has_as_extension(const char *path)
{
    size_t length;
    if (path == NULL)
        return 0;
    length = strlen(path);
    return length > 3 && strcmp(path + length - 3, ".as") == 0;
}

char *replace_extension(const char *source_path, const char *suffix)
{
    size_t source_length = strlen(source_path);
    size_t suffix_length = strlen(suffix);
    char *path;
    if (!has_as_extension(source_path))
        return NULL;
    path = (char *)malloc(source_length - 3 + suffix_length + 1);
    if (path == NULL)
        return NULL;
    memcpy(path, source_path, source_length - 3);
    strcpy(path + source_length - 3, suffix);
    return path;
}

void remove_stale_outputs(const char *source_path, int remove_am)
{
    static const char *suffixes[] = {".am", ".ob", ".ent", ".ext"};
    int first = remove_am ? 0 : 1;
    int i;
    char *path;
    for (i = first; i < 4; ++i) {
        path = replace_extension(source_path, suffixes[i]);
        if (path != NULL) {
            remove(path);
            free(path);
        }
    }
}

/* Writes one memory line containing at most four bytes. */
static void write_memory_line(FILE *file, long address,
                              const unsigned char *bytes, int count)
{
    int i;
    fprintf(file, "%04ld", address);
    for (i = 0; i < count; ++i)
        fprintf(file, " %02X", (unsigned int)bytes[i]);
    fputc('\n', file);
}

/* Creates the mandatory object file with code followed immediately by data. */
static int write_object(const AssemblyContext *context, const char *path)
{
    FILE *file = fopen(path, "wb");
    long offset;
    long total = context->code.size + context->data.size;
    unsigned char group[4];
    int count;
    int i;
    if (file == NULL)
        return 0;
    fprintf(file, "%ld %ld\n", context->code.size, context->data.size);
    offset = 0;
    while (offset < total) {
        count = 0;
        for (i = 0; i < 4 && offset + i < total; ++i) {
            if (offset + i < context->code.size)
                group[i] = context->code.bytes[offset + i];
            else
                group[i] = context->data.bytes[offset + i - context->code.size];
            ++count;
        }
        write_memory_line(file, CODE_START_ADDRESS + offset, group, count);
        offset += count;
    }
    if (fclose(file) != 0)
        return 0;
    return 1;
}

/* Emits entries in table order; the official format does not prescribe order. */
static int write_entries(const AssemblyContext *context, const char *path)
{
    Symbol *symbol;
    FILE *file = NULL;
    for (symbol = context->symbols; symbol != NULL; symbol = symbol->next) {
        if (symbol->is_entry) {
            if (file == NULL) {
                file = fopen(path, "wb");
                if (file == NULL)
                    return 0;
            }
            fprintf(file, "%s %04ld\n", symbol->name, symbol->value);
        }
    }
    return file == NULL || fclose(file) == 0;
}

/* Preserves every external use, including repeated uses of one symbol. */
static int write_externals(const AssemblyContext *context, const char *path)
{
    ExternalUse *use;
    FILE *file;
    if (context->external_uses == NULL)
        return 1;
    file = fopen(path, "wb");
    if (file == NULL)
        return 0;
    for (use = context->external_uses; use != NULL; use = use->next)
        fprintf(file, "%s %04ld\n", use->name, use->address);
    return fclose(file) == 0;
}

int write_output_files(const AssemblyContext *context)
{
    char *ob = replace_extension(context->source_path, ".ob");
    char *ent = replace_extension(context->source_path, ".ent");
    char *ext = replace_extension(context->source_path, ".ext");
    int success = 0;
    if (ob != NULL && ent != NULL && ext != NULL && write_object(context, ob) &&
        write_entries(context, ent) && write_externals(context, ext))
        success = 1;
    if (!success) {
        if (ob != NULL) remove(ob);
        if (ent != NULL) remove(ent);
        if (ext != NULL) remove(ext);
    }
    free(ob);
    free(ent);
    free(ext);
    return success;
}
