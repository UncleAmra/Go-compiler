#include "minigo/codegen.h"

typedef struct VarLocation {
    const char *name;
    int offset;
    struct VarLocation *next;
} VarLocation;

typedef struct {
    CodegenX86 *cg;
    VarLocation *locals;
    int stack_offset;
    int current_label;
    const char *current_func_name;
} FuncContext;

void codegen_x86_init(CodegenX86 *cg, FILE *out, ASTProgram *prog) {
    cg->out = out;
    cg->prog = prog;
    cg->label_counter = 0;
    cg->current_stack_depth = 0;
#ifdef _WIN32
    cg->is_windows_target = true;
#else
    cg->is_windows_target = false;
#endif
}

static int alloc_local_var(FuncContext *ctx, const char *name) {
    ctx->stack_offset += 8;
    VarLocation *loc = (VarLocation *)malloc(sizeof(VarLocation));
    loc->name = name;
    loc->offset = -ctx->stack_offset;
    loc->next = ctx->locals;
    ctx->locals = loc;
    return loc->offset;
}

static int find_local_var(FuncContext *ctx, const char *name) {
    for (VarLocation *loc = ctx->locals; loc != NULL; loc = loc->next) {
        if (strcmp(loc->name, name) == 0) {
            return loc->offset;
        }
    }
    return 0;
}

static void free_local_vars(FuncContext *ctx) {
    VarLocation *loc = ctx->locals;
    while (loc) {
        VarLocation *next = loc->next;
        free(loc);
        loc = next;
    }
    ctx->locals = NULL;
}

static void emit_expr(FuncContext *ctx, ASTExpr *e);

