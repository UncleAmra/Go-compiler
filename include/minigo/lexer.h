#ifndef MINIGO_LEXER_H
#define MINIGO_LEXER_H

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

#endif /* MINIGO_LEXER_H */
