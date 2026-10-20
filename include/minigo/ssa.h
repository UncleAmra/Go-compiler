#ifndef MINIGO_SSA_H
#define MINIGO_SSA_H

#include <string>
#include <vector>
#include <iostream>
#include <utility>
#include "minigo/tac.h"
#include "minigo/cfg.h"

namespace minigo {

struct SSAPhi {
    std::string dest;                                 /* SSA variable name e.g. "x_2" */
    std::string original_var;                         /* Base variable name e.g. "x" */
    std::vector<std::pair<int, std::string>> incoming; /* (pred_block_id, ssa_version) */

    std::string to_string() const;
};

struct SSABlock {
    int id = 0;
    std::string label;
    std::vector<int> preds;
    std::vector<int> succs;
    std::vector<SSAPhi> phis;
    std::vector<TACInstr> instrs;

    bool has_predecessor(int p) const;
    bool has_successor(int s) const;
    std::string to_string() const;
};

struct SSAFunc {
    std::string name;
    std::vector<std::string> params;      /* Original parameter names */
    std::vector<std::string> ssa_params;  /* SSA versioned parameter names */
    std::vector<SSABlock> blocks;
    int entry_block_id = 0;

    SSABlock *get_block(int id);
    const SSABlock *get_block(int id) const;
    std::string to_string() const;
};

struct SSAProgram {
    std::vector<SSAFunc> funcs;

    std::string to_string() const;
    void dump(std::ostream &os) const;
};

/* Converts CFG with dominator analysis into Static Single Assignment (SSA) form */
SSAProgram convert_cfg_to_ssa(const CFGProgram &cfg_prog);

} // namespace minigo

#endif /* MINIGO_SSA_H */
