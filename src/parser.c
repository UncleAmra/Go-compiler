#include "minigo/parser.h"

static void advance(Parser *p) {
    p->current = p->peek;
    p->peek = lexer_next_token(&p->lexer);
}

static bool check(Parser *p, TokenType type) {
    return p->current.type == type;
}

static bool match(Parser *p, TokenType type) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    return false;
}

static Token consume(Parser *p, TokenType type, const char *err_msg) {
    if (check(p, type)) {
        Token tok = p->current;
        advance(p);
        return tok;
    }
    report_error(p->current.pos, "%s (got '%s')", err_msg, token_type_name(p->current.type));
    p->had_error = true;
    return p->current;
}

static void match_semicolon(Parser *p) {
    if (check(p, TOKEN_SEMICOLON)) {
        advance(p);
    }
}

void parser_init(Parser *p, const char *filename, const char *source, size_t length, Arena *arena) {
    lexer_init(&p->lexer, filename, source, length);
    p->arena = arena;
    p->had_error = false;
    p->panic_mode = false;
    /* Prime the pump */
    p->peek = lexer_next_token(&p->lexer);
    advance(p);
}

/* Forward declarations */
static ASTExpr *parse_expression(Parser *p);
static ASTStmt *parse_statement(Parser *p);
static ASTStmt *parse_block(Parser *p);
static TypeNode *parse_type(Parser *p);

static int get_binary_precedence(TokenType op) {
    switch (op) {
        case TOKEN_OR_OR:       return 1;
        case TOKEN_AND_AND:     return 2;
        case TOKEN_EQ_EQ:
        case TOKEN_NOT_EQ:
        case TOKEN_LT:
        case TOKEN_LE:
        case TOKEN_GT:
        case TOKEN_GE:          return 3;
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_PIPE:
        case TOKEN_CARET:       return 4;
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
        case TOKEN_SHL:
        case TOKEN_SHR:
        case TOKEN_AMP:         return 5;
        default:                return 0;
    }
}

static ASTExpr *parse_primary(Parser *p) {
    SourcePos pos = p->current.pos;

    if (check(p, TOKEN_INT_LIT)) {
        int64_t val = p->current.int_val;
        advance(p);
        return expr_new_int_lit(p->arena, pos, val);
    }

    if (check(p, TOKEN_STRING_LIT)) {
        char *str = arena_strndup(p->arena, p->current.start + 1, p->current.length - 2);
        advance(p);
        ASTExpr *e = (ASTExpr *)arena_alloc(p->arena, sizeof(ASTExpr));
        e->kind = EXPR_STRING_LIT;
        e->pos = pos;
        e->str_val = str;
        e->inferred_type = NULL;
        return e;
    }

    if (check(p, TOKEN_IDENT)) {
        char *name = arena_strndup(p->arena, p->current.start, p->current.length);
        advance(p);

        /* Check for function call: name(args...) */
        if (match(p, TOKEN_LPAREN)) {
            ASTExpr **args = NULL;
            size_t count = 0;
            size_t cap = 4;
            args = (ASTExpr **)arena_alloc(p->arena, cap * sizeof(ASTExpr *));

            if (!check(p, TOKEN_RPAREN)) {
                do {
                    if (count >= cap) {
                        cap *= 2;
                        ASTExpr **new_args = (ASTExpr **)arena_alloc(p->arena, cap * sizeof(ASTExpr *));
                        memcpy(new_args, args, count * sizeof(ASTExpr *));
                        args = new_args;
                    }
                    args[count++] = parse_expression(p);
                } while (match(p, TOKEN_COMMA));
            }
            consume(p, TOKEN_RPAREN, "Expected ')' after arguments");
            return expr_new_call(p->arena, pos, name, args, count);
        }

        return expr_new_ident(p->arena, pos, name);
    }

    if (match(p, TOKEN_LPAREN)) {
        ASTExpr *e = parse_expression(p);
        consume(p, TOKEN_RPAREN, "Expected ')' after expression");
        return e;
    }

    report_error(pos, "Expected expression, got '%s'", token_type_name(p->current.type));
    advance(p);
    return expr_new_int_lit(p->arena, pos, 0);
}

