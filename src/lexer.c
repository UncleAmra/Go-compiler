#include "minigo/lexer.h"
#include <ctype.h>

void lexer_init(Lexer *l, const char *filename, const char *source, size_t length) {
    l->filename = filename;
    l->source = source;
    l->length = length;
    l->cursor = 0;
    l->line = 1;
    l->col = 1;
    l->last_emitted_type = TOKEN_ILLEGAL;
    l->pending_auto_semicolon = false;
}

static char peek_char(const Lexer *l) {
    if (l->cursor >= l->length) return '\0';
    return l->source[l->cursor];
}

static char peek_next_char(const Lexer *l) {
    if (l->cursor + 1 >= l->length) return '\0';
    return l->source[l->cursor + 1];
}

static char advance_char(Lexer *l) {
    if (l->cursor >= l->length) return '\0';
    char c = l->source[l->cursor++];
    if (c == '\n') {
        l->line++;
        l->col = 1;
    } else {
        l->col++;
    }
    return c;
}

static bool can_auto_insert_semicolon(TokenType type) {
    switch (type) {
        case TOKEN_IDENT:
        case TOKEN_INT_LIT:
        case TOKEN_FLOAT_LIT:
        case TOKEN_STRING_LIT:
        case TOKEN_RETURN:
        case TOKEN_BREAK:
        case TOKEN_CONTINUE:
        case TOKEN_INC:
        case TOKEN_DEC:
        case TOKEN_RPAREN:
        case TOKEN_RBRACKET:
        case TOKEN_RBRACE:
            return true;
        default:
            return false;
    }
}

static TokenType lookup_keyword(const char *str, size_t len) {
    #define MATCH_KW(kw, tok) \
        if (len == sizeof(kw) - 1 && memcmp(str, kw, len) == 0) return tok;

    MATCH_KW("package",  TOKEN_PACKAGE);
    MATCH_KW("import",   TOKEN_IMPORT);
    MATCH_KW("func",     TOKEN_FUNC);
    MATCH_KW("var",      TOKEN_VAR);
    MATCH_KW("const",    TOKEN_CONST);
    MATCH_KW("type",     TOKEN_TYPE);
    MATCH_KW("struct",   TOKEN_STRUCT);
    MATCH_KW("return",   TOKEN_RETURN);
    MATCH_KW("if",       TOKEN_IF);
    MATCH_KW("else",     TOKEN_ELSE);
    MATCH_KW("for",      TOKEN_FOR);
    MATCH_KW("break",    TOKEN_BREAK);
    MATCH_KW("continue", TOKEN_CONTINUE);

    #undef MATCH_KW
    return TOKEN_IDENT;
}

