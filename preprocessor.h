#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

/* Macro preprocessing interface for one source and its derived .am path. */

/*
 * Expands mcro/mcroend definitions from source_path into am_path.
 * Diagnostics include source filename and line.  Returns 1 only when the
 * complete macro stage succeeds; a failed stage leaves no .am file.
 */
int preprocess_source(const char *source_path, const char *am_path);

#endif
