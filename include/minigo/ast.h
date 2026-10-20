#ifndef MINIGO_AST_H
#define MINIGO_AST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "minigo/common.h"
#include "minigo/token.h"

/* Type representations */
typedef enum {
    TYPE_KIND_VOID,
    TYPE_KIND_INT,
    TYPE_KIND_BOOL,
    TYPE_KIND_STRING,
    TYPE_KIND_POINTER,
    TYPE_KIND_STRUCT,
    TYPE_KIND_NAME /* Unresolved type name during parsing */
} TypeKind;

typedef struct TypeNode TypeNode;

typedef struct {
    const char *name;
    TypeNode *type;
} FieldDef;

struct TypeNode {
    TypeKind kind;
    const char *name;          /* For named types or TYPE_KIND_NAME */
    TypeNode *base;            /* For pointer: *base */
    FieldDef *fields;          /* For struct */
    size_t field_count;
};

/* Expression nodes */
typedef enum {
    EXPR_INT_LIT,
    EXPR_STRING_LIT,
    EXPR_IDENT,
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_CALL,
    EXPR_FIELD_ACCESS
} ExprKind;

typedef struct ASTExpr ASTExpr;

typedef struct {
    TokenType op;
    ASTExpr *left;
    ASTExpr *right;
} BinaryExpr;

typedef struct {
    TokenType op;
    ASTExpr *operand;
} UnaryExpr;

typedef struct {
    const char *callee_name;
    ASTExpr **args;
    size_t arg_count;
} CallExpr;

typedef struct {
    ASTExpr *target;
    const char *field_name;
} FieldAccessExpr;

struct ASTExpr {
    ExprKind kind;
    SourcePos pos;
    TypeNode *inferred_type;
    union {
        int64_t int_val;
        const char *str_val;
        const char *ident_name;
        BinaryExpr binary;
        UnaryExpr unary;
        CallExpr call;
        FieldAccessExpr field_access;
    };
};

/* Statement nodes */
typedef enum {
    STMT_BLOCK,
    STMT_EXPR,
    STMT_VAR_DECL,
    STMT_SHORT_VAR_DECL,
    STMT_ASSIGN,
    STMT_RETURN,
    STMT_IF,
    STMT_FOR
} StmtKind;

typedef struct ASTStmt ASTStmt;

typedef struct {
    ASTStmt **stmts;
    size_t count;
} BlockStmt;

typedef struct {
    const char *var_name;
    TypeNode *type;
    ASTExpr *init;
} VarDeclStmt;

typedef struct {
    const char *var_name;
    ASTExpr *init;
} ShortVarDeclStmt;

typedef struct {
    const char *var_name;
    ASTExpr *value;
} AssignStmt;

typedef struct {
    ASTExpr *value; /* Nullable */
} ReturnStmt;

typedef struct {
    ASTExpr *cond;
    ASTStmt *then_branch;
    ASTStmt *else_branch; /* Nullable */
} IfStmt;

typedef struct {
    ASTStmt *init;        /* Nullable */
    ASTExpr *cond;        /* Nullable */
    ASTStmt *post;        /* Nullable */
    ASTStmt *body;
} ForStmt;

struct ASTStmt {
    StmtKind kind;
    SourcePos pos;
    union {
        BlockStmt block;
        ASTExpr *expr_stmt;
        VarDeclStmt var_decl;
        ShortVarDeclStmt short_var_decl;
        AssignStmt assign;
        ReturnStmt ret;
        IfStmt if_stmt;
        ForStmt for_stmt;
    };
};

/* Parameter definition */
typedef struct {
    const char *name;
    TypeNode *type;
} ParamDef;

/* Top-level declaration nodes */
typedef enum {
    DECL_FUNC,
    DECL_VAR,
    DECL_TYPE
} DeclKind;

typedef struct {
    const char *name;
    ParamDef *params;
    size_t param_count;
    TypeNode *return_type;
    ASTStmt *body;
} FuncDecl;

typedef struct {
    DeclKind kind;
    SourcePos pos;
    union {
        FuncDecl func;
        VarDeclStmt var;
        /* struct / custom type decl */
        struct {
            const char *name;
            TypeNode *type;
        } type_decl;
    };
} ASTDecl;

/* Whole file / package program */
typedef struct {
    const char *package_name;
    ASTDecl **decls;
    size_t decl_count;
} ASTProgram;

/* Helper constructors */
TypeNode *type_new_primitive(Arena *arena, TypeKind kind, const char *name);
TypeNode *type_new_pointer(Arena *arena, TypeNode *base);
ASTExpr *expr_new_int_lit(Arena *arena, SourcePos pos, int64_t val);
ASTExpr *expr_new_ident(Arena *arena, SourcePos pos, const char *name);
ASTExpr *expr_new_binary(Arena *arena, SourcePos pos, TokenType op, ASTExpr *left, ASTExpr *right);
ASTExpr *expr_new_call(Arena *arena, SourcePos pos, const char *callee, ASTExpr **args, size_t arg_count);
ASTStmt *stmt_new_return(Arena *arena, SourcePos pos, ASTExpr *val);
ASTStmt *stmt_new_var_decl(Arena *arena, SourcePos pos, const char *name, TypeNode *type, ASTExpr *init);
ASTStmt *stmt_new_block(Arena *arena, SourcePos pos, ASTStmt **stmts, size_t count);

void ast_dump(const ASTProgram *prog, FILE *out);

#ifdef __cplusplus
}
#endif

#endif /* MINIGO_AST_H */
