/*
 * Mako Compiler - Lexer Implementation
 */

#include "lexer.h"

/* ── Helpers ─────────────────────────────────────────────────────────── */

static char peek(Lexer *l) {
    return l->source[l->pos];
}

static char advance(Lexer *l) {
    char c = l->source[l->pos++];
    if (c == '\n') { l->line++; l->col = 1; }
    else { l->col++; }
    return c;
}

static int is_at_end(Lexer *l) {
    return l->source[l->pos] == '\0';
}

static char peek_next(Lexer *l) {
    if (is_at_end(l)) return '\0';
    return l->source[l->pos + 1];
}

static void skip_whitespace_and_comments(Lexer *l) {
    while (!is_at_end(l)) {
        char c = peek(l);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(l);
        } else if (c == '/' && peek_next(l) == '/') {
            // Single-line comment: skip to end of line
            while (!is_at_end(l) && peek(l) != '\n') advance(l);
        } else if (c == '/' && peek_next(l) == '*') {
            // Multi-line comment
            advance(l); advance(l); // skip /*
            while (!is_at_end(l) && !(peek(l) == '*' && peek_next(l) == '/')) {
                advance(l);
            }
            if (!is_at_end(l)) { advance(l); advance(l); } // skip */
        } else {
            break;
        }
    }
}

/* ── Token Emission ──────────────────────────────────────────────────── */

static void emit_token(Lexer *l, TokenType type, const char *value, int line, int col) {
    if (l->token_count >= l->token_capacity) {
        l->token_capacity *= 2;
        l->tokens = realloc(l->tokens, sizeof(Token) * l->token_capacity);
    }
    Token *t = &l->tokens[l->token_count++];
    t->type = type;
    strncpy(t->value, value, 255);
    t->value[255] = '\0';
    t->line = line;
    t->col = col;
}

static void emit_error(Lexer *l, const char *msg, int line, int col) {
    emit_token(l, TOKEN_ERROR, msg, line, col);
}

/* ── Keyword Lookup ──────────────────────────────────────────────────── */

static TokenType check_keyword(const char *word) {
    if (strcmp(word, "let")    == 0) return TOKEN_LET;
    if (strcmp(word, "if")     == 0) return TOKEN_IF;
    if (strcmp(word, "else")   == 0) return TOKEN_ELSE;
    if (strcmp(word, "while")  == 0) return TOKEN_WHILE;
    if (strcmp(word, "fn")     == 0) return TOKEN_FN;
    if (strcmp(word, "return") == 0) return TOKEN_RETURN;
    if (strcmp(word, "print")  == 0) return TOKEN_PRINT;
    if (strcmp(word, "true")   == 0) return TOKEN_TRUE;
    if (strcmp(word, "false")  == 0) return TOKEN_FALSE;
    return TOKEN_IDENTIFIER;
}

/* ── Scanning Routines ───────────────────────────────────────────────── */

static void scan_number(Lexer *l) {
    int start = l->pos;
    int line = l->line, col = l->col;
    while (!is_at_end(l) && isdigit(peek(l))) advance(l);

    int len = l->pos - start;
    char buf[256];
    strncpy(buf, l->source + start, len);
    buf[len] = '\0';
    emit_token(l, TOKEN_INT_LITERAL, buf, line, col);
}

static void scan_identifier(Lexer *l) {
    int start = l->pos;
    int line = l->line, col = l->col;
    while (!is_at_end(l) && (isalnum(peek(l)) || peek(l) == '_')) advance(l);

    int len = l->pos - start;
    char buf[256];
    strncpy(buf, l->source + start, len);
    buf[len] = '\0';

    TokenType type = check_keyword(buf);
    emit_token(l, type, buf, line, col);
}

static void scan_string(Lexer *l) {
    int line = l->line, col = l->col;
    advance(l); // skip opening "
    int start = l->pos;

    while (!is_at_end(l) && peek(l) != '"') {
        if (peek(l) == '\\') advance(l); // skip escaped char
        advance(l);
    }

    if (is_at_end(l)) {
        emit_error(l, "Unterminated string literal", line, col);
        return;
    }

    int len = l->pos - start;
    char buf[256];
    strncpy(buf, l->source + start, len);
    buf[len] = '\0';
    advance(l); // skip closing "

    emit_token(l, TOKEN_STRING_LITERAL, buf, line, col);
}

/* ── Main Tokenizer ──────────────────────────────────────────────────── */

void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->col = 1;
    lexer->token_capacity = 128;
    lexer->token_count = 0;
    lexer->tokens = malloc(sizeof(Token) * lexer->token_capacity);
}

