/*
 * Module: preprocessor.c
 * Collects legal mcro definitions and expands standalone invocations.  Macro
 * definitions disappear from .am.  State and owned body lines are destroyed
 * after every source, which prevents cross-file macro leakage. It assumes an
 * ordinary text source, uses parser.c for reserved-name rules, and supplies
 * the expanded source consumed by assembler.c.
 */
#include "preprocessor.h"
#include "assembler_types.h"
#include "parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MacroLine {
    char *text;
    struct MacroLine *next;
} MacroLine;

typedef struct Macro {
    char *name;
    MacroLine *body;
    MacroLine *tail;
    struct Macro *next;
} Macro;

/* Allocates an owned exact copy; the caller releases it with free. */
static char *duplicate_text(const char *text)
{
    char *copy = (char *)malloc(strlen(text) + 1);
    if (copy != NULL)
        strcpy(copy, text);
    return copy;
}

/* Performs a case-sensitive lookup in one source file's macro list. */
static Macro *find_macro(Macro *head, const char *name)
{
    while (head != NULL) {
        if (strcmp(head->name, name) == 0)
            return head;
        head = head->next;
    }
    return NULL;
}

/* Releases macro names, body lines, and list nodes after one source. */
static void destroy_macros(Macro *head)
{
    Macro *next_macro;
    MacroLine *line;
    MacroLine *next_line;
    while (head != NULL) {
        next_macro = head->next;
        line = head->body;
        while (line != NULL) {
            next_line = line->next;
            free(line->text);
            free(line);
            line = next_line;
        }
        free(head->name);
        free(head);
        head = next_macro;
    }
}

/* Reads one physical line, strips CR/LF, and reports overlength distinctly. */
static int read_line(FILE *file, char *buffer, int *too_long)
{
    int ch;
    size_t length;
    *too_long = 0;
    if (fgets(buffer, MAX_SOURCE_LINE_LENGTH + 3, file) == NULL)
        return 0;
    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n')
        buffer[--length] = '\0';
    else if (!feof(file)) {
        *too_long = 1;
        while ((ch = fgetc(file)) != '\n' && ch != EOF)
            ;
    }
    if (length > 0 && buffer[length - 1] == '\r')
        buffer[--length] = '\0';
    if (length > MAX_SOURCE_LINE_LENGTH)
        *too_long = 1;
    return 1;
}

/* Extracts up to three whitespace-separated words from a line. */
static int words_of(const char *text, char words[3][MAX_TOKEN_LENGTH])
{
    int count = 0;
    const char *start;
    size_t length;
    while (*text != '\0') {
        while (isspace((unsigned char)*text)) ++text;
        if (*text == '\0' || *text == ';') break;
        start = text;
        while (*text != '\0' && !isspace((unsigned char)*text)) ++text;
        length = (size_t)(text - start);
        if (count >= 3 || length >= MAX_TOKEN_LENGTH)
            return 3;
        memcpy(words[count], start, length);
        words[count][length] = '\0';
        ++count;
    }
    return count;
}

/* Reports a one-based macro-stage diagnostic for the original source path. */
static void macro_error(const char *path, int line, const char *message)
{
    fprintf(stderr, "%s:%d: error: %s\n", path, line, message);
}

