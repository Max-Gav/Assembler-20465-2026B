/* Symbol storage, lookup, relocation, and cleanup. */
#include "hashTable.h"
#include <stdlib.h>
#include <string.h>

Symbol *symbol_find(Symbol *head, const char *name)
{
    while (head != NULL) {
        if (strcmp(head->name, name) == 0)
            return head;
        head = head->next;
    }
    return NULL;
}

Symbol *symbol_add(Symbol **head, const char *name, long value, int code,
                   int data, int external, int line)
{
    Symbol *symbol;
    Symbol *tail;
    char *copy;
    size_t length;
    if (symbol_find(*head, name) != NULL)
        return NULL;
    length = strlen(name);
    copy = (char *)malloc(length + 1);
    symbol = (Symbol *)malloc(sizeof(Symbol));
    if (copy == NULL || symbol == NULL) {
        free(copy);
        free(symbol);
        return NULL;
    }
    strcpy(copy, name);
    symbol->name = copy;
    symbol->value = value;
    symbol->is_code = code;
    symbol->is_data = data;
    symbol->is_external = external;
    symbol->is_entry = 0;
    symbol->definition_line = line;
    symbol->next = NULL;
    if (*head == NULL) {
        *head = symbol;
    } else {
        tail = *head;
        while (tail->next != NULL)
            tail = tail->next;
        tail->next = symbol;
    }
    return symbol;
}

void symbol_relocate_data(Symbol *head, long icf)
{
    while (head != NULL) {
        if (head->is_data)
            head->value += icf;
        head = head->next;
    }
}

void symbol_table_destroy(Symbol *head)
{
    Symbol *next;
    while (head != NULL) {
        next = head->next;
        free(head->name);
        free(head);
        head = next;
    }
}
