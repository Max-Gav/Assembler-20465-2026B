#ifndef ENCODING_H
#define ENCODING_H

/* Host-independent 2026B bit-field and byte-image encoding interface. */

#include "assembler_types.h"

/* Appends one byte, growing the image.  Returns 1 on success. */
int image_append_byte(ByteImage *image, unsigned long value);

/* Appends the low 16 or 32 bits in little-endian byte order. */
int image_append_half(ByteImage *image, unsigned long value);
int image_append_word(ByteImage *image, unsigned long value);

/* Stores a 32-bit instruction at an existing byte offset. */
int image_store_word(ByteImage *image, long offset, unsigned long value);

/* Releases the image and resets all its fields. */
void image_destroy(ByteImage *image);

/* Constructs validated R, I, and J instruction words. */
unsigned long encode_r(int opcode, int rs, int rt, int rd, int funct);
unsigned long encode_i(int opcode, int rs, int rt, long immediate);
unsigned long encode_j(int opcode, int register_bit, long address);

/* Range predicates used before narrowing branch and J fields. */
int encoding_signed16_fits(long value);
int encoding_address25_fits(long value);

#endif
