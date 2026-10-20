#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>

#include "minigo/common.h"
#include "minigo/lexer.h"
#include "minigo/parser.h"
#include "minigo/typecheck.h"
#include "minigo/tac.h"
#include "minigo/cfg.h"
#include "minigo/ssa.h"
#include "minigo/opt.h"
#include "minigo/codegen_ssa.h"

static void print_usage(const char *prog_name) {
    std::cout << "Mini Go Compiler (minigo) - SSA & x86-64 Backend\n";
    std::cout << "Usage: " << prog_name << " [options] <source.go>\n\n";
    std::cout << "Pipeline & Debug Inspection Options:\n";
    std::cout << "  -dump-tokens       Tokenize the input file and print tokens\n";
    std::cout << "  -dump-ast          Parse the input file and print AST representation\n";
    std::cout << "  -typecheck         Typecheck the program and report errors\n";
    std::cout << "  -dump-tac          Lower AST to Three-Address Code (TAC) and print IR\n";
    std::cout << "  -dump-cfg          Construct Control Flow Graph (CFG) and print basic blocks\n";
    std::cout << "  -dump-ssa          Convert CFG to Static Single Assignment (SSA) and print IR\n";
    std::cout << "  -O1, -opt          Run SSA optimizations (Constant Folding & DCE) with debug logs\n";
    std::cout << "  -S                 Generate x86-64 assembly text to stdout or specified -o file\n";
    std::cout << "  -o <file>          Specify output file (.s assembly or native executable)\n";
    std::cout << "  -h, --help         Show this help information\n";
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *source_path = nullptr;
    const char *output_path = nullptr;
    bool dump_tokens = false;
    bool dump_ast = false;
    bool do_typecheck = false;
    bool dump_tac = false;
    bool dump_cfg = false;
    bool dump_ssa = false;
    bool optimize = false;
    bool emit_assembly = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-dump-tokens") == 0) {
            dump_tokens = true;
        } else if (strcmp(argv[i], "-dump-ast") == 0) {
            dump_ast = true;
        } else if (strcmp(argv[i], "-typecheck") == 0) {
            do_typecheck = true;
        } else if (strcmp(argv[i], "-dump-tac") == 0) {
            dump_tac = true;
        } else if (strcmp(argv[i], "-dump-cfg") == 0) {
            dump_cfg = true;
        } else if (strcmp(argv[i], "-dump-ssa") == 0) {
            dump_ssa = true;
        } else if (strcmp(argv[i], "-O1") == 0 || strcmp(argv[i], "-opt") == 0) {
            optimize = true;
        } else if (strcmp(argv[i], "-S") == 0) {
            emit_assembly = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_path = argv[++i];
            } else {
                std::cerr << "Error: -o requires an argument\n";
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage(argv[0]);
            return 1;
        } else {
            source_path = argv[i];
        }
    }

    if (!source_path) {
        std::cerr << "Error: No input file specified\n";
        return 1;
    }

    size_t source_len = 0;
    char *source = read_file_to_string(source_path, &source_len);
    if (!source) {
        std::cerr << "Error: Could not open source file '" << source_path << "'\n";
        return 1;
    }

    /* Phase 1: Lexical Analysis */
    if (dump_tokens) {
        Lexer lexer;
        lexer_init(&lexer, source_path, source, source_len);
        std::cout << "--- Tokens for " << source_path << " ---\n";
        while (true) {
            Token tok = lexer_next_token(&lexer);
            token_print(&tok);
            if (tok.type == TOKEN_EOF) break;
        }
        free(source);
        return 0;
    }

    /* Phase 2: Syntax Analysis & AST Construction */
    Arena arena;
    arena_init(&arena, 64 * 1024);

    Parser parser;
    parser_init(&parser, source_path, source, source_len, &arena);
    ASTProgram *ast = parser_parse_program(&parser);

    if (parser.had_error) {
        std::cerr << "Compilation failed: parsing errors encountered.\n";
        arena_free_all(&arena);
        free(source);
        return 1;
    }

    if (dump_ast) {
        std::cout << "--- AST for " << source_path << " ---\n";
        ast_dump(ast, stdout);
        arena_free_all(&arena);
        free(source);
        return 0;
    }

    /* Phase 3: Semantic Analysis / Type Checking */
    TypeChecker tc;
    typechecker_init(&tc, &arena);
    bool type_ok = typecheck_program(&tc, ast);

    if (do_typecheck) {
        if (type_ok) {
            std::cout << "Type checking passed successfully: " << source_path << "\n";
        } else {
            std::cerr << "Type checking failed with errors: " << source_path << "\n";
        }
        arena_free_all(&arena);
        free(source);
        return type_ok ? 0 : 1;
    }

    /* Phase 4: TAC Lowering */
    minigo::TACProgram tac_prog = minigo::lower_ast_to_tac(ast);

    if (dump_tac) {
        std::cout << "--- Three-Address Code (TAC) for " << source_path << " ---\n";
        tac_prog.dump(std::cout);
        arena_free_all(&arena);
        free(source);
        return 0;
    }

    /* Phase 5: CFG Construction */
    minigo::CFGProgram cfg_prog = minigo::build_cfg(tac_prog);

    if (dump_cfg) {
        std::cout << "--- Control Flow Graph (CFG) for " << source_path << " ---\n";
        cfg_prog.dump(std::cout);
        arena_free_all(&arena);
        free(source);
        return 0;
    }

    /* Phase 6: SSA Form Conversion */
    minigo::SSAProgram ssa_prog = minigo::convert_cfg_to_ssa(cfg_prog);

    /* Phase 7: Optimization Passes (Constant Folding & DCE) */
    if (optimize) {
        minigo::optimize_program(ssa_prog, /*verbose=*/true, std::cout);
        if (dump_ssa) {
            arena_free_all(&arena);
            free(source);
            return 0;
        }
    } else if (dump_ssa) {
        std::cout << "--- Static Single Assignment (SSA) for " << source_path << " ---\n";
        ssa_prog.dump(std::cout);
        arena_free_all(&arena);
        free(source);
        return 0;
    }

    /* Phase 8: x86-64 Target Code Generation */
    minigo::CodegenOptions cg_opts;
