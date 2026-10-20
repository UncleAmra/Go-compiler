#include "minigo/ssa.h"
#include <sstream>
#include <stack>
#include <map>
#include <set>
#include <queue>

namespace minigo {

std::string SSAPhi::to_string() const {
    std::ostringstream oss;
    oss << "    " << dest << " = phi(";
    for (size_t i = 0; i < incoming.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "[B" << incoming[i].first << ": " << incoming[i].second << "]";
    }
    oss << ")";
    return oss.str();
}

bool SSABlock::has_predecessor(int p) const {
    for (int pred : preds) {
        if (pred == p) return true;
    }
    return false;
}

bool SSABlock::has_successor(int s) const {
    for (int succ : succs) {
        if (succ == s) return true;
    }
    return false;
}

std::string SSABlock::to_string() const {
    std::ostringstream oss;
    oss << "  B" << id;
    if (!label.empty()) oss << " (" << label << ")";
    oss << ":\n    [preds:";
    for (int p : preds) oss << " B" << p;
    oss << " | succs:";
    for (int s : succs) oss << " B" << s;
    oss << "]\n";

    for (const auto &phi : phis) {
        oss << phi.to_string() << "\n";
    }
    for (const auto &ins : instrs) {
        oss << ins.to_string() << "\n";
    }
    return oss.str();
}

SSABlock *SSAFunc::get_block(int id) {
    for (auto &b : blocks) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

const SSABlock *SSAFunc::get_block(int id) const {
    for (const auto &b : blocks) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

std::string SSAFunc::to_string() const {
    std::ostringstream oss;
    oss << "function " << name << "(";
    for (size_t i = 0; i < ssa_params.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << ssa_params[i];
    }
    oss << "):\n";
    for (const auto &b : blocks) {
        oss << b.to_string();
    }
    return oss.str();
}

std::string SSAProgram::to_string() const {
    std::ostringstream oss;
    for (const auto &fn : funcs) {
        oss << fn.to_string() << "\n";
    }
    return oss.str();
}

void SSAProgram::dump(std::ostream &os) const {
    os << to_string();
}

namespace {

struct SSAGenContext {
    const CFGFunc &cfg;
    SSAFunc ssa_func;

    std::map<std::string, std::stack<std::string>> var_stacks;
    std::map<std::string, int> var_counters;

    std::string gen_version(const std::string &var) {
        int c = var_counters[var]++;
        std::string ver = var + "_" + std::to_string(c);
        var_stacks[var].push(ver);
        return ver;
    }

    std::string curr_version(const std::string &var) {
        auto &st = var_stacks[var];
        if (st.empty()) {
            return var + "_0";
        }
        return st.top();
    }

    SSAGenContext(const CFGFunc &c) : cfg(c) {}
};

void rename_block_dfs(SSAGenContext &ctx, int block_id) {
    const BasicBlock *cblock = ctx.cfg.get_block(block_id);
    SSABlock *sblock = ctx.ssa_func.get_block(block_id);
    if (!cblock || !sblock) return;

    std::vector<std::string> pushed_vars;

    /* 1. Generate new versions for phi destinations */
    for (auto &phi : sblock->phis) {
        std::string new_v = ctx.gen_version(phi.original_var);
        phi.dest = new_v;
        pushed_vars.push_back(phi.original_var);
    }

    /* 2. Rename operands and destinations in instructions */
    for (auto &ins : sblock->instrs) {
        /* Operands: use current versions */
        if (ins.src1.is_variable_or_temp()) {
            ins.src1.name = ctx.curr_version(ins.src1.name);
        }
        if (ins.src2.is_variable_or_temp()) {
            ins.src2.name = ctx.curr_version(ins.src2.name);
        }
        for (auto &arg : ins.args) {
            if (arg.is_variable_or_temp()) {
                arg.name = ctx.curr_version(arg.name);
            }
        }

        /* Destination: define a new version (unless PARAM or JUMP/RET/PRINT) */
        if (ins.dest.is_variable_or_temp() && ins.op != TACOp::PARAM) {
            std::string orig_name = ins.dest.name;
            std::string new_v = ctx.gen_version(orig_name);
            ins.dest.name = new_v;
            pushed_vars.push_back(orig_name);
        }
    }

    /* 3. Update incoming arguments for phi-nodes in successor blocks */
    for (int succ_id : sblock->succs) {
        SSABlock *succ_block = ctx.ssa_func.get_block(succ_id);
        if (!succ_block) continue;
        for (auto &phi : succ_block->phis) {
            std::string cur_val = ctx.curr_version(phi.original_var);
            phi.incoming.push_back({block_id, cur_val});
        }
    }

    /* 4. Recursively visit children in the dominator tree */
    for (int child_id : cblock->dom_children) {
        rename_block_dfs(ctx, child_id);
    }

    /* 5. Pop pushed versions on exit */
    for (const auto &v : pushed_vars) {
        ctx.var_stacks[v].pop();
    }
}

} // anonymous namespace

SSAProgram convert_cfg_to_ssa(const CFGProgram &cfg_prog) {
    SSAProgram ssa_prog;

    for (const auto &cfunc : cfg_prog.funcs) {
        if (cfunc.blocks.empty()) {
            SSAFunc fn;
            fn.name = cfunc.name;
            fn.params = cfunc.params;
            ssa_prog.funcs.push_back(fn);
            continue;
        }

        SSAGenContext ctx(cfunc);
        ctx.ssa_func.name = cfunc.name;
        ctx.ssa_func.params = cfunc.params;
        ctx.ssa_func.entry_block_id = cfunc.entry_block_id;

        /* Initialize SSA blocks mirroring CFG blocks */
        for (const auto &cb : cfunc.blocks) {
            SSABlock sb;
            sb.id = cb.id;
            sb.label = cb.label;
            sb.preds = cb.preds;
            sb.succs = cb.succs;
            sb.instrs = cb.instrs;
            ctx.ssa_func.blocks.push_back(sb);
        }

        /* Step 1: Collect all variable and temporary names and their definition blocks */
        std::set<std::string> all_vars;
        std::map<std::string, std::set<int>> defs;

        /* Parameters are defined in entry block 0 */
        for (const auto &p : cfunc.params) {
            all_vars.insert(p);
            defs[p].insert(0);
        }

        for (const auto &cb : cfunc.blocks) {
            for (const auto &ins : cb.instrs) {
                if (ins.dest.is_variable_or_temp()) {
                    all_vars.insert(ins.dest.name);
                    defs[ins.dest.name].insert(cb.id);
                }
                if (ins.src1.is_variable_or_temp()) all_vars.insert(ins.src1.name);
                if (ins.src2.is_variable_or_temp()) all_vars.insert(ins.src2.name);
                for (const auto &arg : ins.args) {
                    if (arg.is_variable_or_temp()) all_vars.insert(arg.name);
                }
            }
        }

        /* Step 2: Insert Phi-nodes at Dominance Frontiers (Semi-Pruned SSA) */
        for (const auto &v : all_vars) {
            if (defs[v].size() <= 1) continue;

            std::set<int> has_phi;
            std::queue<int> worklist;
            for (int d : defs[v]) {
                worklist.push(d);
            }

            while (!worklist.empty()) {
                int x = worklist.front();
                worklist.pop();

                const BasicBlock *xb = cfunc.get_block(x);
                if (!xb) continue;

                for (int y : xb->dom_frontier) {
                    if (has_phi.find(y) == has_phi.end()) {
                        SSABlock *yb = ctx.ssa_func.get_block(y);
                        if (yb && yb->preds.size() >= 2) {
                            SSAPhi phi;
                            phi.original_var = v;
                            yb->phis.push_back(phi);
                            has_phi.insert(y);

                            if (defs[v].find(y) == defs[v].end()) {
                                defs[v].insert(y);
                                worklist.push(y);
                            }
                        }
                    }
                }
            }
        }

        /* Step 3: Handle Parameters at function entry */
        for (const auto &p : cfunc.params) {
            std::string ver = ctx.gen_version(p);
            ctx.ssa_func.ssa_params.push_back(ver);
        }

        /* Step 4: Renaming through Dominator Tree DFS */
        rename_block_dfs(ctx, 0);

        ssa_prog.funcs.push_back(ctx.ssa_func);
    }

    return ssa_prog;
}

} // namespace minigo
