/*
 * Mako Compiler - Lexer (Tokenizer)
 * 
 * The lexer breaks raw source code into tokens — the smallest meaningful
 * units of the language (keywords, numbers, operators, identifiers, etc.)
 *
 * This is Stage 1 of the compilation pipeline:
 *   Source Code → [LEXER] → Tokens → Parser → AST → CodeGen → Output
 */

#ifndef MAKO_LEXER_H
#define MAKO_LEXER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Token Types ─────────────────────────────────────────────────────── */
typedef enum {
    // Literals
    TOKEN_INT_LITERAL,      // 42, 100, 0
    TOKEN_STRING_LITERAL,   // "hello"

    // Identifiers & Keywords
    TOKEN_IDENTIFIER,       // myVar, foo, counter
    TOKEN_LET,              // let
    TOKEN_IF,               // if
    TOKEN_ELSE,             // else
    TOKEN_WHILE,            // while
    TOKEN_FN,               // fn
    TOKEN_RETURN,           // return
    TOKEN_PRINT,            // print
    TOKEN_TRUE,             // true
    TOKEN_FALSE,            // false

    // Operators
    TOKEN_PLUS,             // +
    TOKEN_MINUS,            // -
    TOKEN_STAR,             // *
    TOKEN_SLASH,            // /
    TOKEN_PERCENT,          // %
    TOKEN_ASSIGN,           // =
    TOKEN_EQ,               // ==
    TOKEN_NEQ,              // !=
    TOKEN_LT,               // <
    TOKEN_GT,               // >
    TOKEN_LTE,              // <=
    TOKEN_GTE,              // >=
    TOKEN_AND,              // &&
    TOKEN_OR,               // ||
    TOKEN_NOT,              // !

    // Delimiters
    TOKEN_LPAREN,           // (
    TOKEN_RPAREN,           // )
    TOKEN_LBRACE,           // {
    TOKEN_RBRACE,           // }
    TOKEN_SEMICOLON,        // ;
    TOKEN_COMMA,            // ,

    // Special
    TOKEN_EOF,              // End of file
    TOKEN_ERROR             // Lexer error
} TokenType;

/* ── Token Structure ─────────────────────────────────────────────────── */
typedef struct {
    TokenType type;
    char      value[256];   // The raw text of the token
    int       line;         // Source line number (for error reporting)
    int       col;          // Source column number
} Token;

/* ── Lexer State ─────────────────────────────────────────────────────── */
typedef struct {
    const char *source;     // Full source code string
    int         pos;        // Current position in source
    int         line;       // Current line number
    int         col;        // Current column number
    Token      *tokens;     // Dynamic array of produced tokens
    int         token_count;
    int         token_capacity;
} Lexer;

/* ── Public API ──────────────────────────────────────────────────────── */

// Initialize a lexer with source code
void lexer_init(Lexer *lexer, const char *source);

// Tokenize the entire source, populating lexer->tokens
int lexer_tokenize(Lexer *lexer);

// Free all memory owned by the lexer
void lexer_free(Lexer *lexer);

// Get a human-readable name for a token type
const char *token_type_name(TokenType type);

// Print all tokens (for debugging)
void lexer_dump_tokens(const Lexer *lexer);

// Output tokens as JSON array
void lexer_dump_json(const Lexer *lexer, FILE *out);

#endif // MAKO_LEXER_H
