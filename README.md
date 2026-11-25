# 🚀 Mako Compiler

**Mako** is an AI-assisted compiler and learning platform that helps students understand how compilers work by making every stage of compilation visible and interactive.

## ✨ Features (Demo v0.1)

- **Interactive Web Playground**: Real-time browser-based editor with live token stream, AST visualizer, generated C code, and terminal execution
- **Full compilation pipeline**: Lexer → Parser → AST → Code Generator
- **Custom language**: Variables, math, if/else, while loops, functions, recursion
- **Pipeline visualization**: See tokens, AST, and generated C code at each stage (`--tokens`, `--ast`, `--emit-c`, `--json`)
- **Transpiles to C**: Leverages system C compiler for native performance

## 🏗️ Building & Running

```bash
# Build the Mako compiler binary
make

# Launch the interactive Web Playground
make playground
# Then open http://localhost:8080 in your browser!
```

## 📖 CLI Usage

```bash
# Compile and run a Mako program
./mako examples/hello.kv

# See the token stream (lexer output)
./mako examples/hello.kv --tokens

# See the abstract syntax tree
./mako examples/hello.kv --ast

# See the generated C code
./mako examples/hello.kv --emit-c

# Compile to a named binary
./mako examples/hello.kv -o myprogram
```

## 📝 Language Quick Reference

```
// Variables
let x = 42;
let name = "hello";

// Arithmetic
let result = (x + 10) * 2 - 5;

// Control flow
if (x > 10) {
    print("big");
} else {
    print("small");
}

// Loops
let i = 0;
while (i < 10) {
    print(i);
    i = i + 1;
}

// Functions
fn add(a, b) {
    return a + b;
}
print(add(3, 4));  // 7
```

## 🗺️ Roadmap

- [ ] **v0.2** — Web playground with live compilation
- [ ] **v0.3** — AI layer for code optimization suggestions
- [ ] **v0.4** — LLVM backend for real machine code generation
- [ ] **v1.0** — Full platform launch

## 📁 Project Structure

```
mako/
├── src/
│   ├── main.c          # Compiler driver & CLI
│   ├── lexer.h/c       # Stage 1: Tokenizer
│   ├── parser.h/c      # Stage 2: Parser & AST
│   └── codegen.h/c     # Stage 3: C code generator
├── examples/
│   ├── hello.kv        # Hello world
│   ├── fibonacci.kv    # Recursive fibonacci
│   ├── fizzbuzz.kv     # FizzBuzz challenge
│   └── calculator.kv   # Functions & math
├── Makefile
└── README.md
```
