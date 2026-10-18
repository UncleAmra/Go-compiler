#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "minigo/lexer.h"

static void test_keywords(void) {
    const char *src = "package main\nfunc foo() {\nreturn 42\n}\n";
    Lexer l;
    lexer_init(&l, "test_keywords.go", src, strlen(src));

    Token t = lexer_next_token(&l);
    assert(t.type == TOKEN_PACKAGE);

    t = lexer_next_token(&l);
    assert(t.type == TOKEN_IDENT);

    t = lexer_next_token(&l);
    assert(t.type == TOKEN_SEMICOLON); /* ASI after 'main' before '\n' */
    assert(t.is_auto_semicolon);

    t = lexer_next_token(&l);
    assert(t.type == TOKEN_FUNC);

    t = lexer_next_token(&l);
    assert(t.type == TOKEN_IDENT);

    t = lexer_next_token(&l);
    assert(t.type == TOKEN_LPAREN);

    t = lexer_next_token(&l);
    assert(t.type == TOKEN_RPAREN);

    t = lexer_next_token(&l);
    assert(t.type == TOKEN_LBRACE);

    t = lexer_next_token(&l);
    assert(t.type == TOKEN_RETURN);

    t = lexer_next_token(&l);
    assert(t.type == TOKEN_INT_LIT);
    assert(t.int_val == 42);

    t = lexer_next_token(&l);
    assert(t.type == TOKEN_SEMICOLON); /* ASI after '42' before '\n' */
    assert(t.is_auto_semicolon);

    t = lexer_next_token(&l);
    assert(t.type == TOKEN_RBRACE);

    t = lexer_next_token(&l);
    assert(t.type == TOKEN_SEMICOLON); /* ASI after '}' before '\n' */
    assert(t.is_auto_semicolon);

    t = lexer_next_token(&l);
    assert(t.type == TOKEN_EOF);

    printf("[PASS] test_keywords & automatic semicolon insertion\n");
}

static void test_operators(void) {
    const char *src = "x := a + b * (c - 10) == 0";
    Lexer l;
    lexer_init(&l, "test_ops.go", src, strlen(src));

    assert(lexer_next_token(&l).type == TOKEN_IDENT);
    assert(lexer_next_token(&l).type == TOKEN_COLON_ASSIGN);
    assert(lexer_next_token(&l).type == TOKEN_IDENT);
    assert(lexer_next_token(&l).type == TOKEN_PLUS);
    assert(lexer_next_token(&l).type == TOKEN_IDENT);
    assert(lexer_next_token(&l).type == TOKEN_STAR);
    assert(lexer_next_token(&l).type == TOKEN_LPAREN);
    assert(lexer_next_token(&l).type == TOKEN_IDENT);
    assert(lexer_next_token(&l).type == TOKEN_MINUS);
    Token num = lexer_next_token(&l);
    assert(num.type == TOKEN_INT_LIT && num.int_val == 10);
    assert(lexer_next_token(&l).type == TOKEN_RPAREN);
    assert(lexer_next_token(&l).type == TOKEN_EQ_EQ);
    assert(lexer_next_token(&l).type == TOKEN_INT_LIT);

    printf("[PASS] test_operators\n");
}

int main(void) {
    printf("Running Mini Go Lexer Unit Tests...\n");
    test_keywords();
    test_operators();
    printf("All lexer tests passed successfully!\n");
    return 0;
}
