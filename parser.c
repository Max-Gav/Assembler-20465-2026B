/*
 * Module: parser.c
 * Tokenizes and validates the complete 2026B line grammar.  It rejects
 * malformed commas and trailing text early while leaving symbol resolution
 * and final instruction encoding to the assembly passes. Input is a NUL-
 * terminated logical line of at most 80 characters. Shared records come from
 * assembler_types.h and consumers are preprocessor.c and assembler.c.
 */
#include "parser.h"
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const InstructionDesc instructions[] = {
    {"add", FORM_R3, 0, 1, 3}, {"sub", FORM_R3, 0, 2, 3},
    {"and", FORM_R3, 0, 3, 3}, {"or", FORM_R3, 0, 4, 3},
    {"nor", FORM_R3, 0, 5, 3}, {"move", FORM_R2, 1, 1, 2},
    {"mvhi", FORM_R2, 1, 2, 2}, {"mvlo", FORM_R2, 1, 3, 2},
    {"addi", FORM_I_ARITH, 10, 0, 3}, {"subi", FORM_I_ARITH, 11, 0, 3},
    {"andi", FORM_I_ARITH, 12, 0, 3}, {"ori", FORM_I_ARITH, 13, 0, 3},
    {"nori", FORM_I_ARITH, 14, 0, 3}, {"bne", FORM_I_BRANCH, 15, 0, 3},
    {"beq", FORM_I_BRANCH, 16, 0, 3}, {"blt", FORM_I_BRANCH, 17, 0, 3},
    {"bgt", FORM_I_BRANCH, 18, 0, 3}, {"lb", FORM_I_MEMORY, 19, 0, 3},
    {"sb", FORM_I_MEMORY, 20, 0, 3}, {"lw", FORM_I_MEMORY, 21, 0, 3},
    {"sw", FORM_I_MEMORY, 22, 0, 3}, {"lh", FORM_I_MEMORY, 23, 0, 3},
    {"sh", FORM_I_MEMORY, 24, 0, 3}, {"jmp", FORM_J, 30, 0, 1},
    {"la", FORM_J, 31, 0, 1}, {"call", FORM_J, 32, 0, 1},
    {"hlt", FORM_HLT, 63, 0, 0}
};

/* ANSI C90 replacement for bounded diagnostic formatting.
 * All callers provide a 160-byte destination and bounded source tokens. */
static void format_message(char *message, int message_size, const char *format, ...)
{
    va_list arguments;
    (void)message_size;
    va_start(arguments, format);
    vsprintf(message, format, arguments);
    va_end(arguments);
}

const InstructionDesc *find_instruction(const char *name)
{
    size_t i;
    for (i = 0; i < sizeof(instructions) / sizeof(instructions[0]); ++i) {
        if (strcmp(name, instructions[i].name) == 0)
            return &instructions[i];
    }
    return NULL;
}

int is_reserved_name(const char *name)
{
    static const char *directives[] = {
        ".db", ".dh", ".dw", ".asciz", ".entry", ".extern",
        "mcro", "mcroend"
    };
    size_t i;
    if (find_instruction(name) != NULL)
        return 1;
    for (i = 0; i < sizeof(directives) / sizeof(directives[0]); ++i) {
        if (strcmp(name, directives[i]) == 0)
            return 1;
    }
    return 0;
}

int is_valid_symbol_name(const char *name)
{
    size_t i;
    size_t length = strlen(name);
    if (length == 0 || length > MAX_LABEL_LENGTH || !isalpha((unsigned char)name[0]))
        return 0;
    for (i = 1; i < length; ++i) {
        if (!isalnum((unsigned char)name[i]))
            return 0;
    }
    return !is_reserved_name(name);
}

int parse_decimal(const char *text, long minimum, long maximum, long *value)
{
    char *end;
    long parsed;
    if (text == NULL || *text == '\0')
        return 0;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || parsed < minimum || parsed > maximum)
        return 0;
    *value = parsed;
    return 1;
}

int parse_register(const char *text, int *reg)
{
    long value;
    const char *digit;
    if (text == NULL || text[0] != '$' || text[1] == '\0')
        return 0;
    for (digit = text + 1; *digit != '\0'; ++digit) {
        if (!isdigit((unsigned char)*digit))
            return 0;
    }
    if (!parse_decimal(text + 1, REGISTER_MIN, REGISTER_MAX, &value))
        return 0;
    *reg = (int)value;
    return 1;
}

/* Copies a whitespace-delimited word and advances the cursor. */
static int take_word(char **cursor, char *word)
{
    char *start;
    size_t length;
    while (isspace((unsigned char)**cursor))
        ++*cursor;
    start = *cursor;
    while (**cursor != '\0' && !isspace((unsigned char)**cursor))
        ++*cursor;
    length = (size_t)(*cursor - start);
    if (length == 0 || length >= MAX_TOKEN_LENGTH)
        return 0;
    memcpy(word, start, length);
    word[length] = '\0';
    return 1;
}