#if defined(_WIN32) || defined(__CYGWIN__)
    cg_opts.is_windows_target = true;
#else
    cg_opts.is_windows_target = false;
#endif

    if (emit_assembly) {
        if (output_path) {
            if (!minigo::generate_x86_assembly_file(ssa_prog, output_path, cg_opts)) {
                std::cerr << "Error: Could not write assembly to '" << output_path << "'\n";
                arena_free_all(&arena);
                free(source);
                return 1;
            }
        } else {
            minigo::generate_x86_assembly(ssa_prog, std::cout, cg_opts);
        }
        arena_free_all(&arena);
        free(source);
        return 0;
    }

    /* Phase 9: GCC Driver & Assembler/Linker Bridge */
    std::string temp_asm = "minigo_tmp_" + std::to_string(rand()) + ".s";
    if (!minigo::generate_x86_assembly_file(ssa_prog, temp_asm, cg_opts)) {
        std::cerr << "Error: Could not create temporary assembly file\n";
        arena_free_all(&arena);
        free(source);
        return 1;
    }

    std::string bin_out = output_path ? output_path : "a.exe";
    std::string cmd = "gcc -no-pie -o \"" + bin_out + "\" \"" + temp_asm + "\"";
    int ret = system(cmd.c_str());
    remove(temp_asm.c_str());

    if (ret != 0) {
        std::cerr << "Assembler/Linker (GCC) failed with code " << ret << "\n";
        arena_free_all(&arena);
        free(source);
        return 1;
    }

    std::cout << "Successfully compiled '" << source_path << "' -> '" << bin_out << "'\n";

    arena_free_all(&arena);
    free(source);
    return 0;
}
