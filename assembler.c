/*
 * Two-pass assembly and per-source state management.
 * The first pass builds the images and symbol table; the second resolves
 * symbols and entry requests.
 */
#include "assembler.h"
#include "assembler_types.h"
#include "encoding.h"
#include "files.h"
#include "hashTable.h"
#include "parser.h"
#include "preprocessor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reports a diagnostic and increments only the current source's error count. */
static void report_error(AssemblyContext *context, int line, const char *message)
{
    if (line > 0)
        printf("%s:%d: error: %s\n", context->source_path, line, message);
    else
        printf("%s: error: %s\n", context->source_path, message);
    ++context->errors;
}

/* Appends a retained instruction or entry directive for pass two. */
static int retain_statement(AssemblyContext *context, const ParsedLine *parsed,
                            long address, int line)
{
    Statement *statement = (Statement *)malloc(sizeof(Statement));
    if (statement == NULL)
        return 0;
    statement->parsed = *parsed;
    statement->address = address;
    statement->source_line = line;
    statement->next = NULL;
    if (context->statement_tail != NULL)
        context->statement_tail->next = statement;
    else
        context->statements = statement;
    context->statement_tail = statement;
    return 1;
}

/* Adds a local label at the current code or data byte address. */
static void define_label(AssemblyContext *context, const ParsedLine *parsed,
                         int line)
{
    long value;
    int code;
    int data;
    if (parsed->label[0] == '\0')
        return;
    if (parsed->kind == LINE_ENTRY || parsed->kind == LINE_EXTERN)
        return;
    code = parsed->kind == LINE_INSTRUCTION;
    data = parsed->kind == LINE_DB || parsed->kind == LINE_DH ||
           parsed->kind == LINE_DW || parsed->kind == LINE_ASCIZ;
    value = code ? context->ic : context->dc;
    if (symbol_find(context->symbols, parsed->label) != NULL) {
        report_error(context, line, "duplicate or conflicting symbol definition");
        return;
    }
    if (symbol_add(&context->symbols, parsed->label, value, code, data, 0, line) == NULL)
        report_error(context, line, "cannot allocate symbol");
}

/* Validates instruction operand classes that do not require symbol lookup. */
static int validate_instruction(AssemblyContext *context, const ParsedLine *parsed,
                                int line)
{
    const InstructionDesc *desc = parsed->instruction;
    int reg;
    long immediate;
    int ok = 1;
    if (desc->form == FORM_R3) {
        if (!parse_register(parsed->operands[0], &reg) ||
            !parse_register(parsed->operands[1], &reg) ||
            !parse_register(parsed->operands[2], &reg))
            ok = 0;
    } else if (desc->form == FORM_R2) {
        if (!parse_register(parsed->operands[0], &reg) ||
            !parse_register(parsed->operands[1], &reg))
            ok = 0;
    } else if (desc->form == FORM_I_ARITH || desc->form == FORM_I_MEMORY) {
        if (!parse_register(parsed->operands[0], &reg) ||
            !parse_decimal(parsed->operands[1], IMMEDIATE_MIN, IMMEDIATE_MAX, &immediate) ||
            !parse_register(parsed->operands[2], &reg))
            ok = 0;
    } else if (desc->form == FORM_I_BRANCH) {
        if (!parse_register(parsed->operands[0], &reg) ||
            !parse_register(parsed->operands[1], &reg) ||
            !is_valid_symbol_name(parsed->operands[2]))
            ok = 0;
    } else if (desc->form == FORM_J) {
        if (strcmp(desc->name, "jmp") == 0 && parse_register(parsed->operands[0], &reg))
            ok = 1;
        else if (!is_valid_symbol_name(parsed->operands[0]))
            ok = 0;
    }
    if (!ok)
        report_error(context, line, "invalid operand type or value");
    return ok;
}

/* Encodes one numeric directive, using exact signed range and byte width. */
static void encode_data_values(AssemblyContext *context, const ParsedLine *parsed,
                               int line)
{
    long minimum = SIGNED_8_MIN;
    long maximum = SIGNED_8_MAX;
    long value;
    int width = 1;
    int i;
    int success;
    if (parsed->kind == LINE_DH) {
        minimum = SIGNED_16_MIN;
        maximum = SIGNED_16_MAX;
        width = 2;
    } else if (parsed->kind == LINE_DW) {
        minimum = SIGNED_32_MIN;
        maximum = SIGNED_32_MAX;
        width = 4;
    }
    for (i = 0; i < parsed->operand_count; ++i) {
        if (!parse_decimal(parsed->operands[i], minimum, maximum, &value)) {
            report_error(context, line, "data value is malformed or outside directive range");
            continue;
        }
        if (width == 1) success = image_append_byte(&context->data, (unsigned long)value);
        else if (width == 2) success = image_append_half(&context->data, (unsigned long)value);
        else success = image_append_word(&context->data, (unsigned long)value);
        if (!success) {
            report_error(context, line, "out of memory while encoding data");
            return;
        }
        context->dc += width;
    }
}

