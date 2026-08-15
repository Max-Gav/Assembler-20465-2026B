#ifndef ASSEMBLER_TYPES_H
#define ASSEMBLER_TYPES_H

/*
 * Shared assembler model for the 2026B machine.  Addresses and image sizes
 * are byte counts.  unsigned long is used as a container; only its low
 * 32 bits are emitted explicitly, so host word size and byte order do not
 * affect the object file.
 */

#define CODE_START_ADDRESS 100L
#define INSTRUCTION_SIZE_BYTES 4
#define REGISTER_COUNT 32
#define REGISTER_MIN 0
#define REGISTER_MAX 31
#define MAX_SOURCE_LINE_LENGTH 80
#define MAX_LABEL_LENGTH 31
#define MAX_OPERANDS 64
#define MAX_TOKEN_LENGTH 82
#define IMMEDIATE_MIN (-32768L)
#define IMMEDIATE_MAX 32767L
#define MEMORY_ADDRESS_BITS 25
#define MAX_MEMORY_ADDRESS 33554431L
#define SIGNED_8_MIN (-128L)
#define SIGNED_8_MAX 127L
#define SIGNED_16_MIN (-32768L)
#define SIGNED_16_MAX 32767L
#define SIGNED_32_MIN (-2147483647L - 1L)
#define SIGNED_32_MAX 2147483647L

/* Parser result categories; directives retain their exact element width. */
typedef enum {
    LINE_EMPTY, LINE_INSTRUCTION, LINE_DB, LINE_DH, LINE_DW,
    LINE_ASCIZ, LINE_ENTRY, LINE_EXTERN, LINE_INVALID
} LineKind;

/* Operand layouts used to validate and encode the central instruction table. */
typedef enum {
    FORM_R3, FORM_R2, FORM_I_ARITH, FORM_I_BRANCH, FORM_I_MEMORY,
    FORM_J, FORM_HLT
} InstructionForm;

/* Immutable description of one legal machine instruction. */
typedef struct InstructionDesc {
    const char *name;
    InstructionForm form;
    int opcode;
    int funct;
    int operand_count;
} InstructionDesc;

/* Fully tokenized source line.  Numeric conversion is deliberately delayed. */
typedef struct ParsedLine {
    LineKind kind;                 /* Syntactic category selected by parsing. */
    char label[MAX_LABEL_LENGTH + 1]; /* Optional definition without colon. */
    const InstructionDesc *instruction; /* Non-NULL for LINE_INSTRUCTION. */
    int operand_count;             /* Number of comma-separated operands. */
    char operands[MAX_OPERANDS][MAX_TOKEN_LENGTH]; /* Unresolved tokens. */
    char string_value[MAX_SOURCE_LINE_LENGTH + 1]; /* Unquoted .asciz bytes. */
} ParsedLine;

/* A symbol may be code/data, external, or marked as an entry. */
typedef struct Symbol {
    char *name;
    long value;
    int is_code;
    int is_data;
    int is_external;
    int is_entry;
    int definition_line;
    struct Symbol *next;
} Symbol;

/* One parsed instruction retained between the two passes. */
typedef struct Statement {
    ParsedLine parsed;
    long address;
    int source_line;
    struct Statement *next;
} Statement;

/* Each external J reference produces one line in the .ext file. */
typedef struct ExternalUse {
    char *name;
    long address;
    struct ExternalUse *next;
} ExternalUse;

/* Growable byte image used for code and data. */
typedef struct ByteImage {
    unsigned char *bytes; /* Owned byte storage. */
    long size;            /* Number of meaningful code or data bytes. */
    long capacity;        /* Allocated byte capacity. */
} ByteImage;

/* All mutable state is private to one command-line source file. */
typedef struct AssemblyContext {
    const char *source_path;
    const char *am_path;
    Symbol *symbols;
    Statement *statements;
    Statement *statement_tail;
    ExternalUse *external_uses; /* Ordered list written to the .ext file. */
    ExternalUse *external_tail;
    ByteImage code;       /* Instruction image, in bytes. */
    ByteImage data;       /* Packed directive image, in bytes. */
    long ic;              /* Current instruction byte address; begins at 100. */
    long dc;              /* Current data-image byte count. */
    long icf;             /* Final address immediately after code. */
    long dcf;             /* Final data-image byte count. */
    int errors;           /* Diagnostics reported for this source only. */
} AssemblyContext;

#endif
