# Mini Go Language Specification

This document defines the formal syntax, semantics, and language features supported by the **Mini Go** compiler.

---

## 1. Lexical Structure

### 1.1 Identifiers & Keywords
Identifiers follow the regex: `[a-zA-Z_][a-zA-Z0-9_]*`.

Reserved keywords:
```
package   import    func      var       const
type      struct    return    if        else
for       break     continue
```

### 1.2 Operators & Punctuation
- **Arithmetic:** `+`, `-`, `*`, `/`, `%`
- **Bitwise:** `&`, `|`, `^`, `<<`, `>>`
- **Logical:** `&&`, `||`, `!`
- **Comparison:** `==`, `!=`, `<`, `<=`, `>`, `>=`
- **Assignment:** `=`, `:=`, `+=`, `-=`
- **Punctuation:** `(`, `)`, `{`, `}`, `[`, `]`, `,`, `.`, `;`, `:`

### 1.3 Automatic Semicolon Insertion (ASI)
In Go, semicolons are formally statement separators. To relieve programmers from writing semicolons manually, the lexer enforces **Automatic Semicolon Insertion (ASI)**:

> If a newline (`\n`) occurs after any of the following tokens:
> 1. An identifier
> 2. An integer, float, or string literal
> 3. The keywords `return`, `break`, or `continue`
> 4. The delimiters `++`, `--`, `)`, `]`, or `}`
> 
> The lexer inserts a synthetic `TOKEN_SEMICOLON` at the position of the newline.

---

## 2. Type System

Mini Go supports a statically typed, monomorphic type system:
1. **Primitive Types:**
   - `int`: 64-bit signed integer (maps to x86-64 quadword `%rax`).
   - `bool`: Boolean value (`true` = 1, `false` = 0).
   - `string`: Read-only UTF-8 string pointer.
   - `void`: Unit / empty return type.
2. **Composite Types:**
   - **Pointer Type:** `*T` represents an address pointing to a value of type `T`.
   - **Struct Type:** `type <Name> struct { field1 type1; field2 type2 }` represents a contiguous product type laid out sequentially in memory.

---

## 3. Grammar (EBNF)

```ebnf
Program        ::= "package" Identifier ";" { TopLevelDecl }
TopLevelDecl   ::= FuncDecl | VarDecl | TypeDecl

FuncDecl       ::= "func" Identifier "(" [ ParamList ] ")" [ Type ] Block
ParamList      ::= Param { "," Param }
Param          ::= Identifier Type

VarDecl        ::= "var" Identifier [ Type ] [ "=" Expr ] ";"
TypeDecl       ::= "type" Identifier "struct" "{" { FieldDecl ";" } "}" ";"
FieldDecl      ::= Identifier Type

Block          ::= "{" { Statement } "}"
Statement      ::= VarDecl
                 | ShortVarDecl
                 | AssignStmt
                 | ReturnStmt
                 | IfStmt
                 | ForStmt
                 | ExprStmt
                 | Block

ShortVarDecl   ::= Identifier ":=" Expr ";"
AssignStmt     ::= Identifier "=" Expr ";"
ReturnStmt     ::= "return" [ Expr ] ";"
IfStmt         ::= "if" Expr Block [ "else" ( IfStmt | Block ) ]
ForStmt        ::= "for" [ Expr ] Block
ExprStmt       ::= Expr ";"

Expr           ::= LogicOrExpr
LogicOrExpr    ::= LogicAndExpr { "||" LogicAndExpr }
LogicAndExpr   ::= RelExpr { "&&" RelExpr }
RelExpr        ::= AddExpr { ( "==" | "!=" | "<" | "<=" | ">" | ">=" ) AddExpr }
AddExpr        ::= MulExpr { ( "+" | "-" | "|" | "^" ) MulExpr }
MulExpr        ::= UnaryExpr { ( "*" | "/" | "%" | "<<" | ">>" | "&" ) UnaryExpr }
UnaryExpr      ::= ( "-" | "!" | "*" | "&" ) UnaryExpr | PrimaryExpr
PrimaryExpr    ::= IntLiteral
                 | StringLiteral
                 | Identifier [ "(" [ ArgList ] ")" ]
                 | "(" Expr ")"
ArgList        ::= Expr { "," Expr }
```

---

## 4. Execution Semantics
- Program execution starts at `func main()`.
- Return code of `main` becomes the process exit status.
- Built-in functions:
  - `print(x)` / `println(x)`: Outputs integer or string to standard output.
