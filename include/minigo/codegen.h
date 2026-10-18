#ifndef MINIGO_CODEGEN_H
#define MINIGO_CODEGEN_H

#include "minigo/ast.h"
#include "minigo/typecheck.h"

typedef struct {
    FILE *out;
    ASTProgram *prog;
    int label_counter;
    int current_stack_depth;
    bool is_windows_target;
} CodegenX86;

void codegen_x86_init(CodegenX86 *cg, FILE *out, ASTProgram *prog);
bool codegen_x86_emit(CodegenX86 *cg);

#endif /* MINIGO_CODEGEN_H */
