#ifndef MINIGO_OPT_H
#define MINIGO_OPT_H

#include <iostream>
#include "minigo/ssa.h"

namespace minigo {

struct OptimizationStats {
    int constants_folded = 0;
    int instructions_eliminated = 0;
    int branches_folded = 0;
    int phi_nodes_eliminated = 0;
};

/* Performs Constant Folding and Propagation on an SSA function */
bool pass_constant_folding(SSAFunc &func, OptimizationStats &stats);

/* Performs Dead Code Elimination (DCE) on an SSA function */
bool pass_dead_code_elimination(SSAFunc &func, OptimizationStats &stats);

/* Runs optimization pipeline to a fixed point on an SSA function */
void optimize_function(SSAFunc &func, OptimizationStats &stats);

/* Runs optimization pipeline on the entire SSA program.
 * If verbose is true, prints before-and-after debug log of the SSA IR. */
void optimize_program(SSAProgram &prog, bool verbose = false, std::ostream &log_out = std::cout);

} // namespace minigo

#endif /* MINIGO_OPT_H */
