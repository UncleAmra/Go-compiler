# Mini Go Compiler (`minigo`)

> **Compiler Course Final Project (40% of Total Grade)**  
> Target Architecture: **x86-64 Assembly** (GAS Syntax)  
> Implementation Language: **C (C11)** / Extensible with **C++**  
> Optional Extra Points: **Formal Verification in Rocq (Coq) or Lean 4**

---

## Table of Contents
- [1. Project Overview & Objectives](#1-project-overview--objectives)
- [2. How This Compiler Works](#2-how-this-compiler-works)
- [3. What Languages Do People Use for Compilers?](#3-what-languages-do-people-use-for-compilers)
- [4. Architectural Pipeline](#4-architectural-pipeline)
- [5. Repository Structure](#5-repository-structure)
- [6. Course Schedule & Milestone Roadmap](#6-course-schedule--milestone-roadmap)
- [7. Quick Start & Build Instructions](#7-quick-start--build-instructions)
- [8. Running Tests & Examples](#8-running-tests--examples)
- [9. Extra Credit: Formal Verification](#9-extra-credit-formal-verification)

---

## 1. Project Overview & Objectives

The goal of this project is to design and implement an end-to-end compiler for **Mini Go**, a statically typed subset of the Go programming language that compiles down to native **x86-64 assembly**.

### Key Language Features Supported:
- **Package declarations:** `package main`
- **Functions & Parameters:** `func name(x int, y int) int { ... }`
- **Variable declarations:** Explicit (`var x int = 10`) and short declarations (`x := 10`)
- **Primitive & Composite Types:** `int`, `bool`, `string`, `*T` (pointers), `struct`
- **Control Flow:** `if / else` conditionals and `for` loops
- **Arithmetic & Logical Expressions:** Full operator precedence hierarchy (`+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `!`)
- **Automatic Semicolon Insertion (ASI):** Faithful reproduction of Go's official specification rule for newline-triggered semicolons
- **Built-in I/O:** `println(expr)` and `print(expr)`

---

## 2. How This Compiler Works

A compiler is a program that translates human-readable source code into machine instructions that a CPU can directly execute. `minigo` processes code through five core phases:

```mermaid
flowchart TD
    Src["Source Code (.go)"] --> Lexer["1. Lexical Analysis (Scanner + ASI)"]
    Lexer -->|Token Stream| Parser["2. Syntax Analysis (Parser)"]
    Parser -->|Abstract Syntax Tree (AST)| TypeCheck["3. Semantic Analysis (Type Checker)"]
    TypeCheck -->|Decorated AST + Symbol Table| Codegen["4. Code Generation (x86-64)"]
    Codegen -->|Assembly File (.s)| Toolchain["5. Assembler & Linker (gcc)"]
    Toolchain --> Bin["Native Executable (.exe / binary)"]
```

### Phase 1: Lexical Analysis (Lexer) + Automatic Semicolon Insertion (ASI)
- Reads the raw source text character-by-character and groups them into meaningful **tokens** (e.g. keywords, identifiers, numbers, operators).
- **Go's Unique Feature (ASI):** Unlike C or Java where developers must write explicit semicolons, Go automatically inserts semicolons. When the lexer hits a newline (`\n`), if the previous token was an identifier, literal, `return`, `break`, `continue`, or closing delimiter (`)`, `]`, `}`), the lexer automatically yields a `TOKEN_SEMICOLON`.

### Phase 2: Syntax Analysis (Parser) & AST Construction
- Validates that the token sequence obeys Mini Go's grammar rules using **Recursive Descent Parsing**.
- For expressions (`a + b * c`), it uses **Precedence Climbing** to guarantee proper mathematical order of operations.
- Builds an **Abstract Syntax Tree (AST)** allocated cleanly inside a **Dynamic Arena Allocator** for high speed and zero memory leaks.

### Phase 3: Semantic Analysis & Type Checking
- Manages nested **Symbol Tables (Scopes)**: global package scope, function scopes, and block scopes.
- Verifies that variables are declared before use, detects type mismatches (e.g., assigning a string to an integer), and infers types for `:=` declarations.

### Phase 4: Target Code Generation (x86-64)
- Traverses the AST and translates language constructs into **x86-64 assembly instructions** using GNU Assembler (GAS) format:
  - Function entry/exit with standard stack frame prologue (`pushq %rbp; movq %rsp, %rbp; subq $N, %rsp`) and epilogue (`leave; ret`).
  - Expression evaluation using accumulator (`%rax`) and register stack operations.
  - Calling conventions: Supports both **Windows x64 ABI** (`%rcx`, `%rdx`, `%r8`, `%r9` + shadow space) and **System V AMD64 ABI** (`%rdi`, `%rsi`, `%rdx`, `%rcx`, etc. for Linux/macOS).

### Phase 5: Assembler and Linker Driver
- `minigo` emits `.s` assembly and automatically invokes `gcc` (or `as` + `ld`) to assemble the instructions and link against standard C runtime libraries (e.g. `printf` for I/O), producing a native executable.

---

## 3. What Languages Do People Use for Compilers?

You asked what language people typically use to write compilers. Here is how industry and academia choose:

| Language | Typical Use Case | Why People Choose It | Trade-offs |
| :--- | :--- | :--- | :--- |
| **C** | TinyCC (tcc), Lua, early GCC, CPython | Ultimate hardware transparency, manual memory layout, no hidden runtime overhead. | Manual data structures, no built-in pattern matching. |
| **C++** | **LLVM / Clang**, modern GCC, V8 JavaScript engine | The industry standard for production compilers. Rich STL, polymorphic AST class hierarchies, LLVM ecosystem. | Complex syntax, steep learning curve. |
| **OCaml / Haskell** | Academic courses, CompCert, early Rust | Algebraic Data Types (ADTs) and pattern matching make manipulating ASTs very concise. | Less intuitive mapping to hardware memory models. |
| **Rust** | Rustc, SWC, modern tooling | Pattern matching + memory safety without a garbage collector. | Strict borrow checker can complicate graph-like ASTs. |

### Why C (with C++ compatibility) was chosen for this project:
1. **Total Visibility:** Writing the compiler in C forces you to understand **memory layout, pointer arithmetic, struct alignment, and stack frames**—exactly what compiler exams test.
2. **Dynamic Arena Memory Management:** Rather than struggling with hundreds of `malloc`/`free` calls, we use an arena allocator (a standard compiler industry pattern).
3. **Seamless Extensibility:** All header files use standard C interfaces, so you can easily write future modules (e.g. register allocation or optimizer) in C++ simply by compiling with `g++`.

---

## 4. Architectural Pipeline

```text
[Input Mini Go File]
         |
         v
  [src/lexer.c]        <-- Scans characters, handles comments, performs ASI
         |  (Tokens)
         v
  [src/parser.c]       <-- Recursive descent + precedence climbing
         |  (AST Nodes in Arena)
         v
  [src/typecheck.c]    <-- Scopes, symbol table, type validation
         |  (Typed AST)
         v
  [src/codegen_x86.c]  <-- Emits GAS x86-64 assembly instructions
         |  (Assembly .s)
         v
  [gcc driver]         <-- Assembles & links native executable
         |
         v
  [Native Binary (.exe)]
```

---

## 5. Repository Structure

```
compiler/
├── CMakeLists.txt              # Standard CMake build configuration
├── Makefile                    # Makefile for make / mingw32-make
├── .gitignore                  # Git ignore rules for build artifacts
├── README.md                   # Project overview, roadmap & documentation
├── docs/
│   ├── ARCHITECTURE.md         # Detailed compiler internal architecture
│   ├── MINI_GO_SPEC.md         # Mini Go language syntax & grammar specification
│   └── FORMAL_VERIFICATION.md  # Extra points roadmap for Rocq (Coq) / Lean 4
├── include/
│   └── minigo/
│       ├── common.h            # Source positions, arena allocator, error diagnostics
│       ├── token.h             # Token definitions, keywords, operators
│       ├── lexer.h             # Lexer state and interface
│       ├── ast.h               # AST definitions, types, expressions, statements
│       ├── parser.h            # Parser interface
│       ├── typecheck.h         # Symbol table, scopes, type checker
│       └── codegen.h           # x86-64 code generator interface
├── src/
│   ├── main.c                  # Compiler CLI driver (-dump-tokens, -dump-ast, -S, -o)
│   ├── common.c                # Memory arena & diagnostic implementation
│   ├── token.c                 # Token string conversion & formatting
│   ├── lexer.c                 # Hand-written Mini Go lexer with ASI
│   ├── ast.c                   # AST node constructors & visual tree dumper
│   ├── parser.c                # Recursive descent parser & precedence climbing
│   ├── typecheck.c             # Semantic analyzer and scope management
│   └── codegen_x86.c           # x86-64 assembly code generator
├── examples/
│   ├── 01_hello.go             # Basic return and println
│   ├── 02_arithmetic.go        # Variables and binary arithmetic expressions
│   ├── 03_control_flow.go      # If-else conditionals and for loops
│   ├── 04_functions.go         # Function declarations and multi-parameter passing
│   └── 05_structs.go           # Struct type definition
└── tests/
    ├── test_runner.py          # Automated Python integration test suite
    └── test_lexer.c            # Unit tests for lexer & ASI
```

---

## 6. Course Schedule & Milestone Roadmap

This roadmap connects your course syllabus weeks directly to compiler deliverables:

| Week | Course Topic | Syllabus Reference | Compiler Project Milestone | Status |
| :--- | :--- | :--- | :--- | :---: |
| **W01** | 09/09, 11 | Course info, OCaml intro | Repository setup, environment verification (GCC / MinGW), CLI design | Completed |
| **W02** | 09/16, 18 | x86-64 Assembly | Target architecture definition, calling conventions, stack frame layout | Completed |
| **W03** | 09/23, 25 | AST & Semantics | AST node definitions, Arena allocator, grammar specification | Completed |
| **W04** | 09/30, 10/01 | HW1, Interpreter | AST tree printer (`-dump-ast`), small-step expression evaluator | Completed |
| **W05** | 10/07, 09 | *No class (OOPSLA)* | Catch-up / buffer period | Completed |
| **W06** | 10/14, 16 | Lexical analysis | Complete lexer implementation, keyword matching, Automatic Semicolon Insertion | Completed |
| **W07** | 10/21, 23 | HW2 (mini-Python), Parsing | Precedence climbing for binary expressions, primary expressions, calls | Completed |
| **W08** | 10/28, 30 | Parsing | Statements (`var`, `:=`, `if`, `for`, `return`), full file parser | Completed |
| **W09** | 11/04, 06 | Typing | Scope stack, symbol table, type inference for `:=`, type verification | Completed |
| **W10** | 11/11, 13 | HW3 (mini-Turtle), Mid-term | **Midterm Milestone Check:** Complete frontend (Lexer + Parser + Typecheck) | Current |
| **W11** | 11/18, 20 | Parameters passing | Multi-argument passing via ABI registers (`%rcx`/`%rdx` or `%rdi`/`%rsi`) | Completed |
| **W12** | 11/25, 27 | Compiler correctness, FP | Function pointers, nested scopes, start Formal Verification scaffold | Planned |
| **W13** | 12/02, 04 | HW4 (W algorithm), FP | Algorithm W / Hindley-Milner type inference concepts | Planned |
| **W14** | 12/09, 11 | Compilation of OO | Struct memory layout, field offset calculation (`expr.field`), methods | Planned |
| **W15** | 12/16, 18 | Compiler optimization | Constant folding, dead code elimination, algebraic simplifications | Planned |
| **W16** | 12/23, 25 | Compiler optimization, *Noël*| Control Flow Graph (CFG) construction, peephole optimizations | Planned |
| **W17** | 12/30, 01/01 | GC, *Happy New Year* | Memory management runtime: Mark-and-Sweep / Arena / Boehm GC | Planned |
| **W18** | 01/06, 08 | GC | Final integration, performance benchmarks, project report & presentation | Planned |

---

## 7. Quick Start & Build Instructions

### Prerequisites
- **C Compiler:** `gcc` (MinGW-w64 on Windows or standard GCC on Linux)
- **Make / CMake:** `mingw32-make` or `make` or `cmake` (optional)
- **Python:** Python 3.8+ (for automated test runner)

### Method 1: Direct Build with GCC
```powershell
gcc -std=c11 -Wall -Wextra -Iinclude src/common.c src/token.c src/lexer.c src/ast.c src/parser.c src/typecheck.c src/codegen_x86.c src/main.c -o build/minigo.exe
```

### Method 2: Build with Makefile
```powershell
mingw32-make
# or on Linux / macOS:
make
```

### Method 3: Build with CMake
```powershell
cmake -B build
cmake --build build
```

---

## 8. Running Tests & Examples

### CLI Command Options
```text
Mini Go Compiler (minigo)
Usage: minigo [options] <source.go>

Options:
  -dump-tokens       Tokenize the input file and print tokens (shows ASI)
  -dump-ast          Parse the input file and print AST visual tree
  -typecheck         Typecheck the program and report errors
  -S                 Generate x86-64 assembly to stdout or specified -o file
  -o <file>          Specify output file (binary executable, or assembly with -S)
  -h, --help         Show help information
```

### 1. Inspecting Tokens (Lexer + ASI)
```powershell
.\build\minigo.exe -dump-tokens examples/01_hello.go
```

### 2. Inspecting the Abstract Syntax Tree (AST)
```powershell
.\build\minigo.exe -dump-ast examples/02_arithmetic.go
```

### 3. Inspecting Generated x86-64 Assembly
```powershell
.\build\minigo.exe -S examples/04_functions.go
```

### 4. Compiling and Executing Native Binary
```powershell
.\build\minigo.exe -o build/run_arith.exe examples/02_arithmetic.go
.\build\run_arith.exe
# Output: 53
```

### 5. Running the Automated Test Suite
```powershell
python tests/test_runner.py
```

Expected output:
```text
========================================
 Running Mini Go Compiler Integration Tests 
========================================
--> Testing 01_hello.go... PASSED
--> Testing 02_arithmetic.go... PASSED
--> Testing 03_control_flow.go... PASSED
--> Testing 04_functions.go... PASSED
========================================
Results: 4/4 tests passed.
========================================
```

---

## 9. Extra Credit: Formal Verification

The syllabus offers extra credit for formal verification in **Rocq (Coq)** or **Lean 4**.
A dedicated guide and starter scaffold are provided in [docs/FORMAL_VERIFICATION.md](file:///C:/Users/User/Desktop/compiler/docs/FORMAL_VERIFICATION.md):
- **Option 1 (Recommended):** Operational semantics and Type Soundness proof (Progress & Preservation theorems).
- **Option 2:** Verified expression compilation to an abstract stack machine.
