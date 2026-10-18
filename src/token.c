#include "minigo/token.h"

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF:           return "EOF";
        case TOKEN_ILLEGAL:       return "ILLEGAL";
        case TOKEN_IDENT:         return "IDENT";
        case TOKEN_INT_LIT:       return "INT_LIT";
        case TOKEN_FLOAT_LIT:     return "FLOAT_LIT";
        case TOKEN_STRING_LIT:    return "STRING_LIT";

        case TOKEN_PACKAGE:       return "package";
        case TOKEN_IMPORT:        return "import";
        case TOKEN_FUNC:          return "func";
        case TOKEN_VAR:           return "var";
        case TOKEN_CONST:         return "const";
        case TOKEN_TYPE:          return "type";
        case TOKEN_STRUCT:        return "struct";
        case TOKEN_RETURN:        return "return";
        case TOKEN_IF:            return "if";
        case TOKEN_ELSE:          return "else";
        case TOKEN_FOR:           return "for";
        case TOKEN_BREAK:         return "break";
        case TOKEN_CONTINUE:      return "continue";

        case TOKEN_PLUS:          return "+";
        case TOKEN_MINUS:         return "-";
        case TOKEN_STAR:          return "*";
        case TOKEN_SLASH:         return "/";
        case TOKEN_PERCENT:       return "%";
        case TOKEN_AMP:           return "&";
        case TOKEN_PIPE:          return "|";
        case TOKEN_CARET:         return "^";
        case TOKEN_SHL:           return "<<";
        case TOKEN_SHR:           return ">>";
        case TOKEN_AND_AND:       return "&&";
        case TOKEN_OR_OR:         return "||";
        case TOKEN_EQ_EQ:         return "==";
        case TOKEN_NOT_EQ:        return "!=";
        case TOKEN_LT:            return "<";
        case TOKEN_LE:            return "<=";
        case TOKEN_GT:            return ">";
        case TOKEN_GE:            return ">=";
        case TOKEN_EXCLAMATION:   return "!";
        case TOKEN_COLON_ASSIGN:  return ":=";
        case TOKEN_ASSIGN:        return "=";
        case TOKEN_PLUS_ASSIGN:   return "+=";
        case TOKEN_MINUS_ASSIGN:  return "-=";
        case TOKEN_INC:           return "++";
        case TOKEN_DEC:           return "--";

        case TOKEN_LPAREN:        return "(";
        case TOKEN_RPAREN:        return ")";
        case TOKEN_LBRACE:        return "{";
        case TOKEN_RBRACE:        return "}";
        case TOKEN_LBRACKET:      return "[";
        case TOKEN_RBRACKET:      return "]";
        case TOKEN_COMMA:         return ",";
        case TOKEN_DOT:           return ".";
        case TOKEN_SEMICOLON:     return ";";
        case TOKEN_COLON:         return ":";
        default:                  return "UNKNOWN";
    }
}

void token_print(const Token *tok) {
    printf("[%s:%d:%d] %-14s", tok->pos.filename ? tok->pos.filename : "", tok->pos.line, tok->pos.col, token_type_name(tok->type));
    if (tok->is_auto_semicolon) {
        printf(" (auto-inserted)");
    } else if (tok->length > 0 && tok->start) {
        printf(" '%.*s'", (int)tok->length, tok->start);
    }
    printf("\n");
}
