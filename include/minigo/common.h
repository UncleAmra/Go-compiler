#ifndef MINIGO_COMMON_H
#define MINIGO_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>

/* Source code position */
typedef struct {
    const char *filename;
    int line;
    int col;
} SourcePos;

/* Dynamic Memory Arena Allocator:
 * Compilers allocate many small objects (AST nodes, tokens, types).
 * Allocating from an arena gives fast O(1) allocation and frees
 * all memory in a single step at the end of compilation.
 */
typedef struct ArenaChunk {
    struct ArenaChunk *next;
    size_t capacity;
    size_t used;
#if defined(__cplusplus)
    char data[1];
#else
    char data[];
#endif
} ArenaChunk;

typedef struct {
    ArenaChunk *first;
    ArenaChunk *current;
    size_t chunk_size;
} Arena;

void arena_init(Arena *arena, size_t chunk_size);
void *arena_alloc(Arena *arena, size_t size);
char *arena_strndup(Arena *arena, const char *str, size_t len);
char *arena_strdup(Arena *arena, const char *str);
void arena_free_all(Arena *arena);

/* File reader helper */
char *read_file_to_string(const char *path, size_t *out_length);

/* Compiler error reporting */
void report_error(SourcePos pos, const char *fmt, ...);
void report_warning(SourcePos pos, const char *fmt, ...);
extern int g_error_count;

#ifdef __cplusplus
}
#endif

#endif /* MINIGO_COMMON_H */