static ASTExpr *parse_unary(Parser *p) {
    if (check(p, TOKEN_MINUS) || check(p, TOKEN_EXCLAMATION) || check(p, TOKEN_STAR) || check(p, TOKEN_AMP)) {
        TokenType op = p->current.type;
        SourcePos pos = p->current.pos;
        advance(p);
        ASTExpr *operand = parse_unary(p);
        ASTExpr *e = (ASTExpr *)arena_alloc(p->arena, sizeof(ASTExpr));
        e->kind = EXPR_UNARY;
        e->pos = pos;
        e->unary.op = op;
        e->unary.operand = operand;
        e->inferred_type = NULL;
        return e;
    }
    return parse_primary(p);
}

static ASTExpr *parse_binary(Parser *p, int min_prec) {
    ASTExpr *left = parse_unary(p);

    while (1) {
        TokenType op = p->current.type;
        int prec = get_binary_precedence(op);
        if (prec < min_prec) break;

        SourcePos pos = p->current.pos;
        advance(p);
        ASTExpr *right = parse_binary(p, prec + 1);
        left = expr_new_binary(p->arena, pos, op, left, right);
    }
    return left;
}

static ASTExpr *parse_expression(Parser *p) {
    return parse_binary(p, 1);
}

static TypeNode *parse_type(Parser *p) {
    if (match(p, TOKEN_STAR)) {
        TypeNode *base = parse_type(p);
        return type_new_pointer(p->arena, base);
    }

    if (check(p, TOKEN_IDENT)) {
        char *name = arena_strndup(p->arena, p->current.start, p->current.length);
        advance(p);
        TypeKind kind = TYPE_KIND_NAME;
        if (strcmp(name, "int") == 0) kind = TYPE_KIND_INT;
        else if (strcmp(name, "bool") == 0) kind = TYPE_KIND_BOOL;
        else if (strcmp(name, "string") == 0) kind = TYPE_KIND_STRING;
        return type_new_primitive(p->arena, kind, name);
    }

    if (match(p, TOKEN_STRUCT)) {
        consume(p, TOKEN_LBRACE, "Expected '{' after struct");
        TypeNode *st = (TypeNode *)arena_alloc(p->arena, sizeof(TypeNode));
        st->kind = TYPE_KIND_STRUCT;
        st->name = NULL;
        st->base = NULL;
        st->fields = NULL;
        st->field_count = 0;

        FieldDef fields[64];
        size_t count = 0;
        while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
            Token f_tok = consume(p, TOKEN_IDENT, "Expected field name");
            TypeNode *f_type = parse_type(p);
            match_semicolon(p);
            if (count < 64) {
                fields[count].name = arena_strndup(p->arena, f_tok.start, f_tok.length);
                fields[count].type = f_type;
                count++;
            }
        }
        consume(p, TOKEN_RBRACE, "Expected '}' after struct body");
        st->field_count = count;
        st->fields = (FieldDef *)arena_alloc(p->arena, count * sizeof(FieldDef));
        memcpy(st->fields, fields, count * sizeof(FieldDef));
        return st;
    }

    report_error(p->current.pos, "Expected type name");
    return type_new_primitive(p->arena, TYPE_KIND_VOID, "void");
}

