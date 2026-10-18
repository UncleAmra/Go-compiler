#ifndef MINIGO_TYPECHECK_H
#define MINIGO_TYPECHECK_H

#include "minigo/ast.h"

typedef struct Symbol Symbol;

typedef enum {
    SYM_VAR,
    SYM_FUNC,
    SYM_TYPE
} SymbolKind;

struct Symbol {
    const char *name;
    SymbolKind kind;
    TypeNode *type;
    int stack_offset; /* For code generation */
    Symbol *next;
};

typedef struct Scope {
    Symbol *symbols;
    struct Scope *parent;
} Scope;

typedef struct {
    Arena *arena;
    Scope *current_scope;
    TypeNode *type_int;
    TypeNode *type_bool;
    TypeNode *type_string;
    TypeNode *type_void;
    bool has_error;
} TypeChecker;

void typechecker_init(TypeChecker *tc, Arena *arena);
bool typecheck_program(TypeChecker *tc, ASTProgram *prog);

#endif /* MINIGO_TYPECHECK_H */
