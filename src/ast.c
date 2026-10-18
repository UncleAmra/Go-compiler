#include "minigo/ast.h"

TypeNode *type_new_primitive(Arena *arena, TypeKind kind, const char *name) {
    TypeNode *t = (TypeNode *)arena_alloc(arena, sizeof(TypeNode));
    t->kind = kind;
    t->name = name ? arena_strdup(arena, name) : NULL;
    t->base = NULL;
    t->fields = NULL;
    t->field_count = 0;
    return t;
}

TypeNode *type_new_pointer(Arena *arena, TypeNode *base) {
    TypeNode *t = (TypeNode *)arena_alloc(arena, sizeof(TypeNode));
    t->kind = TYPE_KIND_POINTER;
    t->name = NULL;
    t->base = base;
    t->fields = NULL;
    t->field_count = 0;
    return t;
}

ASTExpr *expr_new_int_lit(Arena *arena, SourcePos pos, int64_t val) {
    ASTExpr *e = (ASTExpr *)arena_alloc(arena, sizeof(ASTExpr));
    e->kind = EXPR_INT_LIT;
    e->pos = pos;
    e->int_val = val;
    e->inferred_type = NULL;
    return e;
}

ASTExpr *expr_new_ident(Arena *arena, SourcePos pos, const char *name) {
    ASTExpr *e = (ASTExpr *)arena_alloc(arena, sizeof(ASTExpr));
    e->kind = EXPR_IDENT;
    e->pos = pos;
    e->ident_name = arena_strdup(arena, name);
    e->inferred_type = NULL;
    return e;
}

ASTExpr *expr_new_binary(Arena *arena, SourcePos pos, TokenType op, ASTExpr *left, ASTExpr *right) {
    ASTExpr *e = (ASTExpr *)arena_alloc(arena, sizeof(ASTExpr));
    e->kind = EXPR_BINARY;
    e->pos = pos;
    e->binary.op = op;
    e->binary.left = left;
    e->binary.right = right;
    e->inferred_type = NULL;
    return e;
}

ASTExpr *expr_new_call(Arena *arena, SourcePos pos, const char *callee, ASTExpr **args, size_t arg_count) {
    ASTExpr *e = (ASTExpr *)arena_alloc(arena, sizeof(ASTExpr));
    e->kind = EXPR_CALL;
    e->pos = pos;
    e->call.callee_name = arena_strdup(arena, callee);
    e->call.args = args;
    e->call.arg_count = arg_count;
    e->inferred_type = NULL;
    return e;
}

ASTStmt *stmt_new_return(Arena *arena, SourcePos pos, ASTExpr *val) {
    ASTStmt *s = (ASTStmt *)arena_alloc(arena, sizeof(ASTStmt));
    s->kind = STMT_RETURN;
    s->pos = pos;
    s->ret.value = val;
    return s;
}

ASTStmt *stmt_new_var_decl(Arena *arena, SourcePos pos, const char *name, TypeNode *type, ASTExpr *init) {
    ASTStmt *s = (ASTStmt *)arena_alloc(arena, sizeof(ASTStmt));
    s->kind = STMT_VAR_DECL;
    s->pos = pos;
    s->var_decl.var_name = arena_strdup(arena, name);
    s->var_decl.type = type;
    s->var_decl.init = init;
    return s;
}

ASTStmt *stmt_new_block(Arena *arena, SourcePos pos, ASTStmt **stmts, size_t count) {
    ASTStmt *s = (ASTStmt *)arena_alloc(arena, sizeof(ASTStmt));
    s->kind = STMT_BLOCK;
    s->pos = pos;
    s->block.stmts = stmts;
    s->block.count = count;
    return s;
}

static void print_indent(FILE *out, int indent) {
    for (int i = 0; i < indent; i++) {
        fputs("  ", out);
    }
}

static void ast_dump_expr(const ASTExpr *e, FILE *out, int indent) {
    if (!e) {
        print_indent(out, indent);
        fprintf(out, "(null expr)\n");
        return;
    }
    print_indent(out, indent);
    switch (e->kind) {
        case EXPR_INT_LIT:
            fprintf(out, "IntLit(%lld)\n", (long long)e->int_val);
            break;
        case EXPR_STRING_LIT:
            fprintf(out, "StringLit(\"%s\")\n", e->str_val ? e->str_val : "");
            break;
        case EXPR_IDENT:
            fprintf(out, "Ident(%s)\n", e->ident_name);
            break;
        case EXPR_BINARY:
            fprintf(out, "BinaryExpr(%s)\n", token_type_name(e->binary.op));
            ast_dump_expr(e->binary.left, out, indent + 1);
            ast_dump_expr(e->binary.right, out, indent + 1);
            break;
        case EXPR_UNARY:
            fprintf(out, "UnaryExpr(%s)\n", token_type_name(e->unary.op));
            ast_dump_expr(e->unary.operand, out, indent + 1);
            break;
        case EXPR_CALL:
            fprintf(out, "CallExpr(%s, args=%zu)\n", e->call.callee_name, e->call.arg_count);
            for (size_t i = 0; i < e->call.arg_count; i++) {
                ast_dump_expr(e->call.args[i], out, indent + 1);
            }
            break;
        case EXPR_FIELD_ACCESS:
            fprintf(out, "FieldAccess(.%s)\n", e->field_access.field_name);
            ast_dump_expr(e->field_access.target, out, indent + 1);
            break;
    }
}

