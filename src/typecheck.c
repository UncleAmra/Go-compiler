#include "minigo/typecheck.h"

static Scope *scope_push(TypeChecker *tc) {
    Scope *s = (Scope *)arena_alloc(tc->arena, sizeof(Scope));
    s->symbols = NULL;
    s->parent = tc->current_scope;
    tc->current_scope = s;
    return s;
}

static void scope_pop(TypeChecker *tc) {
    if (tc->current_scope) {
        tc->current_scope = tc->current_scope->parent;
    }
}

static void scope_insert(TypeChecker *tc, const char *name, SymbolKind kind, TypeNode *type) {
    Symbol *sym = (Symbol *)arena_alloc(tc->arena, sizeof(Symbol));
    sym->name = arena_strdup(tc->arena, name);
    sym->kind = kind;
    sym->type = type;
    sym->stack_offset = 0;
    sym->next = tc->current_scope->symbols;
    tc->current_scope->symbols = sym;
}

static Symbol *scope_lookup(TypeChecker *tc, const char *name) {
    for (Scope *s = tc->current_scope; s != NULL; s = s->parent) {
        for (Symbol *sym = s->symbols; sym != NULL; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

void typechecker_init(TypeChecker *tc, Arena *arena) {
    tc->arena = arena;
    tc->current_scope = NULL;
    tc->has_error = false;

    tc->type_int = type_new_primitive(arena, TYPE_KIND_INT, "int");
    tc->type_bool = type_new_primitive(arena, TYPE_KIND_BOOL, "bool");
    tc->type_string = type_new_primitive(arena, TYPE_KIND_STRING, "string");
    tc->type_void = type_new_primitive(arena, TYPE_KIND_VOID, "void");

    /* Global scope */
    scope_push(tc);

    /* Built-in symbols (e.g. print) */
    scope_insert(tc, "print", SYM_FUNC, tc->type_void);
    scope_insert(tc, "println", SYM_FUNC, tc->type_void);
}

static bool type_equals(TypeNode *a, TypeNode *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    if (a->kind == TYPE_KIND_POINTER) {
        return type_equals(a->base, b->base);
    }
    return true;
}

static TypeNode *check_expr(TypeChecker *tc, ASTExpr *e) {
    if (!e) return tc->type_void;

    switch (e->kind) {
        case EXPR_INT_LIT:
            e->inferred_type = tc->type_int;
            return tc->type_int;

        case EXPR_STRING_LIT:
            e->inferred_type = tc->type_string;
            return tc->type_string;

        case EXPR_IDENT: {
            Symbol *sym = scope_lookup(tc, e->ident_name);
            if (!sym) {
                report_error(e->pos, "Undeclared identifier: '%s'", e->ident_name);
                tc->has_error = true;
                e->inferred_type = tc->type_int;
                return tc->type_int;
            }
            e->inferred_type = sym->type ? sym->type : tc->type_int;
            return e->inferred_type;
        }

        case EXPR_BINARY: {
            TypeNode *lt = check_expr(tc, e->binary.left);
            TypeNode *rt = check_expr(tc, e->binary.right);

            if (!type_equals(lt, rt)) {
                report_warning(e->pos, "Type mismatch in binary expression between '%s' and '%s'",
                               lt && lt->name ? lt->name : "unknown",
                               rt && rt->name ? rt->name : "unknown");
            }

            switch (e->binary.op) {
                case TOKEN_PLUS:
                case TOKEN_MINUS:
                case TOKEN_STAR:
                case TOKEN_SLASH:
                case TOKEN_PERCENT:
                case TOKEN_AMP:
                case TOKEN_PIPE:
                case TOKEN_CARET:
                case TOKEN_SHL:
                case TOKEN_SHR:
                    e->inferred_type = lt ? lt : tc->type_int;
                    return e->inferred_type;

                case TOKEN_EQ_EQ:
                case TOKEN_NOT_EQ:
                case TOKEN_LT:
                case TOKEN_LE:
                case TOKEN_GT:
                case TOKEN_GE:
                case TOKEN_AND_AND:
                case TOKEN_OR_OR:
                    e->inferred_type = tc->type_bool;
                    return tc->type_bool;

                default:
                    e->inferred_type = tc->type_int;
                    return tc->type_int;
            }
        }

        case EXPR_UNARY: {
            TypeNode *sub = check_expr(tc, e->unary.operand);
            if (e->unary.op == TOKEN_EXCLAMATION) {
                e->inferred_type = tc->type_bool;
            } else if (e->unary.op == TOKEN_AMP) {
                e->inferred_type = type_new_pointer(tc->arena, sub);
            } else if (e->unary.op == TOKEN_STAR) {
                e->inferred_type = (sub && sub->base) ? sub->base : tc->type_int;
            } else {
                e->inferred_type = sub;
            }
            return e->inferred_type;
        }

        case EXPR_CALL: {
            Symbol *sym = scope_lookup(tc, e->call.callee_name);
            if (!sym) {
                report_warning(e->pos, "Call to undeclared function: '%s'", e->call.callee_name);
            }
            for (size_t i = 0; i < e->call.arg_count; i++) {
                check_expr(tc, e->call.args[i]);
            }
            e->inferred_type = (sym && sym->type) ? sym->type : tc->type_int;
            return e->inferred_type;
        }

        case EXPR_FIELD_ACCESS:
            check_expr(tc, e->field_access.target);
            e->inferred_type = tc->type_int;
            return tc->type_int;
    }
    return tc->type_int;
}

static void check_stmt(TypeChecker *tc, ASTStmt *s, TypeNode *expected_ret) {
    if (!s) return;

    switch (s->kind) {
        case STMT_BLOCK:
            scope_push(tc);
            for (size_t i = 0; i < s->block.count; i++) {
                check_stmt(tc, s->block.stmts[i], expected_ret);
            }
            scope_pop(tc);
            break;

        case STMT_EXPR:
            check_expr(tc, s->expr_stmt);
            break;

        case STMT_VAR_DECL: {
            TypeNode *t = s->var_decl.type ? s->var_decl.type : tc->type_int;
            if (s->var_decl.init) {
                TypeNode *init_t = check_expr(tc, s->var_decl.init);
                if (s->var_decl.type && !type_equals(s->var_decl.type, init_t)) {
                    report_warning(s->pos, "Variable '%s' initialized with mismatched type", s->var_decl.var_name);
                }
                if (!s->var_decl.type) t = init_t;
            }
            scope_insert(tc, s->var_decl.var_name, SYM_VAR, t);
            break;
        }

        case STMT_SHORT_VAR_DECL: {
            TypeNode *init_t = check_expr(tc, s->short_var_decl.init);
            scope_insert(tc, s->short_var_decl.var_name, SYM_VAR, init_t ? init_t : tc->type_int);
            break;
        }

        case STMT_ASSIGN: {
            Symbol *sym = scope_lookup(tc, s->assign.var_name);
            if (!sym) {
                report_error(s->pos, "Assignment to undefined variable: '%s'", s->assign.var_name);
                tc->has_error = true;
            }
            check_expr(tc, s->assign.value);
            break;
        }

        case STMT_RETURN: {
            if (s->ret.value) {
                TypeNode *ret_t = check_expr(tc, s->ret.value);
                if (expected_ret && expected_ret->kind != TYPE_KIND_VOID && !type_equals(expected_ret, ret_t)) {
                    report_warning(s->pos, "Return type mismatch");
                }
            }
            break;
        }

        case STMT_IF:
            check_expr(tc, s->if_stmt.cond);
            check_stmt(tc, s->if_stmt.then_branch, expected_ret);
            if (s->if_stmt.else_branch) {
                check_stmt(tc, s->if_stmt.else_branch, expected_ret);
            }
            break;

        case STMT_FOR:
            if (s->for_stmt.cond) {
                check_expr(tc, s->for_stmt.cond);
            }
            check_stmt(tc, s->for_stmt.body, expected_ret);
            break;
    }
}

bool typecheck_program(TypeChecker *tc, ASTProgram *prog) {
    if (!prog) return true;

    /* First pass: Register all global declarations */
    for (size_t i = 0; i < prog->decl_count; i++) {
        ASTDecl *d = prog->decls[i];
        if (d->kind == DECL_FUNC) {
            scope_insert(tc, d->func.name, SYM_FUNC, d->func.return_type);
        } else if (d->kind == DECL_VAR) {
            scope_insert(tc, d->var.var_name, SYM_VAR, d->var.type ? d->var.type : tc->type_int);
        } else if (d->kind == DECL_TYPE) {
            scope_insert(tc, d->type_decl.name, SYM_TYPE, d->type_decl.type);
        }
    }

    /* Second pass: Check function bodies */
    for (size_t i = 0; i < prog->decl_count; i++) {
        ASTDecl *d = prog->decls[i];
        if (d->kind == DECL_FUNC) {
            scope_push(tc);
            /* Add parameters to function scope */
            for (size_t p = 0; p < d->func.param_count; p++) {
                scope_insert(tc, d->func.params[p].name, SYM_VAR,
                             d->func.params[p].type ? d->func.params[p].type : tc->type_int);
            }
            check_stmt(tc, d->func.body, d->func.return_type);
            scope_pop(tc);
        }
    }

    return !tc->has_error;
}
