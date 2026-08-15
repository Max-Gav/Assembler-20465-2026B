#ifndef FILES_H
#define FILES_H

/* Paths, stale-file cleanup, and text-output helpers. */

#include "assembler_types.h"
#include <stdio.h>

/* Returns nonzero only for a nonempty path ending exactly in .as. */
int has_as_extension(const char *path);

/* Allocates a sibling path by replacing .as with suffix; caller frees it. */
char *replace_extension(const char *source_path, const char *suffix);

/* Removes stale .am/.ob/.ent/.ext outputs belonging to source_path. */
void remove_stale_outputs(const char *source_path, int remove_am);

/* Writes the object, entry, and external files. */
int write_output_files(const AssemblyContext *context);

#endif
