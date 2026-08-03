# MAMAN 14 assembler - course 20465, semester 2026B

This is an ANSI C implementation of the official two-pass assembler for the
Open University Systems Programming Laboratory. It implements the 32-register,
byte-addressed R/I/J machine and writes explicit little-endian machine bytes.

## Build and cleanup

```sh
make
make clean
```

The default target builds an executable named `assembler` with:

```sh
gcc -ansi -Wall -pedantic
```

## Running the assembler

```sh
./assembler file1.as [file2.as ...]
```

Every argument must include `.as`. Files are processed independently and in
argument order. A missing or invalid source does not stop later inputs. The
process exits zero only when every requested source succeeds.

For `program.as`, preprocessing creates `program.am`. Successful assembly also
creates `program.ob`; `program.ent` and `program.ext` are created only when
entry records or external J uses exist. Failed assembly removes stale final
outputs.

## Source language

- Registers: `$0` through `$31`.
- R instructions: `add sub and or nor move mvhi mvlo`.
- I instructions: `addi subi andi ori nori bne beq blt bgt lb sb lw sw lh sh`.
- J instructions: `jmp la call hlt`.
- Directives: `.db .dh .dw .asciz .entry .extern`.
- Macro form: `mcro NAME`, body lines, and `mcroend`.
- Labels begin with a letter, contain only letters/digits, and have at most 31
  characters. Physical source lines have at most 80 characters.

Code begins at byte address 100. Every instruction occupies four bytes.
Branches encode `target address - current instruction address` in bytes. Code
is followed immediately by packed data in the object file. Instructions and
multibyte data are serialized little-endian without writing host integer
memory directly.

## Modules

- `main.c`: command-line validation, ordered iteration, and aggregate status.
- `assembler.c`: preprocessing, both passes, relocation, resolution, cleanup.
- `parser.c`: central instruction metadata and complete line grammar.
- `preprocessor.c`: macro collection, validation, expansion, and `.am` output.
- `encoding.c`: R/I/J masks, ranges, byte images, and little-endian encoding.
- `files.c`: path derivation, stale cleanup, and exact output formats.
- `hashTable.c`: dynamic, owned, per-source symbol collection.
- `assembler_types.h`: shared constants, enums, and documented records.

## Permanent examples

The flat `tests/` directory contains submission-oriented inputs and verified
outputs, not an automated framework:

- `official_ps.as` and its `.am/.ob/.ent/.ext` files reproduce the official PDF
  example exactly.
- `valid_all.as` and its `.am/.ob` files cover every instruction, boundary
  immediates and data values, forward/backward branches, macros, strings, and
  little-endian packing.
- `valid_symbols.as` and its `.am/.ob/.ent/.ext` files cover forward/local
  references, entries, repeated extern declarations, multiple external uses,
  data relocation, and case-sensitive symbols.
- `errors.as` contains independent syntax, range, label, symbol, directive,
  string, and external-branch errors. It intentionally has no final outputs.

## Manual verification and screenshots

Run these commands in a real Ubuntu terminal:

```sh
make clean
make

cp tests/official_ps.ob /tmp/official_ps.expected.ob
./assembler tests/official_ps.as
diff -u /tmp/official_ps.expected.ob tests/official_ps.ob
rm /tmp/official_ps.expected.ob

./assembler tests/valid_all.as tests/valid_symbols.as
echo $?
ls -l tests/valid_all.* tests/valid_symbols.*
cat tests/valid_all.ob
cat tests/valid_symbols.ob
cat tests/valid_symbols.ent
cat tests/valid_symbols.ext

./assembler tests/errors.as
echo $?
ls -l tests/errors.*
test ! -e tests/errors.ob
test ! -e tests/errors.ent
test ! -e tests/errors.ext
```

The first `official_ps.ob` should be compared with a separately saved copy if
reverification is needed; the committed file is the official golden content.
For the required submission evidence, manually capture: (1) clean warning-free
build, (2) both valid runs and filenames, (3) object/entry/external contents,
(4) all line-numbered `errors.as` diagnostics, and (5) proof that the invalid
source has no `.ob/.ent/.ext`. Do not fabricate or store screenshots unless
they are manually added for submission.

## Specification assumptions

- The PDF example `.dh -60431` is outside the stated signed 16-bit range. It is
  treated as invalid; `.dh` accepts exactly -32768 through 32767.
- The prose requires four decimal address digits in `.ent`, while one displayed
  example omits the leading zero for address 116. The formal four-digit rule is
  followed, producing `0116`.
- Extracted sample prose once shows `END hlt`, but its symbol table and machine
  image require END to be a code label. The consistent form `END: hlt` is used.
