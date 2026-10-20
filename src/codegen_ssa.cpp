#include "minigo/codegen_ssa.h"
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <vector>

namespace minigo {

namespace {

struct CodegenCtx {
    const SSAFunc &func;
    const CodegenOptions &opts;
    std::ostream &out;
    std::map<std::string, int> var_offsets;
    int stack_frame_size = 0;

    CodegenCtx(const SSAFunc &f, const CodegenOptions &o, std::ostream &os)
        : func(f), opts(o), out(os) {}

    int get_offset(const std::string &var) {
        auto it = var_offsets.find(var);
        if (it != var_offsets.end()) return it->second;
        return 0;
    }

    void load_operand(const TACOperand &op, const char *reg) {
        if (op.is_const()) {
            out << "    movq $" << op.int_val << ", " << reg << "\n";
        } else if (op.is_variable_or_temp()) {
            int off = get_offset(op.name);
            out << "    movq " << off << "(%rbp), " << reg
                << (opts.verbose_comments ? ("  # load " + op.name) : "") << "\n";
        } else {
            out << "    movq $0, " << reg << "\n";
        }
    }

    void store_dest(const TACOperand &dest, const char *reg) {
        if (dest.is_variable_or_temp()) {
            int off = get_offset(dest.name);
            out << "    movq " << reg << ", " << off << "(%rbp)"
                << (opts.verbose_comments ? ("  # store " + dest.name) : "") << "\n";
        }
    }
};

/* Out-of-SSA deconstruction: inserts copy instructions into predecessor blocks
 * before jump/branch to eliminate phi nodes */
SSAFunc eliminate_phi_nodes(const SSAFunc &src_func) {
    SSAFunc func = src_func;

    /* For each block with phis, collect copies needed per predecessor */
    std::map<int, std::vector<std::pair<std::string, std::string>>> pred_copies;

    for (const auto &blk : func.blocks) {
        for (const auto &phi : blk.phis) {
            for (const auto &inc : phi.incoming) {
                int pred_id = inc.first;
                pred_copies[pred_id].push_back({phi.dest, inc.second});
            }
        }
    }

    /* Insert copies in each predecessor block before its terminator */
    for (auto &blk : func.blocks) {
        blk.phis.clear(); /* Phis eliminated */

        auto it = pred_copies.find(blk.id);
        if (it == pred_copies.end() || it->second.empty()) continue;

        const auto &copies = it->second;
        std::vector<TACInstr> copy_instrs;
        for (const auto &cp : copies) {
            TACInstr c_ins;
            c_ins.op = TACOp::ASSIGN;
            c_ins.dest = TACOperand::make_var(cp.first);
            /* Check if source is integer literal or variable */
            bool is_number = true;
            if (cp.second.empty()) is_number = false;
            for (size_t i = 0; i < cp.second.size(); ++i) {
                if (i == 0 && (cp.second[i] == '-' || cp.second[i] == '+')) continue;
                if (!isdigit(cp.second[i])) { is_number = false; break; }
            }
            if (is_number) {
                c_ins.src1 = TACOperand::make_int(std::stoll(cp.second));
            } else {
                c_ins.src1 = TACOperand::make_var(cp.second);
            }
            copy_instrs.push_back(c_ins);
        }

        /* Find insertion position: right before terminal jump/branch */
        auto insert_pos = blk.instrs.end();
        while (insert_pos != blk.instrs.begin()) {
            auto prev = insert_pos - 1;
            if (prev->op == TACOp::JUMP ||
                prev->op == TACOp::JUMP_IF_ZERO ||
                prev->op == TACOp::JUMP_IF_NOT_ZERO ||
                prev->op == TACOp::RET) {
                insert_pos = prev;
            } else {
                break;
            }
        }

        blk.instrs.insert(insert_pos, copy_instrs.begin(), copy_instrs.end());
    }

    return func;
}

void emit_func_x86(const SSAFunc &raw_func,
                   std::ostream &out,
                   const CodegenOptions &opts) {
    /* 1. Eliminate phi nodes */
    SSAFunc func = eliminate_phi_nodes(raw_func);

    CodegenCtx ctx(func, opts, out);

    /* 2. Assign stack slots to all SSA variables */
    std::set<std::string> all_vars;
    for (const auto &p : func.ssa_params) {
        all_vars.insert(p);
    }
    for (const auto &b : func.blocks) {
        for (const auto &ins : b.instrs) {
            if (ins.dest.is_variable_or_temp()) all_vars.insert(ins.dest.name);
            if (ins.src1.is_variable_or_temp()) all_vars.insert(ins.src1.name);
            if (ins.src2.is_variable_or_temp()) all_vars.insert(ins.src2.name);
            for (const auto &arg : ins.args) {
                if (arg.is_variable_or_temp()) all_vars.insert(arg.name);
            }
        }
    }

    int slot = 0;
    for (const auto &v : all_vars) {
        slot++;
        ctx.var_offsets[v] = -8 * slot;
    }

    /* Stack frame alignment to 16 bytes */
    int raw_size = slot * 8;
    if (opts.is_windows_target) {
        raw_size += 32; /* 32 bytes shadow space */
    }
    ctx.stack_frame_size = ((raw_size + 15) / 16) * 16;
    if (ctx.stack_frame_size < 32 && opts.is_windows_target) {
        ctx.stack_frame_size = 32;
    }

    /* 3. Emit Prologue */
    out << "\n    .globl " << func.name << "\n";
    out << func.name << ":\n";
    out << "    pushq %rbp\n";
    out << "    movq %rsp, %rbp\n";
    if (ctx.stack_frame_size > 0) {
        out << "    subq $" << ctx.stack_frame_size << ", %rsp";
        if (opts.verbose_comments) out << "  # allocate stack frame";
        out << "\n";
    }

    /* Save incoming function parameters to their SSA slots */
    for (size_t i = 0; i < func.ssa_params.size(); ++i) {
        const std::string &p_name = func.ssa_params[i];
        int off = ctx.get_offset(p_name);
        const char *reg = nullptr;
        if (opts.is_windows_target) {
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
            out << "    movq " << reg << ", " << off << "(%rbp)"
                << (opts.verbose_comments ? ("  # param " + p_name) : "") << "\n";
        }
    }

    /* 4. Emit Basic Blocks */
    for (const auto &blk : func.blocks) {
        out << ".L_" << func.name << "_B" << blk.id << ":\n";

        for (const auto &ins : blk.instrs) {
            switch (ins.op) {
                case TACOp::LABEL:
                    /* Handled as block labels */
                    break;

                case TACOp::PARAM:
                    /* Already moved in prologue */
                    break;

                case TACOp::ASSIGN:
                    ctx.load_operand(ins.src1, "%rax");
                    ctx.store_dest(ins.dest, "%rax");
                    break;

                case TACOp::ADD:
                    ctx.load_operand(ins.src1, "%rax");
                    if (ins.src2.is_const()) {
                        out << "    addq $" << ins.src2.int_val << ", %rax\n";
                    } else {
                        ctx.load_operand(ins.src2, "%rcx");
                        out << "    addq %rcx, %rax\n";
                    }
                    ctx.store_dest(ins.dest, "%rax");
                    break;

                case TACOp::SUB:
                    ctx.load_operand(ins.src1, "%rax");
                    if (ins.src2.is_const()) {
                        out << "    subq $" << ins.src2.int_val << ", %rax\n";
                    } else {
                        ctx.load_operand(ins.src2, "%rcx");
                        out << "    subq %rcx, %rax\n";
                    }
                    ctx.store_dest(ins.dest, "%rax");
                    break;

                case TACOp::MUL:
                    ctx.load_operand(ins.src1, "%rax");
                    if (ins.src2.is_const()) {
                        out << "    imulq $" << ins.src2.int_val << ", %rax\n";
                    } else {
                        ctx.load_operand(ins.src2, "%rcx");
                        out << "    imulq %rcx, %rax\n";
                    }
                    ctx.store_dest(ins.dest, "%rax");
                    break;

                case TACOp::DIV:
                case TACOp::MOD:
                    ctx.load_operand(ins.src1, "%rax");
                    out << "    cqto\n";
                    ctx.load_operand(ins.src2, "%rcx");
                    out << "    idivq %rcx\n";
                    if (ins.op == TACOp::DIV) {
                        ctx.store_dest(ins.dest, "%rax");
                    } else {
                        ctx.store_dest(ins.dest, "%rdx");
                    }
                    break;

                case TACOp::NEG:
                    ctx.load_operand(ins.src1, "%rax");
                    out << "    negq %rax\n";
                    ctx.store_dest(ins.dest, "%rax");
                    break;

                case TACOp::NOT:
                    ctx.load_operand(ins.src1, "%rax");
                    out << "    cmpq $0, %rax\n";
                    out << "    sete %al\n";
                    out << "    movzbq %al, %rax\n";
                    ctx.store_dest(ins.dest, "%rax");
                    break;

                case TACOp::EQ:
                case TACOp::NE:
                case TACOp::LT:
                case TACOp::LE:
                case TACOp::GT:
                case TACOp::GE: {
                    ctx.load_operand(ins.src1, "%rax");
                    if (ins.src2.is_const()) {
                        out << "    cmpq $" << ins.src2.int_val << ", %rax\n";
                    } else {
                        ctx.load_operand(ins.src2, "%rcx");
                        out << "    cmpq %rcx, %rax\n";
                    }

                    const char *setcc = "sete";
                    switch (ins.op) {
                        case TACOp::EQ: setcc = "sete";  break;
                        case TACOp::NE: setcc = "setne"; break;
                        case TACOp::LT: setcc = "setl";  break;
                        case TACOp::LE: setcc = "setle"; break;
                        case TACOp::GT: setcc = "setg";  break;
                        case TACOp::GE: setcc = "setge"; break;
                        default: break;
                    }
                    out << "    " << setcc << " %al\n";
                    out << "    movzbq %al, %rax\n";
                    ctx.store_dest(ins.dest, "%rax");
                    break;
                }

                case TACOp::AND:
                    ctx.load_operand(ins.src1, "%rax");
                    ctx.load_operand(ins.src2, "%rcx");
                    out << "    andq %rcx, %rax\n";
                    ctx.store_dest(ins.dest, "%rax");
                    break;

                case TACOp::OR:
                    ctx.load_operand(ins.src1, "%rax");
                    ctx.load_operand(ins.src2, "%rcx");
                    out << "    orq %rcx, %rax\n";
                    ctx.store_dest(ins.dest, "%rax");
                    break;

                case TACOp::JUMP: {
                    std::string target = ins.dest.to_string();
                    out << "    jmp .L_" << func.name << "_" << target << "\n";
                    break;
                }

                case TACOp::JUMP_IF_ZERO: {
                    ctx.load_operand(ins.src1, "%rax");
                    out << "    cmpq $0, %rax\n";
                    std::string target = ins.dest.to_string();
                    out << "    je .L_" << func.name << "_" << target << "\n";
                    break;
                }

                case TACOp::JUMP_IF_NOT_ZERO: {
                    ctx.load_operand(ins.src1, "%rax");
                    out << "    cmpq $0, %rax\n";
                    std::string target = ins.dest.to_string();
                    out << "    jne .L_" << func.name << "_" << target << "\n";
                    break;
                }

                case TACOp::PRINT: {
                    ctx.load_operand(ins.src1, "%rax");
                    if (opts.is_windows_target) {
                        out << "    movq %rax, %rdx\n";
                        out << "    leaq .LC_int_fmt(%rip), %rcx\n";
                        out << "    subq $32, %rsp\n";
                        out << "    call printf\n";
                        out << "    addq $32, %rsp\n";
                    } else {
                        out << "    movq %rax, %rsi\n";
                        out << "    leaq .LC_int_fmt(%rip), %rdi\n";
                        out << "    movl $0, %eax\n";
                        out << "    call printf\n";
                    }
                    break;
                }

                case TACOp::CALL: {
                    /* Pass arguments in registers */
                    for (size_t i = 0; i < ins.args.size(); ++i) {
                        const char *reg = nullptr;
                        if (opts.is_windows_target) {
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
                            ctx.load_operand(ins.args[i], reg);
                        }
                    }

                    if (opts.is_windows_target) {
                        out << "    subq $32, %rsp\n";
                    }
                    out << "    call " << ins.callee << "\n";
                    if (opts.is_windows_target) {
                        out << "    addq $32, %rsp\n";
                    }

                    if (ins.dest.is_variable_or_temp()) {
                        ctx.store_dest(ins.dest, "%rax");
                    }
                    break;
                }

                case TACOp::RET:
                    if (!ins.src1.is_none()) {
                        ctx.load_operand(ins.src1, "%rax");
                    } else {
                        out << "    movq $0, %rax\n";
                    }
                    out << "    jmp .Lret_" << func.name << "\n";
                    break;
            }
        }
    }

    /* 5. Emit Epilogue */
    out << ".Lret_" << func.name << ":\n";
    out << "    movq %rbp, %rsp\n";
    out << "    popq %rbp\n";
    out << "    ret\n";
}

} // anonymous namespace

bool generate_x86_assembly(const SSAProgram &ssa_prog,
                           std::ostream &out,
                           const CodegenOptions &opts) {
    /* Emit read-only data */
    if (opts.is_windows_target) {
        out << "    .section .rdata,\"dr\"\n";
    } else {
        out << "    .section .rodata\n";
    }
    out << ".LC_int_fmt:\n";
    out << "    .string \"%lld\\n\"\n";

    out << "\n    .text\n";

    for (const auto &fn : ssa_prog.funcs) {
        emit_func_x86(fn, out, opts);
    }

    return true;
}

bool generate_x86_assembly_file(const SSAProgram &ssa_prog,
                                const std::string &filepath,
                                const CodegenOptions &opts) {
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;
    return generate_x86_assembly(ssa_prog, ofs, opts);
}

} // namespace minigo