/* Splits an exact comma-separated operand list; empty fields are errors. */
static int split_operands(char *cursor, ParsedLine *parsed, char *message,
                          int message_size)
{
    char *start;
    char *end;
    size_t length;
    int count = 0;
    while (isspace((unsigned char)*cursor))
        ++cursor;
    if (*cursor == '\0') {
        parsed->operand_count = 0;
        return 1;
    }
    for (;;) {
        while (isspace((unsigned char)*cursor))
            ++cursor;
        if (*cursor == ',' || *cursor == '\0') {
            format_message(message, message_size, "empty operand or misplaced comma");
            return 0;
        }
        start = cursor;
        while (*cursor != '\0' && *cursor != ',')
            ++cursor;
        end = cursor;
        while (end > start && isspace((unsigned char)end[-1]))
            --end;
        length = (size_t)(end - start);
        if (count >= MAX_OPERANDS || length == 0 || length >= MAX_TOKEN_LENGTH) {
            format_message(message, message_size, "too many or oversized operands");
            return 0;
        }
        memcpy(parsed->operands[count], start, length);
        parsed->operands[count][length] = '\0';
        ++count;
        if (*cursor == '\0')
            break;
        ++cursor;
        if (*cursor == '\0') {
            format_message(message, message_size, "trailing comma");
            return 0;
        }
    }
    parsed->operand_count = count;
    return 1;
}

/* Parses and validates the quoted payload of .asciz. */
static int parse_asciz(char *cursor, ParsedLine *parsed, char *message,
                       int message_size)
{
    char *closing;
    size_t length;
    while (isspace((unsigned char)*cursor))
        ++cursor;
    if (*cursor != '"') {
        format_message(message, message_size, ".asciz requires a quoted string");
        return 0;
    }
    closing = strchr(cursor + 1, '"');
    if (closing == NULL) {
        format_message(message, message_size, "unterminated .asciz string");
        return 0;
    }
    length = (size_t)(closing - cursor - 1);
    memcpy(parsed->string_value, cursor + 1, length);
    parsed->string_value[length] = '\0';
    ++closing;
    while (isspace((unsigned char)*closing))
        ++closing;
    if (*closing != '\0') {
        format_message(message, message_size, "trailing text after .asciz string");
        return 0;
    }
    return 1;
}

int parse_source_line(const char *text, ParsedLine *parsed,
                      char *message, int message_size)
{
    char buffer[MAX_SOURCE_LINE_LENGTH + 2];
    char first[MAX_TOKEN_LENGTH];
    char *cursor;
    char *colon;
    size_t length;
    memset(parsed, 0, sizeof(*parsed));
    parsed->kind = LINE_INVALID;
    length = strlen(text);
    if (length > MAX_SOURCE_LINE_LENGTH) {
        format_message(message, message_size, "source line exceeds 80 characters");
        return 0;
    }
    strcpy(buffer, text);
    cursor = buffer;
    while (isspace((unsigned char)*cursor))
        ++cursor;
    if (*cursor == '\0' || *cursor == ';') {
        parsed->kind = LINE_EMPTY;
        return 1;
    }
    if (!take_word(&cursor, first)) {
        format_message(message, message_size, "missing or oversized operation");
        return 0;
    }
    colon = strchr(first, ':');
    if (colon != NULL) {
        if (colon[1] != '\0') {
            format_message(message, message_size, "label colon must terminate the label");
            return 0;
        }
        *colon = '\0';
        if (!is_valid_symbol_name(first)) {
            format_message(message, message_size, "invalid or reserved label '%s'", first);
            return 0;
        }
        strcpy(parsed->label, first);
        if (!take_word(&cursor, first)) {
            format_message(message, message_size, "label has no statement");
            return 0;
        }
    }
    parsed->instruction = find_instruction(first);
    if (parsed->instruction != NULL) {
        parsed->kind = LINE_INSTRUCTION;
        if (!split_operands(cursor, parsed, message, message_size))
            return 0;
        if (parsed->operand_count != parsed->instruction->operand_count) {
            format_message(message, message_size, "instruction %s expects %d operand(s)",
                           first, parsed->instruction->operand_count);
            return 0;
        }
        return 1;
    }
    if (strcmp(first, ".asciz") == 0) {
        parsed->kind = LINE_ASCIZ;
        return parse_asciz(cursor, parsed, message, message_size);
    }
    if (strcmp(first, ".db") == 0) parsed->kind = LINE_DB;
    else if (strcmp(first, ".dh") == 0) parsed->kind = LINE_DH;
    else if (strcmp(first, ".dw") == 0) parsed->kind = LINE_DW;
    else if (strcmp(first, ".entry") == 0) parsed->kind = LINE_ENTRY;
    else if (strcmp(first, ".extern") == 0) parsed->kind = LINE_EXTERN;
    else {
        format_message(message, message_size, "unknown operation or directive '%s'", first);
        return 0;
    }
    if (!split_operands(cursor, parsed, message, message_size))
        return 0;
    if ((parsed->kind == LINE_ENTRY || parsed->kind == LINE_EXTERN) &&
        (parsed->operand_count != 1 || !is_valid_symbol_name(parsed->operands[0]))) {
        format_message(message, message_size, "directive requires one valid symbol");
        return 0;
    }
    if ((parsed->kind == LINE_DB || parsed->kind == LINE_DH || parsed->kind == LINE_DW) &&
        parsed->operand_count == 0) {
        format_message(message, message_size, "data directive requires values");
        return 0;
    }
    return 1;
}