/* Handles .extern, allowing identical repeated declarations only. */
static void declare_external(AssemblyContext *context, const char *name, int line)
{
    Symbol *existing = symbol_find(context->symbols, name);
    if (existing != NULL) {
        if (!existing->is_external)
            report_error(context, line, "external declaration conflicts with local symbol");
        return;
    }
    if (symbol_add(&context->symbols, name, 0, 0, 0, 1, line) == NULL)
        report_error(context, line, "cannot allocate external symbol");
}

/* Reads the expanded file and constructs all first-pass state. */
static int first_pass(AssemblyContext *context)
{
    FILE *file = fopen(context->am_path, "r");
    char buffer[MAX_SOURCE_LINE_LENGTH + 3];
    char message[160];
    size_t length;
    int line = 0;
    int ch;
    int too_long;
    ParsedLine parsed;
    int i;
    if (file == NULL) {
        report_error(context, 0, "cannot open expanded source");
        return 0;
    }
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        ++line;
        length = strlen(buffer);
        too_long = 0;
        if (length > 0 && buffer[length - 1] == '\n') buffer[--length] = '\0';
        else if (!feof(file)) {
            too_long = 1;
            while ((ch = fgetc(file)) != '\n' && ch != EOF) ;
        }
        if (length > 0 && buffer[length - 1] == '\r') buffer[--length] = '\0';
        if (length > MAX_SOURCE_LINE_LENGTH) too_long = 1;
        if (too_long) {
            report_error(context, line, "expanded line exceeds 80 characters");
            continue;
        }
        if (!parse_source_line(buffer, &parsed, message, sizeof(message))) {
            report_error(context, line, message);
            continue;
        }
        if (parsed.kind == LINE_EMPTY)
            continue;
        define_label(context, &parsed, line);
        if (parsed.kind == LINE_INSTRUCTION) {
            validate_instruction(context, &parsed, line);
            if (!retain_statement(context, &parsed, context->ic, line) ||
                !image_append_word(&context->code, 0))
                report_error(context, line, "out of memory while storing instruction");
            context->ic += INSTRUCTION_SIZE_BYTES;
        } else if (parsed.kind == LINE_DB || parsed.kind == LINE_DH ||
                   parsed.kind == LINE_DW) {
            encode_data_values(context, &parsed, line);
        } else if (parsed.kind == LINE_ASCIZ) {
            for (i = 0; parsed.string_value[i] != '\0'; ++i) {
                if (!image_append_byte(&context->data,
                                       (unsigned long)(unsigned char)parsed.string_value[i])) {
                    report_error(context, line, "out of memory while storing string");
                    break;
                }
                ++context->dc;
            }
            if (image_append_byte(&context->data, 0)) ++context->dc;
            else report_error(context, line, "out of memory while terminating string");
        } else if (parsed.kind == LINE_EXTERN) {
            declare_external(context, parsed.operands[0], line);
        } else if (parsed.kind == LINE_ENTRY) {
            if (!retain_statement(context, &parsed, 0, line))
                report_error(context, line, "out of memory while storing entry request");
        }
    }
    if (ferror(file))
        report_error(context, 0, "error while reading expanded source");
    fclose(file);
    context->icf = context->ic;
    context->dcf = context->dc;
    symbol_relocate_data(context->symbols, context->icf);
    if (context->icf + context->dcf - 1 > MAX_MEMORY_ADDRESS)
        report_error(context, 0, "assembled image exceeds 25-bit memory");
    return context->errors == 0;
}

/* Records one external J use; ownership remains with the context. */
static int record_external_use(AssemblyContext *context, const char *name,
                               long address)
{
    ExternalUse *use = (ExternalUse *)malloc(sizeof(ExternalUse));
    if (use == NULL)
        return 0;
    use->name = (char *)malloc(strlen(name) + 1);
    if (use->name == NULL) {
        free(use);
        return 0;
    }
    strcpy(use->name, name);
    use->address = address;
    use->next = NULL;
    if (context->external_tail != NULL) context->external_tail->next = use;
    else context->external_uses = use;
    context->external_tail = use;
    return 1;
}

