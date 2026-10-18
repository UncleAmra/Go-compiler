#include "minigo/common.h"

int g_error_count = 0;

void arena_init(Arena *arena, size_t chunk_size) {
    if (chunk_size < 4096) {
        chunk_size = 64 * 1024; /* 64 KB default */
    }
    arena->chunk_size = chunk_size;
    arena->first = NULL;
    arena->current = NULL;
}

static ArenaChunk *arena_alloc_chunk(size_t capacity) {
    ArenaChunk *chunk = (ArenaChunk *)malloc(sizeof(ArenaChunk) + capacity);
    if (!chunk) {
        fprintf(stderr, "Fatal error: Out of memory in Arena allocator!\n");
        exit(1);
    }
    chunk->next = NULL;
    chunk->capacity = capacity;
    chunk->used = 0;
    return chunk;
}

void *arena_alloc(Arena *arena, size_t size) {
    /* 8-byte alignment */
    size = (size + 7) & ~((size_t)7);

    if (!arena->current || arena->current->used + size > arena->current->capacity) {
        size_t next_cap = arena->chunk_size;
        if (size > next_cap) {
            next_cap = size;
        }
        ArenaChunk *new_chunk = arena_alloc_chunk(next_cap);
        if (!arena->first) {
            arena->first = new_chunk;
        } else {
            arena->current->next = new_chunk;
        }
        arena->current = new_chunk;
    }

    void *ptr = arena->current->data + arena->current->used;
    arena->current->used += size;
    return ptr;
}

char *arena_strndup(Arena *arena, const char *str, size_t len) {
    char *copy = (char *)arena_alloc(arena, len + 1);
    memcpy(copy, str, len);
    copy[len] = '\0';
    return copy;
}

char *arena_strdup(Arena *arena, const char *str) {
    return arena_strndup(arena, str, strlen(str));
}

void arena_free_all(Arena *arena) {
    ArenaChunk *curr = arena->first;
    while (curr) {
        ArenaChunk *next = curr->next;
        free(curr);
        curr = next;
    }
    arena->first = NULL;
    arena->current = NULL;
}

char *read_file_to_string(const char *path, size_t *out_length) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 0) {
        fclose(f);
        return NULL;
    }

    char *buf = (char *)malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buf, 1, sz, f);
    buf[read_bytes] = '\0';
    fclose(f);

    if (out_length) {
        *out_length = read_bytes;
    }
    return buf;
}

void report_error(SourcePos pos, const char *fmt, ...) {
    g_error_count++;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "%s:%d:%d: \033[1;31merror:\033[0m ", pos.filename ? pos.filename : "<input>", pos.line, pos.col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void report_warning(SourcePos pos, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "%s:%d:%d: \033[1;33mwarning:\033[0m ", pos.filename ? pos.filename : "<input>", pos.line, pos.col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}
