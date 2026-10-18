# Mini Go Compiler Architecture

This document explains the internal design, algorithms, and data structures of the Mini Go Compiler (`minigo`).

---

## 1. High-Level Compiler Pipeline

```mermaid
flowchart LR
    A["Source Code (.go)"] --> B["Lexer + ASI"]
    B -->|Token Stream| C["Recursive Descent Parser"]
    C -->|Abstract Syntax Tree| D["Type Checker & Symbol Table"]
    D -->|Decorated AST| E["x86-64 Code Generator"]
    E -->|Assembly (.s)| F["Assembler & Linker (gcc/as)"]
    F --> G["Executable (.exe / ELF)"]
```

---

## 2. Component Breakdown

### 2.1 Memory Management: The Dynamic Arena Allocator
- Compilers construct thousands of small objects: tokens, identifiers, AST expressions, type structures, and symbol entries.
- Individual calls to `malloc()` and `free()` cause fragmentation, pointer tracking complexity, and memory leak hazards.
- **Solution:** `minigo` implements a **Dynamic Arena Allocator** (`Arena` in `include/minigo/common.h`):
  - Allocates memory in large contiguous chunks (default 64 KB).
  - Allocation is a fast pointer bump: `O(1)`.
  - Releasing compiler state takes a single sweep: `arena_free_all(&arena)`.

### 2.2 Lexer & Automatic Semicolon Insertion (ASI)
- Source file is scanned character-by-character via `lexer_next_token()`.
- Identifiers, integer literals (decimal and hexadecimal `0x`), strings, and operators are recognized.
- **Go ASI Implementation:** When encountering a newline `\n`, if the preceding token was an identifier, literal, `return`, `break`, `continue`, `++`, `--`, or closing delimiter (`)`, `]`, `}`), a synthetic semicolon token (`TOKEN_SEMICOLON`) is emitted before advancing.

### 2.3 Parsing: Precedence Climbing
- Expressions are parsed using **Precedence Climbing** (an efficient form of Pratt parsing):
  - Operator precedence levels (1 to 5) determine binding power.
  - Multiplicative operators (`*`, `/`, `%`) bind tighter than additive (`+`, `-`), which bind tighter than comparisons (`<`, `==`), which bind tighter than boolean connectives (`&&`, `||`).
- Declarations and statements use recursive descent with standard predictive lookahead (`p->current` and `p->peek`).

### 2.4 Semantic Analysis & Type Checking
- Manages an environment stack of `Scope` objects:
  - **Global Scope:** Holds top-level functions, global variables, and built-in signatures (`println`, `print`).
  - **Local Scopes:** Pushed on entering function bodies and compound `{ ... }` blocks; popped on exit.
- Performs:
  - Scope resolution: Ensuring variables are defined before access.
  - Type inference: Determining types for short variable declarations (`x := 10`).
  - Type checking: Validating operands for arithmetic and relational operations.

### 2.5 Code Generation: x86-64 Machine Target
- Emits GNU Assembler (GAS) formatted x86-64 assembly.
- **Stack-Frame Layout:**
  ```text
  High Address
  |-----------------------------------|
  |  Caller Return Address            |
  |-----------------------------------|
  |  Saved %rbp (Base Pointer)        | <- %rbp points here
  |-----------------------------------|
  |  Local Variable 1  (-8(%rbp))     |
  |  Local Variable 2  (-16(%rbp))    |
  |  Local Variable 3  (-24(%rbp))    |
  |  ...                              |
  |  Shadow Space / Alignment Space   | <- %rsp points here
  Low Address
  ```
- **Calling Conventions:**
  - **Windows x64 ABI:** First 4 integer parameters in `%rcx`, `%rdx`, `%r8`, `%r9`, with 32 bytes of shadow space allocated before `call`.
  - **System V AMD64 ABI (Linux / macOS):** First 6 integer parameters in `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`.
  - Return values are passed in `%rax`.

---

## 3. Extending with C++ or Verification

- **C++ Interoperability:** Because all header files in `include/minigo/` use clean C function signatures and standard types, you can link C++ source files (`.cpp`) seamlessly by enclosing `#include "minigo/..."` within `extern "C" { ... }`.
- **Formal Verification:** The AST and parser output can be translated or mirrored into formal definitions in Rocq (Coq) or Lean 4 to prove semantic equivalence.
