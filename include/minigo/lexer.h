#ifndef MINIGO_LEXER_H
#define MINIGO_LEXER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "minigo/token.h"

typedef struct {
    const char *filename;
    const char *source;
    size_t length;
    size_t cursor;
    int line;
    int col;
    TokenType last_emitted_type;
    bool pending_auto_semicolon;
} Lexer;

void lexer_init(Lexer *l, const char *filename, const char *source, size_t length);
Token lexer_next_token(Lexer *l);
Token lexer_peek_token(Lexer *l);

#ifdef __cplusplus
}
#endif

#endif /* MINIGO_LEXER_H */
