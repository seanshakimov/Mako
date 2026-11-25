/*
 * Mako Compiler - Code Generator
 *
 * The code generator walks the AST and produces executable C code.
 * We transpile to C for the demo — this lets us leverage the system's
 * C compiler (gcc/clang) for optimization and native execution, while
 * keeping the demo simple and educational.
 *
 * This is Stage 3 of the compilation pipeline:
 *   Source Code → Lexer → Tokens → Parser → AST → [CODEGEN] → C Code → Binary
 *
 * Future: replace this with LLVM IR generation or a custom bytecode VM.
 */

#ifndef MAKO_CODEGEN_H
#define MAKO_CODEGEN_H

#include "parser.h"

/* ── Code Generator State ────────────────────────────────────────────── */
typedef struct {
    FILE *output;           // Output file/stream for generated C code
    int   indent_level;     // Current indentation depth
    int   had_error;
    char  error_msg[512];
} CodeGen;

/* ── Public API ──────────────────────────────────────────────────────── */

// Initialize the code generator writing to a file
void codegen_init(CodeGen *cg, FILE *output);

// Generate C code from an AST
int codegen_generate(CodeGen *cg, ASTNode *ast);

#endif // MAKO_CODEGEN_H
