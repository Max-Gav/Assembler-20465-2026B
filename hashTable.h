#ifndef HASH_TABLE_H
#define HASH_TABLE_H

/* Dynamic symbol-table ownership and lookup interface. */

#include "assembler_types.h"

/* Finds a case-sensitive symbol, or returns NULL. */
Symbol *symbol_find(Symbol *head, const char *name);

/*
 * Adds a unique symbol.  The table owns a copy of name.  Returns the symbol,
 * or NULL for a duplicate/allocation failure; callers distinguish duplicates
 * by searching first.
 */
Symbol *symbol_add(Symbol **head, const char *name, long value, int code,
                   int data, int external, int line);

/* Adds ICF to every data symbol after the first pass. */
void symbol_relocate_data(Symbol *head, long icf);

/* Releases the complete symbol list. */
void symbol_table_destroy(Symbol *head);

#endif