static void ast_dump_stmt(const ASTStmt *s, FILE *out, int indent) {
    if (!s) return;
    print_indent(out, indent);
    switch (s->kind) {
        case STMT_BLOCK:
            fprintf(out, "BlockStmt (%zu statements)\n", s->block.count);
            for (size_t i = 0; i < s->block.count; i++) {
                ast_dump_stmt(s->block.stmts[i], out, indent + 1);
            }
            break;
        case STMT_EXPR:
            fprintf(out, "ExprStmt:\n");
            ast_dump_expr(s->expr_stmt, out, indent + 1);
            break;
        case STMT_VAR_DECL:
            fprintf(out, "VarDeclStmt: %s\n", s->var_decl.var_name);
            if (s->var_decl.init) {
                ast_dump_expr(s->var_decl.init, out, indent + 1);
            }
            break;
        case STMT_SHORT_VAR_DECL:
            fprintf(out, "ShortVarDeclStmt: %s :=\n", s->short_var_decl.var_name);
            ast_dump_expr(s->short_var_decl.init, out, indent + 1);
            break;
        case STMT_ASSIGN:
            fprintf(out, "AssignStmt: %s =\n", s->assign.var_name);
            ast_dump_expr(s->assign.value, out, indent + 1);
            break;
        case STMT_RETURN:
            fprintf(out, "ReturnStmt:\n");
            if (s->ret.value) {
                ast_dump_expr(s->ret.value, out, indent + 1);
            }
            break;
        case STMT_IF:
            fprintf(out, "IfStmt (Condition):\n");
            ast_dump_expr(s->if_stmt.cond, out, indent + 1);
            print_indent(out, indent);
            fprintf(out, "Then Branch:\n");
            ast_dump_stmt(s->if_stmt.then_branch, out, indent + 1);
            if (s->if_stmt.else_branch) {
                print_indent(out, indent);
                fprintf(out, "Else Branch:\n");
                ast_dump_stmt(s->if_stmt.else_branch, out, indent + 1);
            }
            break;
        case STMT_FOR:
            fprintf(out, "ForStmt:\n");
            if (s->for_stmt.cond) {
                print_indent(out, indent + 1);
                fprintf(out, "Cond:\n");
                ast_dump_expr(s->for_stmt.cond, out, indent + 2);
            }
            print_indent(out, indent + 1);
            fprintf(out, "Body:\n");
            ast_dump_stmt(s->for_stmt.body, out, indent + 2);
            break;
    }
}

void ast_dump(const ASTProgram *prog, FILE *out) {
    if (!prog) {
        fprintf(out, "Empty Program AST\n");
        return;
    }
    fprintf(out, "Program (Package: %s, Declarations: %zu)\n",
            prog->package_name ? prog->package_name : "<anonymous>",
            prog->decl_count);

    for (size_t i = 0; i < prog->decl_count; i++) {
        ASTDecl *d = prog->decls[i];
        switch (d->kind) {
            case DECL_FUNC:
                print_indent(out, 1);
                fprintf(out, "FuncDecl: func %s (params: %zu)\n", d->func.name, d->func.param_count);
                for (size_t p = 0; p < d->func.param_count; p++) {
                    print_indent(out, 2);
                    fprintf(out, "Param: %s : %s\n", d->func.params[p].name,
                            d->func.params[p].type && d->func.params[p].type->name ? d->func.params[p].type->name : "any");
                }
                if (d->func.body) {
                    ast_dump_stmt(d->func.body, out, 2);
                }
                break;
            case DECL_VAR:
                print_indent(out, 1);
                fprintf(out, "GlobalVarDecl: var %s\n", d->var.var_name);
                if (d->var.init) {
                    ast_dump_expr(d->var.init, out, 2);
                }
                break;
            case DECL_TYPE:
                print_indent(out, 1);
                fprintf(out, "TypeDecl: type %s struct\n", d->type_decl.name);
                break;
        }
    }
}
