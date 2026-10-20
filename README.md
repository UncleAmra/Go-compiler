# Mini Go Compiler (`minigo`)

> **Compiler Course Final Project**  
> Target Architecture: **x86-64 Assembly** (GAS / AT&T Syntax)  
> Implementation Language: **C (C11) Frontend + C++ (C++17) Middle-End & Backend**  
> Complete Pipeline: **Lexer & Parser -> AST -> TAC -> CFG -> SSA -> Optimizations (O1) -> x86-64 Assembly -> GCC Bridge**

---

## Table of Contents
- [1. Project Overview & Features](#1-project-overview--features)
- [2. End-to-End Compiler Pipeline](#2-end-to-end-compiler-pipeline)
  - [Phase 1: Lexical Analysis (Scanner + ASI)](#phase-1-lexical-analysis-scanner--asi)
  - [Phase 2: Syntax Analysis (Recursive Descent + AST)](#phase-2-syntax-analysis-recursive-descent--ast)
  - [Phase 3: Semantic Analysis & Type Checking](#phase-3-semantic-analysis--type-checking)
  - [Phase 4: Three-Address Code (TAC) Lowering](#phase-4-three-address-code-tac-lowering)
  - [Phase 5: Basic Blocks & Control Flow Graph (CFG)](#phase-5-basic-blocks--control-flow-graph-cfg)
  - [Phase 6: Static Single Assignment (SSA) Form](#phase-6-static-single-assignment-ssa-form)
  - [Phase 7: SSA Optimization Passes (Constant Folding & DCE)](#phase-7-ssa-optimization-passes-constant-folding--dce)
  - [Phase 8: x86-64 Assembly Code Generation](#phase-8-x86-64-assembly-code-generation)
  - [Phase 9: GCC Bridge & Driver](#phase-9-gcc-bridge--driver)
- [3. Repository Structure](#3-repository-structure)
- [4. Quick Start & Build Instructions](#4-quick-start--build-instructions)
- [5. Compiling Mini Go Programs (GCC Bridge)](#5-compiling-mini-go-programs-gcc-bridge)
- [6. Intermediate Representation (IR) Inspection](#6-intermediate-representation-ir-inspection)
- [7. Running the Automated Test Suite](#7-running-the-automated-test-suite)

---

## 1. Project Overview & Features

`minigo` is a complete, educational, optimizing compiler for **Mini Go**, a statically typed subset of the Go programming language that compiles directly down to native **x86-64 assembly** and links with GCC into native binaries.

### Key Language Features Supported:
- **Package declarations:** `package main`
- **Functions & Parameters:** `func add(a int, b int) int { ... }`
- **Variable declarations:** Explicit (`var x int = 10`) and short declarations (`x := 10`)
- **Types:** `int`, `bool`, `string`, `*T` (pointers), `struct`
- **Control Flow:** `if / else` conditionals and `for` loops (including init, condition, and post-statements)
- **Arithmetic & Logical Expressions:** Full operator precedence hierarchy (`+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `!`, unary `-`)
- **Automatic Semicolon Insertion (ASI):** Faithful implementation of Go's official specification rule for newline-triggered semicolons
- **Built-in I/O:** `println(expr)` and `print(expr)` linked to C runtime `printf`

---

## 2. End-to-End Compiler Pipeline

The compiler is organized as a modular pipeline taking source code through nine clear stages:

```mermaid
flowchart TD
    Src["Source Code (.go)"] --> Lexer["1. Lexer (Scanner + ASI)"]
    Lexer -->|Tokens| Parser["2. Parser (Recursive Descent)"]
    Parser -->|AST| TypeCheck["3. Semantic Type Checker"]
    TypeCheck -->|Typed AST| TAC["4. TAC Lowering (3-Address Code)"]
    TAC -->|Linear TAC IR| CFG["5. CFG & Dominator Tree"]
    CFG -->|Basic Blocks + DF| SSA["6. SSA Conversion (Phi-Nodes)"]
    SSA -->|SSA IR| Opt["7. SSA Optimizations (Folding + DCE)"]
    Opt -->|Optimized SSA| Codegen["8. Target Codegen (x86-64)"]
    Codegen -->|Assembly File (.s)| GCC["9. GCC Assembler & Linker"]
    GCC --> Bin["Native Executable (.exe)"]
```

### Phase 1: Lexical Analysis (Scanner + ASI)
- Located in: `include/minigo/lexer.h`, `src/lexer.c`
- Tokenizes raw UTF-8 Go source text.
- **Automatic Semicolon Insertion (ASI):** Automatically inserts `TOKEN_SEMICOLON` at newlines if the preceding token was an identifier, literal, `return`, `break`, `continue`, or closing delimiter (`)`, `]`, `}`).

### Phase 2: Syntax Analysis (Recursive Descent + AST)
- Located in: `include/minigo/parser.h`, `include/minigo/ast.h`, `src/parser.c`, `src/ast.c`
- Parses statements and declarations using recursive descent.
- Evaluates binary expressions with correct operator precedence using precedence climbing.
- Allocates AST nodes inside a high-speed memory arena (`include/minigo/common.h`).

### Phase 3: Semantic Analysis & Type Checking
- Located in: `include/minigo/typecheck.h`, `src/typecheck.c`
- Manages lexical scopes and symbol tables (global, function, block).
- Validates declaration before use, checks type consistency, and infers types for `:=` declarations.

### Phase 4: Three-Address Code (TAC) Lowering
- Located in: `include/minigo/tac.h`, `src/tac.cpp`
- Flattens nested AST expressions into a linearized intermediate representation (IR) where every instruction has at most one operator and at most three operand addresses:
  ```text
  t0 = a * 2
  t1 = t0 + b
  t2 = t1 - 4
  c = t2
  ```
- Generates synthetic temporaries (`t0`, `t1`, ...) and jump labels (`L_else_0`, `L_endif_1`, `L_loop_cond_2`).

### Phase 5: Basic Blocks & Control Flow Graph (CFG)
- Located in: `include/minigo/cfg.h`, `src/cfg.cpp`
- Partitions TAC instructions into **Basic Blocks** using leader analysis (block entries, jump targets, post-jump instructions).
- Connects predecessor and successor edges, ensuring explicit fallthrough jumps between blocks.
- Filters unreachable blocks via BFS reachability.
- Computes **Dominators ($Dom(B)$)**, **Immediate Dominators ($idom(B)$)**, and **Dominance Frontiers ($DF(B)$)** using Cytron's textbook algorithm.

### Phase 6: Static Single Assignment (SSA) Form
- Located in: `include/minigo/ssa.h`, `src/ssa.cpp`
- Transforms the program into true SSA form where **every variable is assigned exactly once**:
  - Inserts $\phi$-nodes (e.g. `sum_1 = phi([B3: sum_0], [B5: sum_2])`) at join points identified by iterated dominance frontiers ($IDF$).
  - Renames variable definitions into monotonically increasing versions (`x_0`, `x_1`, `x_2`).
  - Resolves variable operand reads to their currently active reaching definition using dominator-tree depth-first search (DFS).

### Phase 7: SSA Optimization Passes (Constant Folding & DCE)
- Located in: `include/minigo/opt.h`, `src/opt.cpp`
- **Constant Folding & Propagation:** Evaluates arithmetic expressions (`+`, `-`, `*`, `/`, `%`) and comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`) at compile time when operands are constants. Folds identical $\phi$-nodes and resolves conditional branches whose conditions are statically known.
- **Dead Code Elimination (DCE):** Constructs a dependency graph from observable side-effect roots (`println`, `ret`, `call`, branches) and eliminates unreferenced variables and unused instructions.
- Iterates passes in a fixed-point loop until no further optimizations can be made.
- Prints clear before-and-after debug logs when `-O1` or `-opt` is enabled.

### Phase 8: x86-64 Assembly Code Generation
- Located in: `include/minigo/codegen_ssa.h`, `src/codegen_ssa.cpp`
- **Out-of-SSA Deconstruction:** Eliminates $\phi$-nodes by placing parallel copies along incoming predecessor edges before branch instructions.
- **Stack Frame Layout:** Allocates unique 8-byte slots for all SSA variables and temporaries, ensuring 16-byte stack frame alignment.
- **Calling Conventions:**
  - **Windows x64 ABI:** Parameters passed in `%rcx`, `%rdx`, `%r8`, `%r9` with 32-byte shadow space.
  - **System V AMD64 ABI (Linux/macOS):** Parameters passed in `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`.
- Emits clean, well-commented GAS / AT&T syntax assembly.

### Phase 9: GCC Bridge & Driver
- Located in: `src/main.cpp`
- Emits the assembly text file (`.s`) and invokes the system `gcc` assembler/linker to produce a standalone native binary (`.exe`).

---

## 3. Repository Structure

```
compiler/
├── CMakeLists.txt              # CMake build script supporting C11 and C++17
├── Makefile                    # Portable Makefile (Windows mingw32-make & Linux make)
├── README.md                   # This documentation
├── docs/
│   ├── ARCHITECTURE.md         # Internal compiler architecture reference
│   ├── MINI_GO_SPEC.md         # Mini Go grammar and language specification
│   └── FORMAL_VERIFICATION.md  # Extra points roadmap for Rocq (Coq) / Lean 4
├── include/
│   └── minigo/
│       ├── common.h            # SourcePos, memory arena allocator, error diagnostics
│       ├── token.h             # Token definitions, keywords, operators
│       ├── lexer.h             # Lexer state and scanner interface
│       ├── ast.h               # AST nodes, expressions, statements, types
│       ├── parser.h            # Recursive descent parser interface
│       ├── typecheck.h         # Symbol tables, scopes, type checker
│       ├── tac.h               # Three-Address Code (TAC) structures and lowering
│       ├── cfg.h               # Basic blocks, CFG construction, Dominator tree & DF
│       ├── ssa.h               # SSA form, Phi-nodes, SSA conversion
│       ├── opt.h               # SSA optimizations: Constant Folding & DCE
│       └── codegen_ssa.h       # x86-64 assembly generation from SSA IR
├── src/
│   ├── common.c                # Arena allocator and diagnostic implementation
│   ├── token.c                 # Token formatting and string conversion
│   ├── lexer.c                 # Hand-written Mini Go lexer with ASI
│   ├── ast.c                   # AST node constructors & visual tree dumper
│   ├── parser.c                # Parser with precedence climbing
│   ├── typecheck.c             # Semantic analyzer and scope management
│   ├── tac.cpp                 # AST to linear TAC lowering implementation
│   ├── cfg.cpp                 # Basic block partition, dominators, dominance frontiers
│   ├── ssa.cpp                 # Phi-node placement and dominator-tree DFS renaming
│   ├── opt.cpp                 # Constant folding, propagation, and DCE passes
│   ├── codegen_ssa.cpp         # Out-of-SSA deconstruction and x86-64 code generation
│   └── main.cpp                # Compiler CLI driver & GCC bridge
├── examples/
│   ├── 01_hello.go             # Println and return
│   ├── 02_arithmetic.go        # Binary arithmetic, variable assignment, precedence
│   ├── 03_control_flow.go      # If-else conditionals and for-loops
│   ├── 04_functions.go         # Multi-parameter function declaration and calls
│   └── 05_structs.go           # Struct type definitions
└── tests/
    ├── test_runner.py          # Automated integration test runner
    └── test_lexer.c            # Unit tests for lexer & ASI
```

---

## 4. Quick Start & Build Instructions

### Prerequisites
- **C/C++ Toolchain:** GCC and G++ with C++17 support (`gcc` / `g++` via MinGW-w64 on Windows or standard GCC on Linux)
- **Make:** `mingw32-make` (Windows) or `make` (Linux)
- **Python:** Python 3.8+ (for automated tests)

### Build with Make (Recommended)
On Windows (PowerShell / CMD):
```powershell
mingw32-make
```
On Linux / macOS:
```bash
make
```

### Direct Compilation with GCC & G++
If you prefer building without make:
```powershell
# 1. Compile C frontend modules
gcc -std=c11 -Wall -Wextra -Iinclude -O2 -c src/common.c src/token.c src/lexer.c src/ast.c src/parser.c src/typecheck.c

# 2. Compile C++ middle-end and backend modules
g++ -std=c++17 -Wall -Wextra -Iinclude -O2 -c src/tac.cpp src/cfg.cpp src/ssa.cpp src/opt.cpp src/codegen_ssa.cpp src/main.cpp

# 3. Link everything into the compiler executable
g++ -std=c++17 *.o -o build/minigo.exe
```

---

## 5. Compiling Mini Go Programs (GCC Bridge)

The compiler supports both **two-stage compilation** (generating a `.s` file and piping into GCC) and **one-step compilation** (direct executable generation).

### Method A: Two-Step Assembly & GCC Linking (Pipeline Inspection)
Generate the x86-64 assembly text file with `-S` and compile it with `gcc`:
```powershell
# 1. Generate x86-64 assembly text file (.s)
.\build\minigo.exe examples/02_arithmetic.go -S -o output.s

# 2. Assemble and link with GCC into an executable
gcc output.s -o program.exe

# 3. Execute the resulting binary
.\program.exe
```

You can chain these commands in a single line:
```powershell
.\build\minigo.exe examples/02_arithmetic.go -S -o output.s && gcc output.s -o program.exe && .\program.exe
```

### Method B: One-Step Compilation via GCC Bridge
The compiler automatically handles the intermediate assembly and GCC invocation:
```powershell
.\build\minigo.exe -o program.exe examples/02_arithmetic.go
.\program.exe
```

### Method C: Optimized Compilation (`-O1` / `-opt`)
Enable Constant Folding and Dead Code Elimination before code generation:
```powershell
.\build\minigo.exe -O1 examples/02_arithmetic.go -S -o output_opt.s && gcc output_opt.s -o program_opt.exe && .\program_opt.exe
```

---

## 6. Intermediate Representation (IR) Inspection

`minigo` provides CLI flags to inspect every phase of the compiler pipeline:

### 1. Three-Address Code (TAC)
Flattened linear IR with temporaries:
```powershell
.\build\minigo.exe -dump-tac examples/02_arithmetic.go
```
*Output preview:*
```text
func main():
    a = 15
    b = 27
    t0 = a * 2
    t1 = t0 + b
    t2 = t1 - 4
    c = t2
    println(c)
    ret c
```

### 2. Control Flow Graph (CFG) & Basic Blocks
Shows basic blocks, predecessors, successors, and fallthroughs:
```powershell
.\build\minigo.exe -dump-cfg examples/03_control_flow.go
```

### 3. Static Single Assignment (SSA) Form
Displays versioned variables and $\phi$-nodes at control flow merge points:
```powershell
.\build\minigo.exe -dump-ssa examples/03_control_flow.go
```
*Output preview:*
```text
function main():
  B0 (B0):
    [preds: | succs: B2 B1]
    x_0 = 10
    t0_0 = x_0 > 5
    ifz t0_0 goto B2
    jmp B1
  ...
  B4 (B4):
    [preds: B3 B5 | succs: B6 B5]
    i_1 = phi([B3: i_0], [B5: i_2])
    sum_1 = phi([B3: sum_0], [B5: sum_2])
    t1_0 = i_1 < 10
    ifz t1_0 goto B6
    jmp B5
  B5 (B5):
    [preds: B4 | succs: B4]
    t2_0 = sum_1 + i_1
    sum_2 = t2_0
    t3_0 = i_1 + 1
    i_2 = t3_0
    jmp B4
  B6 (B6):
    [preds: B4 | succs:]
    println(sum_1)
    ret sum_1
```

### 4. Before-and-After Optimization Log (`-O1`)
Displays the transformation made by Constant Folding and Dead Code Elimination:
```powershell
.\build\minigo.exe -O1 examples/02_arithmetic.go
```
*Output preview:*
```text
============================================================
              SSA IR BEFORE OPTIMIZATION
============================================================
function main():
  B0 (B0):
    a_0 = 15
    b_0 = 27
    t0_0 = a_0 * 2
    t1_0 = t0_0 + b_0
    t2_0 = t1_0 - 4
    c_0 = t2_0
    println(c_0)
    ret c_0

============================================================
              SSA IR AFTER OPTIMIZATION
============================================================
function main():
  B0 (B0):
    println(53)
    ret 53

============================================================
Optimization Summary:
  - Constants folded:       3
  - Dead instructions removed: 6
============================================================
```

---

## 7. Running the Automated Test Suite

Run the end-to-end integration test runner:
```powershell
python tests/test_runner.py
```
Run the unit tests:
```powershell
.\build\test_lexer.exe
```
# 1. Build the compiler from source
mingw32-make

# 2. Inspect intermediate representation (IR) phases
.\build\minigo.exe -dump-tac examples/02_arithmetic.go
.\build\minigo.exe -dump-cfg examples/03_control_flow.go
.\build\minigo.exe -dump-ssa examples/03_control_flow.go
.\build\minigo.exe -O1 examples/02_arithmetic.go

# 3. Run full optimization pipeline, compile to assembly, link with GCC, and execute
.\build\minigo.exe -O1 examples/02_arithmetic.go -S -o output.s; gcc output.s -o program.exe; .\program.exe

# 4. Inspect generated assembly and machine code
Get-Content output.s
objdump -d program.exe