static ASTStmt *parse_statement(Parser *p) {
    while (check(p, TOKEN_SEMICOLON)) {
        advance(p);
    }
    SourcePos pos = p->current.pos;

    /* Block */
    if (check(p, TOKEN_LBRACE)) {
        ASTStmt *b = parse_block(p);
        match_semicolon(p);
        return b;
    }

    /* Return statement */
    if (match(p, TOKEN_RETURN)) {
        ASTExpr *val = NULL;
        if (!check(p, TOKEN_SEMICOLON) && !check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
            val = parse_expression(p);
        }
        match_semicolon(p);
        return stmt_new_return(p->arena, pos, val);
    }

    /* var declaration: var x int = 5 */
    if (match(p, TOKEN_VAR)) {
        Token id_tok = consume(p, TOKEN_IDENT, "Expected variable name after 'var'");
        char *vname = arena_strndup(p->arena, id_tok.start, id_tok.length);
        TypeNode *type = NULL;
        if (!check(p, TOKEN_ASSIGN) && !check(p, TOKEN_SEMICOLON)) {
            type = parse_type(p);
        }
        ASTExpr *init = NULL;
        if (match(p, TOKEN_ASSIGN)) {
            init = parse_expression(p);
        }
        match_semicolon(p);
        return stmt_new_var_decl(p->arena, pos, vname, type, init);
    }

    /* If statement: if cond { ... } else { ... } */
    if (match(p, TOKEN_IF)) {
        ASTExpr *cond = parse_expression(p);
        ASTStmt *then_br = parse_block(p);
        ASTStmt *else_br = NULL;
        if (match(p, TOKEN_ELSE)) {
            if (check(p, TOKEN_IF)) {
                else_br = parse_statement(p);
            } else {
                else_br = parse_block(p);
            }
        }
        match_semicolon(p);
        ASTStmt *s = (ASTStmt *)arena_alloc(p->arena, sizeof(ASTStmt));
        s->kind = STMT_IF;
        s->pos = pos;
        s->if_stmt.cond = cond;
        s->if_stmt.then_branch = then_br;
        s->if_stmt.else_branch = else_br;
        return s;
    }

    /* For loop: for cond { ... } */
    if (match(p, TOKEN_FOR)) {
        ASTExpr *cond = NULL;
        if (!check(p, TOKEN_LBRACE)) {
            cond = parse_expression(p);
        }
        ASTStmt *body = parse_block(p);
        match_semicolon(p);
        ASTStmt *s = (ASTStmt *)arena_alloc(p->arena, sizeof(ASTStmt));
        s->kind = STMT_FOR;
        s->pos = pos;
        s->for_stmt.init = NULL;
        s->for_stmt.cond = cond;
        s->for_stmt.post = NULL;
        s->for_stmt.body = body;
        return s;
    }

    /* Identifier-led statement: short-decl (x := 5), assignment (x = 5), or call */
    if (check(p, TOKEN_IDENT)) {
        Token id_tok = p->current;
        char *ident_str = arena_strndup(p->arena, id_tok.start, id_tok.length);
        advance(p);

        /* Short variable declaration: x := expr */
        if (match(p, TOKEN_COLON_ASSIGN)) {
            ASTExpr *init = parse_expression(p);
            match_semicolon(p);
            ASTStmt *s = (ASTStmt *)arena_alloc(p->arena, sizeof(ASTStmt));
            s->kind = STMT_SHORT_VAR_DECL;
            s->pos = pos;
            s->short_var_decl.var_name = ident_str;
            s->short_var_decl.init = init;
            return s;
        }

        /* Assignment: x = expr */
        if (match(p, TOKEN_ASSIGN)) {
            ASTExpr *val = parse_expression(p);
            match_semicolon(p);
            ASTStmt *s = (ASTStmt *)arena_alloc(p->arena, sizeof(ASTStmt));
            s->kind = STMT_ASSIGN;
            s->pos = pos;
            s->assign.var_name = ident_str;
            s->assign.value = val;
            return s;
        }

        /* Function call as statement: ident(args...) */
        if (match(p, TOKEN_LPAREN)) {
            ASTExpr **args = NULL;
            size_t count = 0;
            size_t cap = 4;
            args = (ASTExpr **)arena_alloc(p->arena, cap * sizeof(ASTExpr *));

            if (!check(p, TOKEN_RPAREN)) {
                do {
                    if (count >= cap) {
                        cap *= 2;
                        ASTExpr **new_args = (ASTExpr **)arena_alloc(p->arena, cap * sizeof(ASTExpr *));
                        memcpy(new_args, args, count * sizeof(ASTExpr *));
                        args = new_args;
                    }
                    args[count++] = parse_expression(p);
                } while (match(p, TOKEN_COMMA));
            }
            consume(p, TOKEN_RPAREN, "Expected ')' after function arguments");
            match_semicolon(p);

            ASTExpr *call_expr = expr_new_call(p->arena, pos, ident_str, args, count);
            ASTStmt *s = (ASTStmt *)arena_alloc(p->arena, sizeof(ASTStmt));
            s->kind = STMT_EXPR;
            s->pos = pos;
            s->expr_stmt = call_expr;
            return s;
        }
    }

    /* Fallback: Expression statement */
    ASTExpr *e = parse_expression(p);
    match_semicolon(p);
    ASTStmt *s = (ASTStmt *)arena_alloc(p->arena, sizeof(ASTStmt));
    s->kind = STMT_EXPR;
    s->pos = pos;
    s->expr_stmt = e;
    return s;
}

