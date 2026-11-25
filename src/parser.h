/*
 * Mako Compiler - Parser
 *
 * The parser takes the flat stream of tokens from the lexer and builds
 * an Abstract Syntax Tree (AST) — a tree structure that represents the
 * hierarchical structure of the program.
 *
 * This is Stage 2 of the compilation pipeline:
 *   Source Code → Lexer → Tokens → [PARSER] → AST → CodeGen → Output
 */

#ifndef MAKO_PARSER_H
#define MAKO_PARSER_H

#include "lexer.h"

/* ── AST Node Types ──────────────────────────────────────────────────── */
typedef enum {
    // Program root
    NODE_PROGRAM,

    // Statements
    NODE_VAR_DECL,          // let x = expr;
    NODE_ASSIGNMENT,        // x = expr;
    NODE_IF,                // if (cond) { ... } else { ... }
    NODE_WHILE,             // while (cond) { ... }
    NODE_PRINT,             // print(expr);
    NODE_BLOCK,             // { stmt; stmt; ... }
    NODE_FN_DECL,           // fn name(params) { body }
    NODE_RETURN,            // return expr;
    NODE_EXPR_STMT,         // expression as statement (e.g., function call)
    NODE_FN_CALL,           // name(args)

    // Expressions
    NODE_BINARY_OP,         // expr op expr
    NODE_UNARY_OP,          // !expr, -expr
    NODE_INT_LITERAL,       // 42
    NODE_STRING_LITERAL,    // "hello"
    NODE_BOOL_LITERAL,      // true, false
    NODE_IDENTIFIER,        // variable reference
} NodeType;

/* ── AST Node ────────────────────────────────────────────────────────── */
typedef struct ASTNode {
    NodeType type;
    int      line;          // Source line for error messages

    // Value (for literals and identifiers)
    char value[256];

    // Operator (for binary/unary ops)
    char op[4];

    // Children — flexible structure used differently per node type:
    //   PROGRAM/BLOCK:   children = list of statements
    //   VAR_DECL:        children[0] = initializer expr, value = var name
    //   ASSIGNMENT:      children[0] = rhs expr, value = var name
    //   IF:              children[0] = condition, children[1] = then-block,
    //                    children[2] = else-block (optional)
    //   WHILE:           children[0] = condition, children[1] = body
    //   PRINT:           children[0] = expression to print
    //   BINARY_OP:       children[0] = left, children[1] = right
    //   UNARY_OP:        children[0] = operand
    //   FN_DECL:         value = name, params = param names,
    //                    children[0] = body block
    //   FN_CALL:         value = name, children = arguments
    //   RETURN:          children[0] = return value expr
    struct ASTNode **children;
    int              child_count;
    int              child_capacity;

    // Function parameters (for FN_DECL)
    char **params;
    int    param_count;
} ASTNode;

/* ── Parser State ────────────────────────────────────────────────────── */
typedef struct {
    Token *tokens;
    int    token_count;
    int    pos;             // Current token index
    int    had_error;
    char   error_msg[512];
} Parser;

/* ── Public API ──────────────────────────────────────────────────────── */

// Create a new AST node
ASTNode *ast_new(NodeType type, int line);

// Add a child to a node
void ast_add_child(ASTNode *parent, ASTNode *child);

// Free an entire AST tree
void ast_free(ASTNode *node);

// Initialize the parser with a token array
void parser_init(Parser *parser, Token *tokens, int token_count);

// Parse the full token stream into an AST
ASTNode *parser_parse(Parser *parser);

// Pretty-print the AST (for debugging / educational visualization)
void ast_print(const ASTNode *node, int indent);

#endif // MAKO_PARSER_H