int lexer_tokenize(Lexer *l) {
    while (!is_at_end(l)) {
        skip_whitespace_and_comments(l);
        if (is_at_end(l)) break;

        char c = peek(l);
        int line = l->line, col = l->col;

        // Numbers
        if (isdigit(c)) {
            scan_number(l);
            continue;
        }

        // Identifiers & keywords
        if (isalpha(c) || c == '_') {
            scan_identifier(l);
            continue;
        }

        // Strings
        if (c == '"') {
            scan_string(l);
            continue;
        }

        // Operators and delimiters
        advance(l);
        switch (c) {
            case '+': emit_token(l, TOKEN_PLUS,      "+", line, col); break;
            case '-': emit_token(l, TOKEN_MINUS,     "-", line, col); break;
            case '*': emit_token(l, TOKEN_STAR,      "*", line, col); break;
            case '%': emit_token(l, TOKEN_PERCENT,   "%", line, col); break;
            case '(': emit_token(l, TOKEN_LPAREN,    "(", line, col); break;
            case ')': emit_token(l, TOKEN_RPAREN,    ")", line, col); break;
            case '{': emit_token(l, TOKEN_LBRACE,    "{", line, col); break;
            case '}': emit_token(l, TOKEN_RBRACE,    "}", line, col); break;
            case ';': emit_token(l, TOKEN_SEMICOLON, ";", line, col); break;
            case ',': emit_token(l, TOKEN_COMMA,     ",", line, col); break;

            case '/':
                emit_token(l, TOKEN_SLASH, "/", line, col);
                break;

            case '=':
                if (peek(l) == '=') { advance(l); emit_token(l, TOKEN_EQ,  "==", line, col); }
                else { emit_token(l, TOKEN_ASSIGN, "=", line, col); }
                break;

            case '!':
                if (peek(l) == '=') { advance(l); emit_token(l, TOKEN_NEQ, "!=", line, col); }
                else { emit_token(l, TOKEN_NOT, "!", line, col); }
                break;

            case '<':
                if (peek(l) == '=') { advance(l); emit_token(l, TOKEN_LTE, "<=", line, col); }
                else { emit_token(l, TOKEN_LT, "<", line, col); }
                break;

            case '>':
                if (peek(l) == '=') { advance(l); emit_token(l, TOKEN_GTE, ">=", line, col); }
                else { emit_token(l, TOKEN_GT, ">", line, col); }
                break;

            case '&':
                if (peek(l) == '&') { advance(l); emit_token(l, TOKEN_AND, "&&", line, col); }
                else { emit_error(l, "Unexpected '&' (did you mean '&&'?)", line, col); }
                break;

            case '|':
                if (peek(l) == '|') { advance(l); emit_token(l, TOKEN_OR, "||", line, col); }
                else { emit_error(l, "Unexpected '|' (did you mean '||'?)", line, col); }
                break;

            default: {
                char msg[64];
                snprintf(msg, 64, "Unexpected character '%c'", c);
                emit_error(l, msg, line, col);
                break;
            }
        }
    }

    emit_token(l, TOKEN_EOF, "EOF", l->line, l->col);
    return 1;
}

void lexer_free(Lexer *lexer) {
    if (lexer->tokens) {
        free(lexer->tokens);
        lexer->tokens = NULL;
    }
}

/* ── Debug Utilities ─────────────────────────────────────────────────── */

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_INT_LITERAL:    return "INT";
        case TOKEN_STRING_LITERAL: return "STR";
        case TOKEN_IDENTIFIER:     return "IDENT";
        case TOKEN_LET:            return "LET";
        case TOKEN_IF:             return "IF";
        case TOKEN_ELSE:           return "ELSE";
        case TOKEN_WHILE:          return "WHILE";
        case TOKEN_FN:             return "FN";
        case TOKEN_RETURN:         return "RETURN";
        case TOKEN_PRINT:          return "PRINT";
        case TOKEN_TRUE:           return "TRUE";
        case TOKEN_FALSE:          return "FALSE";
        case TOKEN_PLUS:           return "PLUS";
        case TOKEN_MINUS:          return "MINUS";
        case TOKEN_STAR:           return "STAR";
        case TOKEN_SLASH:          return "SLASH";
        case TOKEN_PERCENT:        return "PERCENT";
        case TOKEN_ASSIGN:         return "ASSIGN";
        case TOKEN_EQ:             return "EQ";
        case TOKEN_NEQ:            return "NEQ";
        case TOKEN_LT:             return "LT";
        case TOKEN_GT:             return "GT";
        case TOKEN_LTE:            return "LTE";
        case TOKEN_GTE:            return "GTE";
        case TOKEN_AND:            return "AND";
        case TOKEN_OR:             return "OR";
        case TOKEN_NOT:            return "NOT";
        case TOKEN_LPAREN:         return "LPAREN";
        case TOKEN_RPAREN:         return "RPAREN";
        case TOKEN_LBRACE:         return "LBRACE";
        case TOKEN_RBRACE:         return "RBRACE";
        case TOKEN_SEMICOLON:      return "SEMICOLON";
        case TOKEN_COMMA:          return "COMMA";
        case TOKEN_EOF:            return "EOF";
        case TOKEN_ERROR:          return "ERROR";
        default:                   return "UNKNOWN";
    }
}

void lexer_dump_tokens(const Lexer *lexer) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  MAKO LEXER — Token Stream                      ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    for (int i = 0; i < lexer->token_count; i++) {
        const Token *t = &lexer->tokens[i];
        printf("║  [%3d:%2d] %-12s │ %s\n", t->line, t->col, token_type_name(t->type), t->value);
    }
    printf("╚══════════════════════════════════════════════════╝\n");
}