static void emit_expr(FuncContext *ctx, ASTExpr *e) {
    FILE *out = ctx->cg->out;
    if (!e) return;

    switch (e->kind) {
        case EXPR_INT_LIT:
            fprintf(out, "    movq $%lld, %%rax\n", (long long)e->int_val);
            break;

        case EXPR_IDENT: {
            int off = find_local_var(ctx, e->ident_name);
            if (off != 0) {
                fprintf(out, "    movq %d(%%rbp), %%rax  # load %s\n", off, e->ident_name);
            } else {
                /* Global variable or fallback */
                fprintf(out, "    movq %s(%%rip), %%rax\n", e->ident_name);
            }
            break;
        }

        case EXPR_BINARY: {
            emit_expr(ctx, e->binary.left);
            fprintf(out, "    pushq %%rax\n");
            emit_expr(ctx, e->binary.right);
            fprintf(out, "    popq %%rcx\n"); /* rcx = left, rax = right */

            switch (e->binary.op) {
                case TOKEN_PLUS:
                    fprintf(out, "    addq %%rcx, %%rax\n");
                    break;
                case TOKEN_MINUS:
                    fprintf(out, "    subq %%rax, %%rcx\n");
                    fprintf(out, "    movq %%rcx, %%rax\n");
                    break;
                case TOKEN_STAR:
                    fprintf(out, "    imulq %%rcx, %%rax\n");
                    break;
                case TOKEN_SLASH:
                    /* rcx = dividend, rax = divisor */
                    fprintf(out, "    movq %%rax, %%rbx\n");
                    fprintf(out, "    movq %%rcx, %%rax\n");
                    fprintf(out, "    cqto\n");
                    fprintf(out, "    idivq %%rbx\n");
                    break;
                case TOKEN_PERCENT:
                    fprintf(out, "    movq %%rax, %%rbx\n");
                    fprintf(out, "    movq %%rcx, %%rax\n");
                    fprintf(out, "    cqto\n");
                    fprintf(out, "    idivq %%rbx\n");
                    fprintf(out, "    movq %%rdx, %%rax\n");
                    break;
                case TOKEN_EQ_EQ:
                    fprintf(out, "    cmpq %%rax, %%rcx\n");
                    fprintf(out, "    sete %%al\n");
                    fprintf(out, "    movzbq %%al, %%rax\n");
                    break;
                case TOKEN_NOT_EQ:
                    fprintf(out, "    cmpq %%rax, %%rcx\n");
                    fprintf(out, "    setne %%al\n");
                    fprintf(out, "    movzbq %%al, %%rax\n");
                    break;
                case TOKEN_LT:
                    fprintf(out, "    cmpq %%rax, %%rcx\n");
                    fprintf(out, "    setl %%al\n");
                    fprintf(out, "    movzbq %%al, %%rax\n");
                    break;
                case TOKEN_LE:
                    fprintf(out, "    cmpq %%rax, %%rcx\n");
                    fprintf(out, "    setle %%al\n");
                    fprintf(out, "    movzbq %%al, %%rax\n");
                    break;
                case TOKEN_GT:
                    fprintf(out, "    cmpq %%rax, %%rcx\n");
                    fprintf(out, "    setg %%al\n");
                    fprintf(out, "    movzbq %%al, %%rax\n");
                    break;
                case TOKEN_GE:
                    fprintf(out, "    cmpq %%rax, %%rcx\n");
                    fprintf(out, "    setge %%al\n");
                    fprintf(out, "    movzbq %%al, %%rax\n");
                    break;
                default:
                    fprintf(out, "    addq %%rcx, %%rax\n");
                    break;
            }
            break;
        }

        case EXPR_UNARY: {
            emit_expr(ctx, e->unary.operand);
            if (e->unary.op == TOKEN_MINUS) {
                fprintf(out, "    negq %%rax\n");
            } else if (e->unary.op == TOKEN_EXCLAMATION) {
                fprintf(out, "    cmpq $0, %%rax\n");
                fprintf(out, "    sete %%al\n");
                fprintf(out, "    movzbq %%al, %%rax\n");
            }
            break;
        }

        case EXPR_CALL: {
            /* Built-in print / println */
            if (strcmp(e->call.callee_name, "println") == 0 || strcmp(e->call.callee_name, "print") == 0) {
                if (e->call.arg_count > 0) {
                    emit_expr(ctx, e->call.args[0]);
                    if (ctx->cg->is_windows_target) {
                        fprintf(out, "    movq %%rax, %%rdx\n");
                        fprintf(out, "    leaq .LC_int_fmt(%%rip), %%rcx\n");
                        fprintf(out, "    subq $32, %%rsp\n");
                        fprintf(out, "    call printf\n");
                        fprintf(out, "    addq $32, %%rsp\n");
                    } else {
                        fprintf(out, "    movq %%rax, %%rsi\n");
                        fprintf(out, "    leaq .LC_int_fmt(%%rip), %%rdi\n");
                        fprintf(out, "    movl $0, %%eax\n");
                        fprintf(out, "    call printf\n");
                    }
                }
            } else {
                /* User function call */
                for (size_t i = 0; i < e->call.arg_count; i++) {
                    emit_expr(ctx, e->call.args[i]);
                    fprintf(out, "    pushq %%rax\n");
                }
                /* Pop arguments into ABI argument registers */
                for (int i = (int)e->call.arg_count - 1; i >= 0; i--) {
                    const char *reg = NULL;
                    if (ctx->cg->is_windows_target) {
                        if (i == 0) reg = "%rcx";
                        else if (i == 1) reg = "%rdx";
                        else if (i == 2) reg = "%r8";
                        else if (i == 3) reg = "%r9";
                    } else {
                        if (i == 0) reg = "%rdi";
                        else if (i == 1) reg = "%rsi";
                        else if (i == 2) reg = "%rdx";
                        else if (i == 3) reg = "%rcx";
                        else if (i == 4) reg = "%r8";
                        else if (i == 5) reg = "%r9";
                    }
                    if (reg) {
                        fprintf(out, "    popq %s\n", reg);
                    }
                }
                if (ctx->cg->is_windows_target) {
                    fprintf(out, "    subq $32, %%rsp\n");
                }
                fprintf(out, "    call %s\n", e->call.callee_name);
                if (ctx->cg->is_windows_target) {
                    fprintf(out, "    addq $32, %%rsp\n");
                }
            }
            break;
        }

        default:
            break;
    }
}

