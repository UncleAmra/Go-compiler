#include "minigo/opt.h"
#include <map>
#include <set>
#include <queue>
#include <vector>

namespace minigo {

bool pass_constant_folding(SSAFunc &func, OptimizationStats &stats) {
    bool changed = false;
    std::map<std::string, int64_t> constants;

    /* 1. Fold phi nodes if all incoming values are the same constant */
    for (auto &blk : func.blocks) {
        for (auto it = blk.phis.begin(); it != blk.phis.end();) {
            if (it->incoming.empty()) {
                it++;
                continue;
            }

            bool all_const = true;
            int64_t first_val = 0;
            bool first = true;

            for (const auto &inc : it->incoming) {
                if (constants.count(inc.second)) {
                    int64_t v = constants[inc.second];
                    if (first) {
                        first_val = v;
                        first = false;
                    } else if (v != first_val) {
                        all_const = false;
                        break;
                    }
                } else {
                    all_const = false;
                    break;
                }
            }

            if (all_const && !first) {
                constants[it->dest] = first_val;
                TACInstr copy_ins;
                copy_ins.op = TACOp::ASSIGN;
                copy_ins.dest = TACOperand::make_var(it->dest);
                copy_ins.src1 = TACOperand::make_int(first_val);
                blk.instrs.insert(blk.instrs.begin(), copy_ins);

                it = blk.phis.erase(it);
                stats.constants_folded++;
                stats.phi_nodes_eliminated++;
                changed = true;
            } else {
                it++;
            }
        }
    }

    /* 2. Propagate and fold instructions */
    for (auto &blk : func.blocks) {
        std::vector<TACInstr> new_instrs;

        for (auto &ins : blk.instrs) {
            /* Replace src1 if constant */
            if (ins.src1.is_variable_or_temp() && constants.count(ins.src1.name)) {
                ins.src1 = TACOperand::make_int(constants[ins.src1.name]);
                changed = true;
            }
            /* Replace src2 if constant */
            if (ins.src2.is_variable_or_temp() && constants.count(ins.src2.name)) {
                ins.src2 = TACOperand::make_int(constants[ins.src2.name]);
                changed = true;
            }
            /* Replace call args if constant */
            for (auto &arg : ins.args) {
                if (arg.is_variable_or_temp() && constants.count(arg.name)) {
                    arg = TACOperand::make_int(constants[arg.name]);
                    changed = true;
                }
            }

            /* Record copy constants */
            if (ins.op == TACOp::ASSIGN && ins.dest.is_variable_or_temp() && ins.src1.is_const()) {
                constants[ins.dest.name] = ins.src1.int_val;
            }

            /* Binary operations constant folding */
            if (ins.src1.is_const() && ins.src2.is_const() && ins.dest.is_variable_or_temp()) {
                int64_t a = ins.src1.int_val;
                int64_t b = ins.src2.int_val;
                int64_t res = 0;
                bool folded = true;

                switch (ins.op) {
                    case TACOp::ADD: res = a + b; break;
                    case TACOp::SUB: res = a - b; break;
                    case TACOp::MUL: res = a * b; break;
                    case TACOp::DIV: res = (b != 0) ? (a / b) : 0; break;
                    case TACOp::MOD: res = (b != 0) ? (a % b) : 0; break;
                    case TACOp::EQ:  res = (a == b) ? 1 : 0; break;
                    case TACOp::NE:  res = (a != b) ? 1 : 0; break;
                    case TACOp::LT:  res = (a < b)  ? 1 : 0; break;
                    case TACOp::LE:  res = (a <= b) ? 1 : 0; break;
                    case TACOp::GT:  res = (a > b)  ? 1 : 0; break;
                    case TACOp::GE:  res = (a >= b) ? 1 : 0; break;
                    case TACOp::AND: res = (a != 0 && b != 0) ? 1 : 0; break;
                    case TACOp::OR:  res = (a != 0 || b != 0) ? 1 : 0; break;
                    default:         folded = false; break;
                }

                if (folded) {
                    ins.op = TACOp::ASSIGN;
                    ins.src1 = TACOperand::make_int(res);
                    ins.src2 = TACOperand::make_none();
                    constants[ins.dest.name] = res;
                    stats.constants_folded++;
                    changed = true;
                }
            }

            /* Unary operations constant folding */
            if (ins.src1.is_const() && ins.dest.is_variable_or_temp()) {
                int64_t a = ins.src1.int_val;
                if (ins.op == TACOp::NEG) {
                    ins.op = TACOp::ASSIGN;
                    ins.src1 = TACOperand::make_int(-a);
                    constants[ins.dest.name] = -a;
                    stats.constants_folded++;
                    changed = true;
                } else if (ins.op == TACOp::NOT) {
                    int64_t res = (a == 0) ? 1 : 0;
                    ins.op = TACOp::ASSIGN;
                    ins.src1 = TACOperand::make_int(res);
                    constants[ins.dest.name] = res;
                    stats.constants_folded++;
                    changed = true;
                }
            }

            /* Branch folding */
            if (ins.op == TACOp::JUMP_IF_ZERO && ins.src1.is_const()) {
                if (ins.src1.int_val == 0) {
                    /* Condition is 0 (false) -> branch is always taken */
                    ins.op = TACOp::JUMP;
                    ins.src1 = TACOperand::make_none();
                    stats.branches_folded++;
                    changed = true;
                    new_instrs.push_back(ins);
                    break; /* Unconditional jump ends block */
                } else {
                    /* Condition is non-zero -> branch is never taken */
                    stats.branches_folded++;
                    changed = true;
                    continue; /* Drop this branch */
                }
            }

            new_instrs.push_back(ins);
        }

        blk.instrs = new_instrs;
    }

    return changed;
}

bool pass_dead_code_elimination(SSAFunc &func, OptimizationStats &stats) {
    bool changed = false;

    /* Step 1: Collect defining instructions for each SSA variable */
    std::map<std::string, TACInstr*> def_instrs;
    std::map<std::string, SSAPhi*> def_phis;

    for (auto &blk : func.blocks) {
        for (auto &phi : blk.phis) {
            def_phis[phi.dest] = &phi;
        }
        for (auto &ins : blk.instrs) {
            if (ins.dest.is_variable_or_temp()) {
                def_instrs[ins.dest.name] = &ins;
            }
        }
    }

    /* Step 2: Mark root live variables */
    std::set<std::string> live_vars;
    std::queue<std::string> worklist;

    auto mark_live = [&](const TACOperand &op) {
        if (op.is_variable_or_temp()) {
            if (live_vars.insert(op.name).second) {
                worklist.push(op.name);
            }
        }
    };

    for (const auto &blk : func.blocks) {
        for (const auto &ins : blk.instrs) {
            /* Observable side effects: PRINT, RET, CALL, conditional jumps */
            if (ins.op == TACOp::PRINT ||
                ins.op == TACOp::RET ||
                ins.op == TACOp::CALL ||
                ins.op == TACOp::JUMP_IF_ZERO ||
                ins.op == TACOp::JUMP_IF_NOT_ZERO) {

                mark_live(ins.src1);
                mark_live(ins.src2);
                for (const auto &arg : ins.args) {
                    mark_live(arg);
                }
            }
        }
    }

    /* Step 3: Propagate liveness transitively */
    while (!worklist.empty()) {
        std::string var = worklist.front();
        worklist.pop();

        if (def_instrs.count(var)) {
            TACInstr *ins = def_instrs[var];
            mark_live(ins->src1);
            mark_live(ins->src2);
            for (const auto &arg : ins->args) {
                mark_live(arg);
            }
        } else if (def_phis.count(var)) {
            SSAPhi *phi = def_phis[var];
            for (const auto &inc : phi->incoming) {
                if (live_vars.insert(inc.second).second) {
                    worklist.push(inc.second);
                }
            }
        }
    }

    /* Step 4: Remove dead instructions and phi nodes */
    for (auto &blk : func.blocks) {
        /* Phis */
        for (auto it = blk.phis.begin(); it != blk.phis.end();) {
            if (!live_vars.count(it->dest)) {
                it = blk.phis.erase(it);
                stats.instructions_eliminated++;
                stats.phi_nodes_eliminated++;
                changed = true;
            } else {
                it++;
            }
        }

        /* Instructions */
        std::vector<TACInstr> active_instrs;
        for (const auto &ins : blk.instrs) {
            if (ins.dest.is_variable_or_temp() &&
                ins.op != TACOp::PRINT &&
                ins.op != TACOp::RET &&
                ins.op != TACOp::CALL &&
                ins.op != TACOp::PARAM) {

                if (live_vars.count(ins.dest.name)) {
                    active_instrs.push_back(ins);
                } else {
                    stats.instructions_eliminated++;
                    changed = true;
                }
            } else {
                active_instrs.push_back(ins);
            }
        }
        blk.instrs = active_instrs;
    }

    return changed;
}

void optimize_function(SSAFunc &func, OptimizationStats &stats) {
    bool progress = true;
    int iterations = 0;
    while (progress && iterations < 16) {
        progress = false;
        if (pass_constant_folding(func, stats)) progress = true;
        if (pass_dead_code_elimination(func, stats)) progress = true;
        iterations++;
    }
}

void optimize_program(SSAProgram &prog, bool verbose, std::ostream &log_out) {
    if (verbose) {
        log_out << "\n============================================================\n";
        log_out << "              SSA IR BEFORE OPTIMIZATION\n";
        log_out << "============================================================\n";
        prog.dump(log_out);
    }

    OptimizationStats stats;
    for (auto &fn : prog.funcs) {
        optimize_function(fn, stats);
    }

    if (verbose) {
        log_out << "============================================================\n";
        log_out << "              SSA IR AFTER OPTIMIZATION\n";
        log_out << "============================================================\n";
        prog.dump(log_out);
        log_out << "============================================================\n";
        log_out << "Optimization Summary:\n";
        log_out << "  - Constants folded:       " << stats.constants_folded << "\n";
        log_out << "  - Dead instructions removed: " << stats.instructions_eliminated << "\n";
        log_out << "  - Conditional branches folded: " << stats.branches_folded << "\n";
        log_out << "  - Phi-nodes eliminated:   " << stats.phi_nodes_eliminated << "\n";
        log_out << "============================================================\n\n";
    }
}

} // namespace minigo