/* First scan validates and stores definitions while continuing after errors. */
static int collect_macros(FILE *source, const char *path, Macro **macros)
{
    char buffer[MAX_SOURCE_LINE_LENGTH + 3];
    char words[3][MAX_TOKEN_LENGTH];
    int count;
    int line_number = 0;
    int too_long;
    int errors = 0;
    Macro *current = NULL;
    Macro *macro;
    MacroLine *body_line;
    while (read_line(source, buffer, &too_long)) {
        ++line_number;
        if (too_long) {
            macro_error(path, line_number, "source line exceeds 80 characters");
            ++errors;
            continue;
        }
        count = words_of(buffer, words);
        if (current != NULL) {
            if (count > 0 && strcmp(words[0], "mcroend") == 0) {
                if (count != 1) {
                    macro_error(path, line_number, "mcroend must appear alone");
                    ++errors;
                }
                current = NULL;
            } else {
                body_line = (MacroLine *)calloc(1, sizeof(MacroLine));
                if (body_line == NULL || (body_line->text = duplicate_text(buffer)) == NULL) {
                    free(body_line);
                    macro_error(path, line_number, "out of memory while storing macro");
                    ++errors;
                    current = NULL;
                } else {
                    if (current->tail != NULL) current->tail->next = body_line;
                    else current->body = body_line;
                    current->tail = body_line;
                }
            }
        } else if (count > 0 && strcmp(words[0], "mcro") == 0) {
            if (count != 2 || !is_valid_symbol_name(words[1])) {
                macro_error(path, line_number, "mcro requires one legal, non-reserved name");
                ++errors;
                continue;
            }
            if (find_macro(*macros, words[1]) != NULL) {
                macro_error(path, line_number, "duplicate macro definition");
                ++errors;
                continue;
            }
            macro = (Macro *)calloc(1, sizeof(Macro));
            if (macro == NULL || (macro->name = duplicate_text(words[1])) == NULL) {
                free(macro);
                macro_error(path, line_number, "out of memory while defining macro");
                ++errors;
                continue;
            }
            macro->next = *macros;
            *macros = macro;
            current = macro;
        } else if (count > 0 && strcmp(words[0], "mcroend") == 0) {
            macro_error(path, line_number, "mcroend without mcro");
            ++errors;
        }
    }
    if (current != NULL) {
        macro_error(path, line_number, "unterminated macro definition");
        ++errors;
    }
    return errors == 0;
}

/* Rejects a label whose name is also a macro, including forward definitions. */
static int validate_macro_label_conflicts(FILE *source, const char *path,
                                          Macro *macros)
{
    char buffer[MAX_SOURCE_LINE_LENGTH + 3];
    char words[3][MAX_TOKEN_LENGTH];
    char label[MAX_TOKEN_LENGTH];
    int count;
    int line_number = 0;
    int too_long;
    int errors = 0;
    size_t length;
    while (read_line(source, buffer, &too_long)) {
        ++line_number;
        if (too_long)
            continue;
        count = words_of(buffer, words);
        if (count == 0)
            continue;
        length = strlen(words[0]);
        if (length > 1 && words[0][length - 1] == ':') {
            memcpy(label, words[0], length - 1);
            label[length - 1] = '\0';
            if (find_macro(macros, label) != NULL) {
                macro_error(path, line_number, "label conflicts with macro name");
                ++errors;
            }
        }
    }
    return errors == 0;
}

/* Second scan omits definitions and writes expanded standalone invocations. */
static int expand_macros(FILE *source, FILE *output, const char *path, Macro *macros)
{
    char buffer[MAX_SOURCE_LINE_LENGTH + 3];
    char words[3][MAX_TOKEN_LENGTH];
    int count;
    int line_number = 0;
    int too_long;
    int inside = 0;
    int errors = 0;
    Macro *macro;
    MacroLine *line;
    while (read_line(source, buffer, &too_long)) {
        ++line_number;
        count = words_of(buffer, words);
        if (inside) {
            if (count > 0 && strcmp(words[0], "mcroend") == 0)
                inside = 0;
            continue;
        }
        if (count > 0 && strcmp(words[0], "mcro") == 0) {
            inside = 1;
            continue;
        }
        macro = count > 0 ? find_macro(macros, words[0]) : NULL;
        if (macro != NULL) {
            if (count != 1) {
                macro_error(path, line_number, "macro invocation must appear alone");
                ++errors;
                continue;
            }
            for (line = macro->body; line != NULL; line = line->next)
                fprintf(output, "%s\n", line->text);
        } else {
            fprintf(output, "%s\n", buffer);
        }
    }
    return errors == 0 && !ferror(output);
}

int preprocess_source(const char *source_path, const char *am_path)
{
    FILE *source = fopen(source_path, "r");
    FILE *output;
    Macro *macros = NULL;
    int success;
    if (source == NULL) {
        fprintf(stderr, "%s: error: cannot open source file\n", source_path);
        return 0;
    }
    success = collect_macros(source, source_path, &macros);
    rewind(source);
    if (!validate_macro_label_conflicts(source, source_path, macros))
        success = 0;
    if (success) {
        rewind(source);
        output = fopen(am_path, "wb");
        if (output == NULL) {
            fprintf(stderr, "%s: error: cannot create macro output\n", source_path);
            success = 0;
        } else {
            success = expand_macros(source, output, source_path, macros);
            if (fclose(output) != 0)
                success = 0;
        }
    }
    fclose(source);
    destroy_macros(macros);
    if (!success)
        remove(am_path);
    return success;
}