static ASTStmt *parse_block(Parser *p) {
    SourcePos pos = p->current.pos;
    consume(p, TOKEN_LBRACE, "Expected '{'");

    size_t cap = 8;
    size_t count = 0;
    ASTStmt **stmts = (ASTStmt **)arena_alloc(p->arena, cap * sizeof(ASTStmt *));

    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        if (check(p, TOKEN_SEMICOLON)) {
            advance(p);
            continue;
        }
        if (count >= cap) {
            cap *= 2;
            ASTStmt **new_stmts = (ASTStmt **)arena_alloc(p->arena, cap * sizeof(ASTStmt *));
            memcpy(new_stmts, stmts, count * sizeof(ASTStmt *));
            stmts = new_stmts;
        }
        stmts[count++] = parse_statement(p);
    }

    consume(p, TOKEN_RBRACE, "Expected '}'");
    return stmt_new_block(p->arena, pos, stmts, count);
}

static ASTDecl *parse_func_decl(Parser *p) {
    SourcePos pos = p->current.pos;
    consume(p, TOKEN_FUNC, "Expected 'func'");

    Token name_tok = consume(p, TOKEN_IDENT, "Expected function name");
    char *func_name = arena_strndup(p->arena, name_tok.start, name_tok.length);

    consume(p, TOKEN_LPAREN, "Expected '(' after function name");

    ParamDef params[32];
    size_t param_count = 0;
    if (!check(p, TOKEN_RPAREN)) {
        do {
            Token p_tok = consume(p, TOKEN_IDENT, "Expected parameter name");
            TypeNode *p_type = parse_type(p);
            if (param_count < 32) {
                params[param_count].name = arena_strndup(p->arena, p_tok.start, p_tok.length);
                params[param_count].type = p_type;
                param_count++;
            }
        } while (match(p, TOKEN_COMMA));
    }
    consume(p, TOKEN_RPAREN, "Expected ')' after parameters");

    TypeNode *ret_type = NULL;
    if (!check(p, TOKEN_LBRACE) && !check(p, TOKEN_SEMICOLON)) {
        ret_type = parse_type(p);
    } else {
        ret_type = type_new_primitive(p->arena, TYPE_KIND_VOID, "void");
    }

    ASTStmt *body = parse_block(p);
    match_semicolon(p);

    ASTDecl *decl = (ASTDecl *)arena_alloc(p->arena, sizeof(ASTDecl));
    decl->kind = DECL_FUNC;
    decl->pos = pos;
    decl->func.name = func_name;
    decl->func.param_count = param_count;
    decl->func.params = (ParamDef *)arena_alloc(p->arena, param_count * sizeof(ParamDef));
    memcpy(decl->func.params, params, param_count * sizeof(ParamDef));
    decl->func.return_type = ret_type;
    decl->func.body = body;
    return decl;
}

