#ifndef MINIGO_CFG_H
#define MINIGO_CFG_H

#include <vector>
#include <string>
#include <set>
#include <map>
#include <iostream>
#include "minigo/tac.h"

namespace minigo {

struct BasicBlock {
    int id = 0;
    std::string label;
    std::vector<TACInstr> instrs;
    std::vector<int> preds;
    std::vector<int> succs;

    /* Dominator information */
    int idom = -1;
    std::vector<int> dom_children;
    std::set<int> dom_frontier;
    std::set<int> dominators;

    bool has_successor(int succ_id) const;
    bool has_predecessor(int pred_id) const;
    std::string to_string() const;
};

struct CFGFunc {
    std::string name;
    std::vector<std::string> params;
    std::vector<BasicBlock> blocks;
    int entry_block_id = 0;

    BasicBlock *get_block(int id);
    const BasicBlock *get_block(int id) const;
    std::string to_string() const;
};

struct CFGProgram {
    std::vector<CFGFunc> funcs;

    std::string to_string() const;
    void dump(std::ostream &os) const;
};

/* Builds Control Flow Graphs from TAC functions */
CFGProgram build_cfg(const TACProgram &tac_prog);

/* Computes Dominators and Dominance Frontiers for a CFG function */
void compute_dominators_and_frontiers(CFGFunc &cfg_func);

} // namespace minigo

#endif /* MINIGO_CFG_H */
