#ifndef MINIGO_PARSER_H
#define MINIGO_PARSER_H

#include "minigo/lexer.h"
#include "minigo/ast.h"

typedef struct {
    Lexer lexer;
    Token current;
    Token peek;
    Arena *arena;
    bool had_error;
    bool panic_mode;
} Parser;

void parser_init(Parser *p, const char *filename, const char *source, size_t length, Arena *arena);
ASTProgram *parser_parse_program(Parser *p);

#endif /* MINIGO_PARSER_H */
