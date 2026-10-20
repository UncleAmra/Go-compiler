#ifndef MINIGO_CODEGEN_SSA_H
#define MINIGO_CODEGEN_SSA_H

#include <iostream>
#include <string>
#include "minigo/ssa.h"

namespace minigo {

struct CodegenOptions {
    bool is_windows_target = true;
    bool verbose_comments = true;
};

/* Emits valid x86-64 assembly text from the SSA Program */
bool generate_x86_assembly(const SSAProgram &ssa_prog,
                           std::ostream &out,
                           const CodegenOptions &opts = CodegenOptions());

/* Generates assembly directly to a file path */
bool generate_x86_assembly_file(const SSAProgram &ssa_prog,
                                const std::string &filepath,
                                const CodegenOptions &opts = CodegenOptions());

} // namespace minigo

#endif /* MINIGO_CODEGEN_SSA_H */