/* Resolves and constructs exactly one retained instruction. */
static int resolve_instruction(AssemblyContext *context, Statement *statement,
                               unsigned long *word)
{
    ParsedLine *parsed = &statement->parsed;
    const InstructionDesc *desc = parsed->instruction;
    int rs = 0;
    int rt = 0;
    int rd = 0;
    long immediate = 0;
    long address = 0;
    int register_bit = 0;
    Symbol *symbol;
    if (desc->form == FORM_R3) {
        if (!parse_register(parsed->operands[0], &rs) ||
            !parse_register(parsed->operands[1], &rt) ||
            !parse_register(parsed->operands[2], &rd)) return 0;
        *word = encode_r(desc->opcode, rs, rt, rd, desc->funct);
    } else if (desc->form == FORM_R2) {
        if (!parse_register(parsed->operands[0], &rs) ||
            !parse_register(parsed->operands[1], &rd)) return 0;
        *word = encode_r(desc->opcode, rs, 0, rd, desc->funct);
    } else if (desc->form == FORM_I_ARITH || desc->form == FORM_I_MEMORY) {
        if (!parse_register(parsed->operands[0], &rs) ||
            !parse_decimal(parsed->operands[1], IMMEDIATE_MIN, IMMEDIATE_MAX, &immediate) ||
            !parse_register(parsed->operands[2], &rt)) return 0;
        *word = encode_i(desc->opcode, rs, rt, immediate);
    } else if (desc->form == FORM_I_BRANCH) {
        if (!parse_register(parsed->operands[0], &rs) ||
            !parse_register(parsed->operands[1], &rt)) return 0;
        symbol = symbol_find(context->symbols, parsed->operands[2]);
        if (symbol == NULL) {
            report_error(context, statement->source_line, "undefined branch target");
            return 0;
        }
        if (symbol->is_external) {
            report_error(context, statement->source_line, "branch target cannot be external");
            return 0;
        }
        immediate = symbol->value - statement->address;
        if (!encoding_signed16_fits(immediate)) {
            report_error(context, statement->source_line, "branch displacement is outside signed 16-bit range");
            return 0;
        }
        *word = encode_i(desc->opcode, rs, rt, immediate);
    } else if (desc->form == FORM_J) {
        if (strcmp(desc->name, "jmp") == 0 && parse_register(parsed->operands[0], &rd)) {
            register_bit = 1;
            address = rd;
        } else {
            symbol = symbol_find(context->symbols, parsed->operands[0]);
            if (symbol == NULL) {
                report_error(context, statement->source_line, "undefined J-instruction symbol");
                return 0;
            }
            address = symbol->is_external ? 0 : symbol->value;
            if (symbol->is_external &&
                !record_external_use(context, symbol->name, statement->address)) {
                report_error(context, statement->source_line, "cannot record external use");
                return 0;
            }
        }
        if (!encoding_address25_fits(address)) {
            report_error(context, statement->source_line, "J address exceeds 25 bits");
            return 0;
        }
        *word = encode_j(desc->opcode, register_bit, address);
    } else {
        *word = encode_j(desc->opcode, 0, 0);
    }
    return 1;
}

/* Completes unresolved code and validates all entry requests. */
static int second_pass(AssemblyContext *context)
{
    Statement *statement;
    Symbol *symbol;
    unsigned long word;
    for (statement = context->statements; statement != NULL; statement = statement->next) {
        if (statement->parsed.kind == LINE_ENTRY) {
            symbol = symbol_find(context->symbols, statement->parsed.operands[0]);
            if (symbol == NULL) {
                report_error(context, statement->source_line, "entry symbol is undefined");
            } else if (symbol->is_external) {
                report_error(context, statement->source_line, "symbol cannot be both entry and external");
            } else {
                symbol->is_entry = 1;
            }
        } else if (resolve_instruction(context, statement, &word)) {
            if (!image_store_word(&context->code,
                                  statement->address - CODE_START_ADDRESS, word))
                report_error(context, statement->source_line, "internal code-image error");
        }
    }
    return context->errors == 0;
}

/* Releases every object owned by a per-file context. */
static void destroy_context(AssemblyContext *context)
{
    Statement *statement;
    Statement *next_statement;
    ExternalUse *use;
    ExternalUse *next_use;
    symbol_table_destroy(context->symbols);
    for (statement = context->statements; statement != NULL; statement = next_statement) {
        next_statement = statement->next;
        free(statement);
    }
    for (use = context->external_uses; use != NULL; use = next_use) {
        next_use = use->next;
        free(use->name);
        free(use);
    }
    image_destroy(&context->code);
    image_destroy(&context->data);
}

int assemble_source(const char *source_path)
{
    AssemblyContext context;
    char *am_path;
    int success = 0;
    memset(&context, 0, sizeof(context));
    context.source_path = source_path;
    context.ic = CODE_START_ADDRESS;
    remove_stale_outputs(source_path, 1);
    am_path = replace_extension(source_path, ".am");
    if (am_path == NULL) {
        printf("%s: error: cannot derive output paths\n", source_path);
        return 0;
    }
    context.am_path = am_path;
    if (preprocess_source(source_path, am_path)) {
        if (first_pass(&context)) {
            if (second_pass(&context)) {
                if (write_output_files(&context))
                    success = 1;
                else
                    report_error(&context, 0, "cannot write output files");
            }
        }
    }
    if (!success)
        remove_stale_outputs(source_path, 0);
    destroy_context(&context);
    free(am_path);
    return success;
}
