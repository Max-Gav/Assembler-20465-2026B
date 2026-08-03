#ifndef ASSEMBLER_H
#define ASSEMBLER_H

/* Public orchestration interface for assembling one independent source. */

/*
 * Runs the complete independent assembly pipeline for one .as path:
 * stale cleanup, macro expansion, two passes, relocation, and output.
 * Returns 1 on success and 0 after reporting any error.
 */
int assemble_source(const char *source_path);

#endif
