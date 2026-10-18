#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minigo/common.h"
#include "minigo/lexer.h"
#include "minigo/parser.h"
#include "minigo/typecheck.h"
#include "minigo/codegen.h"

static void print_usage(const char *prog_name) {
    printf("Mini Go Compiler (minigo)\n");
    printf("Usage: %s [options] <source.go>\n\n", prog_name);
    printf("Options:\n");
    printf("  -dump-tokens       Tokenize the input file and print tokens\n");
    printf("  -dump-ast          Parse the input file and print AST representation\n");
    printf("  -typecheck         Typecheck the program and report errors\n");
    printf("  -S                 Generate x86-64 assembly to stdout or specified -o file\n");
    printf("  -o <file>          Specify output file (binary or assembly with -S)\n");
    printf("  -h, --help         Show this help information\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *source_path = NULL;
    const char *output_path = NULL;
    bool dump_tokens = false;
    bool dump_ast = false;
    bool do_typecheck = false;
    bool emit_assembly = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-dump-tokens") == 0) {
            dump_tokens = true;
        } else if (strcmp(argv[i], "-dump-ast") == 0) {
            dump_ast = true;
        } else if (strcmp(argv[i], "-typecheck") == 0) {
            do_typecheck = true;
        } else if (strcmp(argv[i], "-S") == 0) {
            emit_assembly = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_path = argv[++i];
            } else {
                fprintf(stderr, "Error: -o requires an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            source_path = argv[i];
        }
    }

    if (!source_path) {
        fprintf(stderr, "Error: No input file specified\n");
        return 1;
    }

    size_t source_len = 0;
    char *source = read_file_to_string(source_path, &source_len);
    if (!source) {
        fprintf(stderr, "Error: Could not open source file '%s'\n", source_path);
        return 1;
    }

    /* 1. Lexing phase / dump tokens */
    if (dump_tokens) {
        Lexer lexer;
        lexer_init(&lexer, source_path, source, source_len);
        printf("--- Tokens for %s ---\n", source_path);
        while (1) {
            Token tok = lexer_next_token(&lexer);
            token_print(&tok);
            if (tok.type == TOKEN_EOF) break;
        }
        free(source);
        return 0;
    }

    /* 2. Parsing phase */
    Arena arena;
    arena_init(&arena, 64 * 1024);

    Parser parser;
    parser_init(&parser, source_path, source, source_len, &arena);
    ASTProgram *prog = parser_parse_program(&parser);

    if (parser.had_error) {
        fprintf(stderr, "Compilation failed: parsing errors encountered.\n");
        arena_free_all(&arena);
        free(source);
        return 1;
    }

    if (dump_ast) {
        printf("--- AST for %s ---\n", source_path);
        ast_dump(prog, stdout);
        arena_free_all(&arena);
        free(source);
        return 0;
    }

    /* 3. Type checking phase */
    TypeChecker tc;
    typechecker_init(&tc, &arena);
    bool type_ok = typecheck_program(&tc, prog);

    if (do_typecheck) {
        if (type_ok) {
            printf("Type checking passed successfully: %s\n", source_path);
        } else {
            fprintf(stderr, "Type checking failed with errors: %s\n", source_path);
        }
        arena_free_all(&arena);
        free(source);
        return type_ok ? 0 : 1;
    }

    /* 4. Code generation phase (x86-64) */
    FILE *out_file = stdout;
    char temp_asm_path[512] = {0};

    if (emit_assembly) {
        if (output_path) {
            out_file = fopen(output_path, "w");
            if (!out_file) {
                fprintf(stderr, "Error: Could not open output file '%s'\n", output_path);
                arena_free_all(&arena);
                free(source);
                return 1;
            }
        }
    } else {
        /* Produce assembly in temporary file, then invoke gcc to assemble & link */
        snprintf(temp_asm_path, sizeof(temp_asm_path), "minigo_tmp_%d.s", (int)rand());
        out_file = fopen(temp_asm_path, "w");
        if (!out_file) {
            fprintf(stderr, "Error: Could not create temporary assembly file\n");
            arena_free_all(&arena);
            free(source);
            return 1;
        }
    }

    CodegenX86 cg;
    codegen_x86_init(&cg, out_file, prog);
    codegen_x86_emit(&cg);

    if (out_file != stdout) {
        fclose(out_file);
    }

    /* If compiling to executable */
    if (!emit_assembly) {
        const char *bin_out = output_path ? output_path : "a.exe";
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "gcc -no-pie -o \"%s\" \"%s\"", bin_out, temp_asm_path);
        int ret = system(cmd);
        remove(temp_asm_path);
        if (ret != 0) {
            fprintf(stderr, "Assembler/Linker failed with code %d\n", ret);
            arena_free_all(&arena);
            free(source);
            return 1;
        }
        printf("Successfully compiled '%s' -> '%s'\n", source_path, bin_out);
    }

    arena_free_all(&arena);
    free(source);
    return 0;
}
