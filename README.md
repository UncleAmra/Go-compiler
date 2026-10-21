# Mini Go Compiler (`minigo`)

## Project Overview & Features
`minigo` is a educational purpose mini version of Go language compiler. It's a statically typed subset of the Go programming language that compiles directly to it's native x86-64 asssembly and links it to the GCC for the native binary machine codes. (for my final project)
 The complete pipeline process is: **Lexer & Parser -> AST -> TAC -> CFG -> SSA -> Optimizations (O1) -> x86-64 Assembly -> GCC Bridge**

 After all the process is done and the native x86-64 assembler is generated. Because the sheer large scale and complexity of lowest level of optimization of the final compilation. I plugged GCC to get the native machine code. Avoiding the final optimization.

 Despite I let GCC do the rest, my `minigo` follows almost every steps and optimization of a modern GO compiler in a simpler version.(lexer, parser, syntax tree, SSA etc).

## How to build:
First make your file. these will print every step output. It must directly print it on your terminal if you run the commands.
```
mingw32-make
.\build\minigo.exe -dump-tac examples/02_arithmetic.go
.\build\minigo.exe -dump-cfg examples/03_control_flow.go
.\build\minigo.exe -dump-ssa examples/03_control_flow.go
.\build\minigo.exe -O1 examples/02_arithmetic.go
.\build\minigo.exe -O1 examples/02_arithmetic.go -S -o output.s; gcc output.s -o program.exe; .\program.exe
```
To see the final native assembler and binary:
```
Get-Content output.s
objdump -d program.exe
```
