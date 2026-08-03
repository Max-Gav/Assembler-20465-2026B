/*
 * Module: encoding.c
 * Constructs 32-bit machine words and serializes all numeric values in
 * little-endian order. No host integer is written as raw memory and all shifts
 * operate on unsigned values. It assumes validated fields from assembler.c
 * and uses the byte-image types defined in assembler_types.h.
 */
#include "encoding.h"
#include <stdlib.h>

/* Ensures room for the requested number of bytes. */
static int image_reserve(ByteImage *image, long needed)
{
    unsigned char *grown;
    long capacity;
    if (needed <= image->capacity)
        return 1;
    capacity = image->capacity ? image->capacity : 64;
    while (capacity < needed)
        capacity *= 2;
    grown = (unsigned char *)realloc(image->bytes, (size_t)capacity);
    if (grown == NULL)
        return 0;
    image->bytes = grown;
    image->capacity = capacity;
    return 1;
}

int image_append_byte(ByteImage *image, unsigned long value)
{
    if (!image_reserve(image, image->size + 1))
        return 0;
    image->bytes[image->size++] = (unsigned char)(value & 0xFFUL);
    return 1;
}

int image_append_half(ByteImage *image, unsigned long value)
{
    return image_append_byte(image, value) &&
           image_append_byte(image, value >> 8);
}

int image_append_word(ByteImage *image, unsigned long value)
{
    return image_append_byte(image, value) &&
           image_append_byte(image, value >> 8) &&
           image_append_byte(image, value >> 16) &&
           image_append_byte(image, value >> 24);
}

int image_store_word(ByteImage *image, long offset, unsigned long value)
{
    if (offset < 0 || offset + 4 > image->size)
        return 0;
    image->bytes[offset] = (unsigned char)(value & 0xFFUL);
    image->bytes[offset + 1] = (unsigned char)((value >> 8) & 0xFFUL);
    image->bytes[offset + 2] = (unsigned char)((value >> 16) & 0xFFUL);
    image->bytes[offset + 3] = (unsigned char)((value >> 24) & 0xFFUL);
    return 1;
}

void image_destroy(ByteImage *image)
{
    free(image->bytes);
    image->bytes = NULL;
    image->size = 0;
    image->capacity = 0;
}

unsigned long encode_r(int opcode, int rs, int rt, int rd, int funct)
{
    return (((unsigned long)opcode & 0x3FUL) << 26) |
           (((unsigned long)rs & 0x1FUL) << 21) |
           (((unsigned long)rt & 0x1FUL) << 16) |
           (((unsigned long)rd & 0x1FUL) << 11) |
           (((unsigned long)funct & 0x1FUL) << 6);
}

unsigned long encode_i(int opcode, int rs, int rt, long immediate)
{
    return (((unsigned long)opcode & 0x3FUL) << 26) |
           (((unsigned long)rs & 0x1FUL) << 21) |
           (((unsigned long)rt & 0x1FUL) << 16) |
           ((unsigned long)immediate & 0xFFFFUL);
}

unsigned long encode_j(int opcode, int register_bit, long address)
{
    return (((unsigned long)opcode & 0x3FUL) << 26) |
           (((unsigned long)register_bit & 1UL) << 25) |
           ((unsigned long)address & 0x1FFFFFFUL);
}

int encoding_signed16_fits(long value)
{
    return value >= IMMEDIATE_MIN && value <= IMMEDIATE_MAX;
}

int encoding_address25_fits(long value)
{
    return value >= 0 && value <= MAX_MEMORY_ADDRESS;
}