static void emit_stmt(FuncContext *ctx, ASTStmt *s) {
    FILE *out = ctx->cg->out;
    if (!s) return;

    switch (s->kind) {
        case STMT_BLOCK:
            for (size_t i = 0; i < s->block.count; i++) {
                emit_stmt(ctx, s->block.stmts[i]);
            }
            break;

        case STMT_EXPR:
            emit_expr(ctx, s->expr_stmt);
            break;

        case STMT_VAR_DECL: {
            int off = alloc_local_var(ctx, s->var_decl.var_name);
            if (s->var_decl.init) {
                emit_expr(ctx, s->var_decl.init);
                fprintf(out, "    movq %%rax, %d(%%rbp)  # init %s\n", off, s->var_decl.var_name);
            } else {
                fprintf(out, "    movq $0, %d(%%rbp)     # zero init %s\n", off, s->var_decl.var_name);
            }
            break;
        }

        case STMT_SHORT_VAR_DECL: {
            int off = alloc_local_var(ctx, s->short_var_decl.var_name);
            emit_expr(ctx, s->short_var_decl.init);
            fprintf(out, "    movq %%rax, %d(%%rbp)  # short decl %s\n", off, s->short_var_decl.var_name);
            break;
        }

        case STMT_ASSIGN: {
            int off = find_local_var(ctx, s->assign.var_name);
            emit_expr(ctx, s->assign.value);
            if (off != 0) {
                fprintf(out, "    movq %%rax, %d(%%rbp)  # assign to %s\n", off, s->assign.var_name);
            } else {
                fprintf(out, "    movq %%rax, %s(%%rip)\n", s->assign.var_name);
            }
            break;
        }

        case STMT_RETURN:
            if (s->ret.value) {
                emit_expr(ctx, s->ret.value);
            } else {
                fprintf(out, "    movl $0, %%eax\n");
            }
            fprintf(out, "    jmp .Lret_%s\n", ctx->current_func_name);
            break;

        case STMT_IF: {
            int lbl = ++ctx->cg->label_counter;
            emit_expr(ctx, s->if_stmt.cond);
            fprintf(out, "    cmpq $0, %%rax\n");
            fprintf(out, "    je .Lelse_%d\n", lbl);
            emit_stmt(ctx, s->if_stmt.then_branch);
            fprintf(out, "    jmp .Lend_if_%d\n", lbl);
            fprintf(out, ".Lelse_%d:\n", lbl);
            if (s->if_stmt.else_branch) {
                emit_stmt(ctx, s->if_stmt.else_branch);
            }
            fprintf(out, ".Lend_if_%d:\n", lbl);
            break;
        }

        case STMT_FOR: {
            int lbl = ++ctx->cg->label_counter;
            fprintf(out, ".Lfor_start_%d:\n", lbl);
            if (s->for_stmt.cond) {
                emit_expr(ctx, s->for_stmt.cond);
                fprintf(out, "    cmpq $0, %%rax\n");
                fprintf(out, "    je .Lfor_end_%d\n", lbl);
            }
            emit_stmt(ctx, s->for_stmt.body);
            fprintf(out, "    jmp .Lfor_start_%d\n", lbl);
            fprintf(out, ".Lfor_end_%d:\n", lbl);
            break;
        }
    }
}

static void emit_func(CodegenX86 *cg, FuncDecl *f) {
    FILE *out = cg->out;
    FuncContext ctx;
    ctx.cg = cg;
    ctx.locals = NULL;
    ctx.stack_offset = 0;
    ctx.current_label = 0;
    ctx.current_func_name = f->name;

    fprintf(out, "\n    .globl %s\n", f->name);
    fprintf(out, "%s:\n", f->name);
    fprintf(out, "    pushq %%rbp\n");
    fprintf(out, "    movq %%rsp, %%rbp\n");
    fprintf(out, "    subq $128, %%rsp  # stack frame\n");

    /* Map parameters from registers to stack */
    for (size_t i = 0; i < f->param_count; i++) {
        int off = alloc_local_var(&ctx, f->params[i].name);
        const char *reg = NULL;
        if (cg->is_windows_target) {
            if (i == 0) reg = "%rcx";
            else if (i == 1) reg = "%rdx";
            else if (i == 2) reg = "%r8";
            else if (i == 3) reg = "%r9";
        } else {
            if (i == 0) reg = "%rdi";
            else if (i == 1) reg = "%rsi";
            else if (i == 2) reg = "%rdx";
            else if (i == 3) reg = "%rcx";
            else if (i == 4) reg = "%r8";
            else if (i == 5) reg = "%r9";
        }
        if (reg) {
            fprintf(out, "    movq %s, %d(%%rbp)  # param %s\n", reg, off, f->params[i].name);
        }
    }

    emit_stmt(&ctx, f->body);

    fprintf(out, ".Lret_%s:\n", f->name);
    fprintf(out, "    movq %%rbp, %%rsp\n");
    fprintf(out, "    popq %%rbp\n");
    fprintf(out, "    ret\n");

    free_local_vars(&ctx);
}

bool codegen_x86_emit(CodegenX86 *cg) {
    FILE *out = cg->out;
    if (!cg->prog) return false;

    fprintf(out, "    .section .rdata,\"dr\"\n");
    fprintf(out, ".LC_int_fmt:\n");
    fprintf(out, "    .string \"%%lld\\n\"\n");

    fprintf(out, "\n    .text\n");

    for (size_t i = 0; i < cg->prog->decl_count; i++) {
        ASTDecl *d = cg->prog->decls[i];
        if (d->kind == DECL_FUNC) {
            emit_func(cg, &d->func);
        }
    }

    return true;
}
