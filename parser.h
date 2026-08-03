#ifndef PARSER_H
#define PARSER_H

/* Line grammar, instruction metadata, and token validation interface. */

#include "assembler_types.h"

/* Returns the descriptor for an exact lowercase instruction name. */
const InstructionDesc *find_instruction(const char *name);

/* Returns nonzero when name is an instruction, directive, or macro keyword. */
int is_reserved_name(const char *name);

/* Validates the lexical form of a label or macro name. */
int is_valid_symbol_name(const char *name);

/* Parses one logical line.  On failure, message receives a diagnostic. */
int parse_source_line(const char *text, ParsedLine *parsed,
                      char *message, int message_size);

/* Parses an exact decimal integer within inclusive limits. */
int parse_decimal(const char *text, long minimum, long maximum, long *value);

/* Parses an exact register token in the range $0 through $31. */
int parse_register(const char *text, int *reg);

#endif
