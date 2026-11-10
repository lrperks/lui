/* luic — the .lui compiler (see README.md). */
#ifndef LUIC_H
#define LUIC_H

#include <stddef.h>

/* Compiles .lui source into a Lua chunk.  Returns a malloc'd, NUL-terminated
 * string (its length in *out_len if not NULL), or NULL on a syntax error with
 * *error pointing at a message like "line 4: unterminated <button>". */
char *luic_compile(const char *source, size_t len, size_t *out_len, const char **error);

#endif