Token lexer_next_token(Lexer *l) {
    while (l->cursor < l->length) {
        char c = peek_char(l);

        /* Handle spaces and tabs */
        if (c == ' ' || c == '\t' || c == '\r') {
            advance_char(l);
            continue;
        }

        /* Check for Go Automatic Semicolon Insertion upon newline */
        if (c == '\n') {
            SourcePos pos = { l->filename, l->line, l->col };
            advance_char(l);
            if (can_auto_insert_semicolon(l->last_emitted_type)) {
                Token tok;
                tok.type = TOKEN_SEMICOLON;
                tok.start = ";";
                tok.length = 1;
                tok.pos = pos;
                tok.is_auto_semicolon = true;
                l->last_emitted_type = TOKEN_SEMICOLON;
                return tok;
            }
            continue;
        }

        /* Comments */
        if (c == '/' && peek_next_char(l) == '/') {
            /* Single-line comment: keep newline for ASI check */
            while (l->cursor < l->length && peek_char(l) != '\n') {
                advance_char(l);
            }
            continue;
        }

        if (c == '/' && peek_next_char(l) == '*') {
            advance_char(l); // '/'
            advance_char(l); // '*'
            bool has_newline = false;
            while (l->cursor < l->length) {
                if (peek_char(l) == '\n') has_newline = true;
                if (peek_char(l) == '*' && peek_next_char(l) == '/') {
                    advance_char(l); // '*'
                    advance_char(l); // '/'
                    break;
                }
                advance_char(l);
            }
            if (has_newline && can_auto_insert_semicolon(l->last_emitted_type)) {
                Token tok;
                tok.type = TOKEN_SEMICOLON;
                tok.start = ";";
                tok.length = 1;
                tok.pos = (SourcePos){ l->filename, l->line, l->col };
                tok.is_auto_semicolon = true;
                l->last_emitted_type = TOKEN_SEMICOLON;
                return tok;
            }
            continue;
        }

        break;
    }

    SourcePos pos = { l->filename, l->line, l->col };

    /* End of file handling */
    if (l->cursor >= l->length) {
        if (can_auto_insert_semicolon(l->last_emitted_type)) {
            Token tok;
            tok.type = TOKEN_SEMICOLON;
            tok.start = ";";
            tok.length = 1;
            tok.pos = pos;
            tok.is_auto_semicolon = true;
            l->last_emitted_type = TOKEN_SEMICOLON;
            return tok;
        }
        Token tok;
        tok.type = TOKEN_EOF;
        tok.start = "";
        tok.length = 0;
        tok.pos = pos;
        tok.is_auto_semicolon = false;
        l->last_emitted_type = TOKEN_EOF;
        return tok;
    }

    const char *start_ptr = &l->source[l->cursor];
    char c = advance_char(l);

    /* Identifiers and keywords */
    if (isalpha((unsigned char)c) || c == '_') {
        while (isalnum((unsigned char)peek_char(l)) || peek_char(l) == '_') {
            advance_char(l);
        }
        size_t len = (size_t)(&l->source[l->cursor] - start_ptr);
        TokenType type = lookup_keyword(start_ptr, len);

        Token tok;
        tok.type = type;
        tok.start = start_ptr;
        tok.length = len;
        tok.pos = pos;
        tok.is_auto_semicolon = false;
        l->last_emitted_type = type;
        return tok;
    }

    /* Numeric literals */
    if (isdigit((unsigned char)c)) {
        int64_t val = c - '0';
        if (c == '0' && (peek_char(l) == 'x' || peek_char(l) == 'X')) {
            advance_char(l); /* 'x' */
            val = 0;
            while (isxdigit((unsigned char)peek_char(l))) {
                char h = advance_char(l);
                int d = isdigit((unsigned char)h) ? (h - '0') : (tolower((unsigned char)h) - 'a' + 10);
                val = (val << 4) | d;
            }
        } else {
            while (isdigit((unsigned char)peek_char(l))) {
                val = val * 10 + (advance_char(l) - '0');
            }
        }
        size_t len = (size_t)(&l->source[l->cursor] - start_ptr);
        Token tok;
        tok.type = TOKEN_INT_LIT;
        tok.start = start_ptr;
        tok.length = len;
        tok.pos = pos;
        tok.int_val = val;
        tok.is_auto_semicolon = false;
        l->last_emitted_type = TOKEN_INT_LIT;
        return tok;
    }

    /* String literals */
    if (c == '"') {
        while (l->cursor < l->length && peek_char(l) != '"' && peek_char(l) != '\n') {
            if (peek_char(l) == '\\') {
                advance_char(l); /* escape */
            }
            advance_char(l);
        }
        if (peek_char(l) == '"') {
            advance_char(l); /* closing quote */
        } else {
            report_error(pos, "Unterminated string literal");
        }
        size_t len = (size_t)(&l->source[l->cursor] - start_ptr);
        Token tok;
        tok.type = TOKEN_STRING_LIT;
        tok.start = start_ptr;
        tok.length = len;
        tok.pos = pos;
        tok.is_auto_semicolon = false;
        l->last_emitted_type = TOKEN_STRING_LIT;
        return tok;
    }

    /* Operators & punctuation */
    TokenType type = TOKEN_ILLEGAL;
    switch (c) {
        case '+':
            if (peek_char(l) == '+') { advance_char(l); type = TOKEN_INC; }
            else if (peek_char(l) == '=') { advance_char(l); type = TOKEN_PLUS_ASSIGN; }
            else { type = TOKEN_PLUS; }
            break;
        case '-':
            if (peek_char(l) == '-') { advance_char(l); type = TOKEN_DEC; }
            else if (peek_char(l) == '=') { advance_char(l); type = TOKEN_MINUS_ASSIGN; }
            else { type = TOKEN_MINUS; }
            break;
        case '*': type = TOKEN_STAR; break;
        case '/': type = TOKEN_SLASH; break;
        case '%': type = TOKEN_PERCENT; break;
        case '&':
            if (peek_char(l) == '&') { advance_char(l); type = TOKEN_AND_AND; }
            else { type = TOKEN_AMP; }
            break;
        case '|':
            if (peek_char(l) == '|') { advance_char(l); type = TOKEN_OR_OR; }
            else { type = TOKEN_PIPE; }
            break;
        case '^': type = TOKEN_CARET; break;
        case ':':
            if (peek_char(l) == '=') { advance_char(l); type = TOKEN_COLON_ASSIGN; }
            else { type = TOKEN_COLON; }
            break;
        case '=':
            if (peek_char(l) == '=') { advance_char(l); type = TOKEN_EQ_EQ; }
            else { type = TOKEN_ASSIGN; }
            break;
        case '!':
            if (peek_char(l) == '=') { advance_char(l); type = TOKEN_NOT_EQ; }
            else { type = TOKEN_EXCLAMATION; }
            break;
        case '<':
            if (peek_char(l) == '=') { advance_char(l); type = TOKEN_LE; }
            else if (peek_char(l) == '<') { advance_char(l); type = TOKEN_SHL; }
            else { type = TOKEN_LT; }
            break;
        case '>':
            if (peek_char(l) == '=') { advance_char(l); type = TOKEN_GE; }
            else if (peek_char(l) == '>') { advance_char(l); type = TOKEN_SHR; }
            else { type = TOKEN_GT; }
            break;
        case '(': type = TOKEN_LPAREN; break;
        case ')': type = TOKEN_RPAREN; break;
        case '{': type = TOKEN_LBRACE; break;
        case '}': type = TOKEN_RBRACE; break;
        case '[': type = TOKEN_LBRACKET; break;
        case ']': type = TOKEN_RBRACKET; break;
        case ',': type = TOKEN_COMMA; break;
        case '.': type = TOKEN_DOT; break;
        case ';': type = TOKEN_SEMICOLON; break;
        default:
            report_error(pos, "Unexpected character: '%c' (0x%02X)", c, (unsigned char)c);
            type = TOKEN_ILLEGAL;
            break;
    }

    size_t len = (size_t)(&l->source[l->cursor] - start_ptr);
    Token tok;
    tok.type = type;
    tok.start = start_ptr;
    tok.length = len;
    tok.pos = pos;
    tok.is_auto_semicolon = false;
    l->last_emitted_type = type;
    return tok;
}