static ASTDecl *parse_type_decl(Parser *p) {
    SourcePos pos = p->current.pos;
    consume(p, TOKEN_TYPE, "Expected 'type'");

    Token name_tok = consume(p, TOKEN_IDENT, "Expected type name");
    char *tname = arena_strndup(p->arena, name_tok.start, name_tok.length);
    TypeNode *st = parse_type(p);
    match_semicolon(p);

    ASTDecl *decl = (ASTDecl *)arena_alloc(p->arena, sizeof(ASTDecl));
    decl->kind = DECL_TYPE;
    decl->pos = pos;
    decl->type_decl.name = tname;
    decl->type_decl.type = st;
    return decl;
}

ASTProgram *parser_parse_program(Parser *p) {
    ASTProgram *prog = (ASTProgram *)arena_alloc(p->arena, sizeof(ASTProgram));
    prog->package_name = NULL;
    prog->decls = NULL;
    prog->decl_count = 0;

    /* Package declaration */
    if (match(p, TOKEN_PACKAGE)) {
        Token pkg_tok = consume(p, TOKEN_IDENT, "Expected package name");
        prog->package_name = arena_strndup(p->arena, pkg_tok.start, pkg_tok.length);
        match_semicolon(p);
    }

    /* Optional import statements */
    while (match(p, TOKEN_IMPORT)) {
        if (check(p, TOKEN_STRING_LIT)) {
            advance(p);
            match_semicolon(p);
        } else if (match(p, TOKEN_LPAREN)) {
            while (!check(p, TOKEN_RPAREN) && !check(p, TOKEN_EOF)) {
                consume(p, TOKEN_STRING_LIT, "Expected imported package string");
                match_semicolon(p);
            }
            consume(p, TOKEN_RPAREN, "Expected ')' after import list");
            match_semicolon(p);
        }
    }

    size_t cap = 16;
    size_t count = 0;
    ASTDecl **decls = (ASTDecl **)arena_alloc(p->arena, cap * sizeof(ASTDecl *));

    while (!check(p, TOKEN_EOF)) {
        if (count >= cap) {
            cap *= 2;
            ASTDecl **new_decls = (ASTDecl **)arena_alloc(p->arena, cap * sizeof(ASTDecl *));
            memcpy(new_decls, decls, count * sizeof(ASTDecl *));
            decls = new_decls;
        }

        if (check(p, TOKEN_FUNC)) {
            decls[count++] = parse_func_decl(p);
        } else if (check(p, TOKEN_TYPE)) {
            decls[count++] = parse_type_decl(p);
        } else if (match(p, TOKEN_VAR)) {
            SourcePos pos = p->current.pos;
            Token id_tok = consume(p, TOKEN_IDENT, "Expected variable name");
            char *vname = arena_strndup(p->arena, id_tok.start, id_tok.length);
            TypeNode *type = NULL;
            if (!check(p, TOKEN_ASSIGN) && !check(p, TOKEN_SEMICOLON)) {
                type = parse_type(p);
            }
            ASTExpr *init = NULL;
            if (match(p, TOKEN_ASSIGN)) {
                init = parse_expression(p);
            }
            match_semicolon(p);

            ASTDecl *d = (ASTDecl *)arena_alloc(p->arena, sizeof(ASTDecl));
            d->kind = DECL_VAR;
            d->pos = pos;
            d->var.var_name = vname;
            d->var.type = type;
            d->var.init = init;
            decls[count++] = d;
        } else if (check(p, TOKEN_SEMICOLON)) {
            advance(p);
        } else {
            report_error(p->current.pos, "Unexpected top-level token: '%s'", token_type_name(p->current.type));
            advance(p);
        }
    }

    prog->decls = decls;
    prog->decl_count = count;
    return prog;
}
