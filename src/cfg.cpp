#include "minigo/cfg.h"
#include <sstream>
#include <queue>
#include <algorithm>

namespace minigo {

bool BasicBlock::has_successor(int succ_id) const {
    for (int s : succs) {
        if (s == succ_id) return true;
    }
    return false;
}

bool BasicBlock::has_predecessor(int pred_id) const {
    for (int p : preds) {
        if (p == pred_id) return true;
    }
    return false;
}

std::string BasicBlock::to_string() const {
    std::ostringstream oss;
    oss << "  B" << id;
    if (!label.empty()) {
        oss << " (" << label << ")";
    }
    oss << ":\n    [preds:";
    for (int p : preds) oss << " B" << p;
    oss << " | succs:";
    for (int s : succs) oss << " B" << s;
    oss << "]\n";

    for (const auto &instr : instrs) {
        oss << instr.to_string() << "\n";
    }
    return oss.str();
}

BasicBlock *CFGFunc::get_block(int id) {
    for (auto &b : blocks) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

const BasicBlock *CFGFunc::get_block(int id) const {
    for (const auto &b : blocks) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

std::string CFGFunc::to_string() const {
    std::ostringstream oss;
    oss << "CFG for func " << name << "():\n";
    for (const auto &b : blocks) {
        oss << b.to_string();
    }
    return oss.str();
}

std::string CFGProgram::to_string() const {
    std::ostringstream oss;
    for (const auto &fn : funcs) {
        oss << fn.to_string() << "\n";
    }
    return oss.str();
}

void CFGProgram::dump(std::ostream &os) const {
    os << to_string();
}

CFGProgram build_cfg(const TACProgram &tac_prog) {
    CFGProgram cfg_prog;

    for (const auto &tfunc : tac_prog.funcs) {
        CFGFunc cfunc;
        cfunc.name = tfunc.name;
        cfunc.params = tfunc.params;

        if (tfunc.instrs.empty()) {
            cfg_prog.funcs.push_back(cfunc);
            continue;
        }

        const auto &instrs = tfunc.instrs;
        size_t n = instrs.size();

        /* Step 1: Identify leaders */
        std::vector<bool> is_leader(n, false);
        is_leader[0] = true;

        for (size_t i = 0; i < n; ++i) {
            const auto &ins = instrs[i];
            if (ins.op == TACOp::LABEL) {
                is_leader[i] = true;
            } else if (ins.op == TACOp::JUMP ||
                       ins.op == TACOp::JUMP_IF_ZERO ||
                       ins.op == TACOp::JUMP_IF_NOT_ZERO ||
                       ins.op == TACOp::RET) {
                if (i + 1 < n) {
                    is_leader[i + 1] = true;
                }
            }
        }

        /* Step 2: Form basic blocks */
        std::vector<BasicBlock> raw_blocks;
        std::map<std::string, int> label_to_block_id;

        int current_id = 0;
        for (size_t i = 0; i < n;) {
            BasicBlock block;
            block.id = current_id++;

            size_t j = i + 1;
            while (j < n && !is_leader[j]) {
                j++;
            }

            for (size_t k = i; k < j; ++k) {
                if (instrs[k].op == TACOp::LABEL) {
                    block.label = instrs[k].dest.to_string();
                    label_to_block_id[block.label] = block.id;
                } else {
                    block.instrs.push_back(instrs[k]);
                }
            }

            if (block.label.empty()) {
                block.label = "B" + std::to_string(block.id);
                label_to_block_id[block.label] = block.id;
            }

            raw_blocks.push_back(block);
            i = j;
        }

        /* Step 3: Insert explicit fallthrough jumps between blocks */
        for (size_t b = 0; b < raw_blocks.size(); ++b) {
            auto &blk = raw_blocks[b];
            std::string next_label = (b + 1 < raw_blocks.size()) ? raw_blocks[b + 1].label : "";

            if (blk.instrs.empty()) {
                if (!next_label.empty()) {
                    TACInstr jmp_ins;
                    jmp_ins.op = TACOp::JUMP;
                    jmp_ins.dest = TACOperand::make_label(next_label);
                    blk.instrs.push_back(jmp_ins);
                }
                continue;
            }

            const auto &last_ins = blk.instrs.back();
            if (last_ins.op == TACOp::JUMP_IF_ZERO || last_ins.op == TACOp::JUMP_IF_NOT_ZERO) {
                /* If conditional jump, fallthrough to next block */
                if (!next_label.empty()) {
                    TACInstr jmp_ins;
                    jmp_ins.op = TACOp::JUMP;
                    jmp_ins.dest = TACOperand::make_label(next_label);
                    blk.instrs.push_back(jmp_ins);
                }
            } else if (last_ins.op != TACOp::JUMP && last_ins.op != TACOp::RET) {
                /* Regular instructions falling through to next block */
                if (!next_label.empty()) {
                    TACInstr jmp_ins;
                    jmp_ins.op = TACOp::JUMP;
                    jmp_ins.dest = TACOperand::make_label(next_label);
                    blk.instrs.push_back(jmp_ins);
                }
            }
        }

        /* Step 4: Construct edges */
        for (auto &blk : raw_blocks) {
            for (const auto &ins : blk.instrs) {
                if (ins.op == TACOp::JUMP ||
                    ins.op == TACOp::JUMP_IF_ZERO ||
                    ins.op == TACOp::JUMP_IF_NOT_ZERO) {
                    std::string target_label = ins.dest.to_string();
                    auto it = label_to_block_id.find(target_label);
                    if (it != label_to_block_id.end()) {
                        int succ_id = it->second;
                        if (!blk.has_successor(succ_id)) {
                            blk.succs.push_back(succ_id);
                        }
                    }
                }
            }
        }

        for (auto &blk : raw_blocks) {
            for (int s : blk.succs) {
                for (auto &other : raw_blocks) {
                    if (other.id == s && !other.has_predecessor(blk.id)) {
                        other.preds.push_back(blk.id);
                    }
                }
            }
        }

        /* Step 5: Filter unreachable blocks from B0 */
        std::set<int> reachable;
        std::queue<int> q;
        if (!raw_blocks.empty()) {
            q.push(0);
            reachable.insert(0);
        }
        while (!q.empty()) {
            int curr = q.front();
            q.pop();
            for (auto &b : raw_blocks) {
                if (b.id == curr) {
                    for (int s : b.succs) {
                        if (reachable.find(s) == reachable.end()) {
                            reachable.insert(s);
                            q.push(s);
                        }
                    }
                    break;
                }
            }
        }

        /* Build clean reachable blocks with reindexed IDs */
        std::map<int, int> old_to_new;
        int new_id = 0;
        for (const auto &b : raw_blocks) {
            if (reachable.count(b.id)) {
                old_to_new[b.id] = new_id++;
            }
        }

        for (auto &b : raw_blocks) {
            if (!reachable.count(b.id)) continue;
            BasicBlock cleaned;
            cleaned.id = old_to_new[b.id];
            cleaned.label = "B" + std::to_string(cleaned.id);

            for (int p : b.preds) {
                if (reachable.count(p)) cleaned.preds.push_back(old_to_new[p]);
            }
            for (int s : b.succs) {
                if (reachable.count(s)) cleaned.succs.push_back(old_to_new[s]);
            }

            for (auto ins : b.instrs) {
                if (ins.op == TACOp::JUMP ||
                    ins.op == TACOp::JUMP_IF_ZERO ||
                    ins.op == TACOp::JUMP_IF_NOT_ZERO) {
                    auto it = label_to_block_id.find(ins.dest.to_string());
                    if (it != label_to_block_id.end() && reachable.count(it->second)) {
                        ins.dest = TACOperand::make_label("B" + std::to_string(old_to_new[it->second]));
                    }
                }
                cleaned.instrs.push_back(ins);
            }

            cfunc.blocks.push_back(cleaned);
        }

        compute_dominators_and_frontiers(cfunc);
        cfg_prog.funcs.push_back(cfunc);
    }

    return cfg_prog;
}

void compute_dominators_and_frontiers(CFGFunc &cfg_func) {
    if (cfg_func.blocks.empty()) return;

    size_t n = cfg_func.blocks.size();
    std::set<int> all_blocks;
    for (size_t i = 0; i < n; ++i) {
        all_blocks.insert(cfg_func.blocks[i].id);
    }

    /* Step 1: Compute dominators */
    cfg_func.blocks[0].dominators.clear();
    cfg_func.blocks[0].dominators.insert(cfg_func.blocks[0].id);

    for (size_t i = 1; i < n; ++i) {
        cfg_func.blocks[i].dominators = all_blocks;
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 1; i < n; ++i) {
            auto &b = cfg_func.blocks[i];
            std::set<int> intersection;

            if (!b.preds.empty()) {
                const BasicBlock *p0 = cfg_func.get_block(b.preds[0]);
                if (p0) intersection = p0->dominators;

                for (size_t p = 1; p < b.preds.size(); ++p) {
                    const BasicBlock *pred = cfg_func.get_block(b.preds[p]);
                    if (!pred) continue;
                    std::set<int> temp;
                    std::set_intersection(intersection.begin(), intersection.end(),
                                          pred->dominators.begin(), pred->dominators.end(),
                                          std::inserter(temp, temp.begin()));
                    intersection = temp;
                }
            }

            intersection.insert(b.id);
            if (intersection != b.dominators) {
                b.dominators = intersection;
                changed = true;
            }
        }
    }

    /* Step 2: Compute immediate dominator (idom)
     * For any block B != entry, dominators form a strict linear chain:
     * entry dom ... dom idom(B) dom B.
     * Thus, idom(B) is the unique strict dominator with the maximum |Dom(d)|. */
    cfg_func.blocks[0].idom = -1;
    for (size_t i = 1; i < n; ++i) {
        auto &b = cfg_func.blocks[i];
        std::set<int> strict_doms = b.dominators;
        strict_doms.erase(b.id);

        int best_idom = -1;
        size_t max_dom_size = 0;
        for (int d : strict_doms) {
            const BasicBlock *cand = cfg_func.get_block(d);
            if (!cand) continue;
            if (cand->dominators.size() > max_dom_size) {
                max_dom_size = cand->dominators.size();
                best_idom = d;
            }
        }

        b.idom = best_idom;
        if (best_idom != -1) {
            BasicBlock *idom_block = cfg_func.get_block(best_idom);
            if (idom_block) {
                idom_block->dom_children.push_back(b.id);
            }
        }
    }

    /* Step 3: Compute Dominance Frontiers */
    for (size_t i = 0; i < n; ++i) {
        const auto &b = cfg_func.blocks[i];
        if (b.preds.size() >= 2) {
            for (int p : b.preds) {
                int runner = p;
                while (runner != b.idom && runner != -1) {
                    BasicBlock *rb = cfg_func.get_block(runner);
                    if (!rb) break;
                    rb->dom_frontier.insert(b.id);
                    runner = rb->idom;
                }
            }
        }
    }
}

} // namespace minigo
