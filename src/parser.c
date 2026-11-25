/*
 * Mako Compiler - Parser Implementation
 *
 * Recursive descent parser implementing a simple precedence-climbing
 * expression parser with statement-level constructs.
 */

#include "parser.h"

/* ── AST Construction ────────────────────────────────────────────────── */

ASTNode *ast_new(NodeType type, int line) {
    ASTNode *node = calloc(1, sizeof(ASTNode));
    node->type = type;
    node->line = line;
    node->child_capacity = 4;
    node->children = malloc(sizeof(ASTNode *) * node->child_capacity);
    node->child_count = 0;
    node->params = NULL;
    node->param_count = 0;
    return node;
}

void ast_add_child(ASTNode *parent, ASTNode *child) {
    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity *= 2;
        parent->children = realloc(parent->children, sizeof(ASTNode *) * parent->child_capacity);
    }
    parent->children[parent->child_count++] = child;
}

void ast_free(ASTNode *node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++) {
        ast_free(node->children[i]);
    }
    free(node->children);
    if (node->params) {
        for (int i = 0; i < node->param_count; i++) free(node->params[i]);
        free(node->params);
    }
    free(node);
}

/* ── Parser Helpers ──────────────────────────────────────────────────── */

static Token *current(Parser *p) {
    return &p->tokens[p->pos];
}

static Token *previous(Parser *p) {
    return &p->tokens[p->pos - 1];
}

static int check(Parser *p, TokenType type) {
    return current(p)->type == type;
}

static int match(Parser *p, TokenType type) {
    if (check(p, type)) {
        p->pos++;
        return 1;
    }
    return 0;
}

static void error(Parser *p, const char *msg) {
    if (p->had_error) return; // only report first error
    p->had_error = 1;
    Token *t = current(p);
    snprintf(p->error_msg, 512, "Parse error at line %d, col %d near '%s': %s",
             t->line, t->col, t->value, msg);
    fprintf(stderr, "❌ %s\n", p->error_msg);
}

static Token *expect(Parser *p, TokenType type, const char *msg) {
    if (check(p, type)) {
        p->pos++;
        return previous(p);
    }
    error(p, msg);
    return NULL;
}

/* ── Forward Declarations ────────────────────────────────────────────── */
static ASTNode *parse_statement(Parser *p);
static ASTNode *parse_expression(Parser *p);
static ASTNode *parse_block(Parser *p);

/* ── Expression Parsing (Precedence Climbing) ────────────────────────── */

static ASTNode *parse_primary(Parser *p) {
    int line = current(p)->line;

    // Integer literal
    if (match(p, TOKEN_INT_LITERAL)) {
        ASTNode *node = ast_new(NODE_INT_LITERAL, line);
        strcpy(node->value, previous(p)->value);
        return node;
    }

    // String literal
    if (match(p, TOKEN_STRING_LITERAL)) {
        ASTNode *node = ast_new(NODE_STRING_LITERAL, line);
        strcpy(node->value, previous(p)->value);
        return node;
    }

    // Boolean literal
    if (match(p, TOKEN_TRUE) || match(p, TOKEN_FALSE)) {
        ASTNode *node = ast_new(NODE_BOOL_LITERAL, line);
        strcpy(node->value, previous(p)->value);
        return node;
    }

    // Identifier or function call
    if (match(p, TOKEN_IDENTIFIER)) {
        char name[256];
        strcpy(name, previous(p)->value);

        // Check for function call: ident(...)
        if (check(p, TOKEN_LPAREN)) {
            p->pos++; // consume (
            ASTNode *call = ast_new(NODE_FN_CALL, line);
            strcpy(call->value, name);

            if (!check(p, TOKEN_RPAREN)) {
                ast_add_child(call, parse_expression(p));
                while (match(p, TOKEN_COMMA)) {
                    ast_add_child(call, parse_expression(p));
                }
            }
            expect(p, TOKEN_RPAREN, "Expected ')' after function arguments");
            return call;
        }

        ASTNode *node = ast_new(NODE_IDENTIFIER, line);
        strcpy(node->value, name);
        return node;
    }

    // Parenthesized expression
    if (match(p, TOKEN_LPAREN)) {
        ASTNode *expr = parse_expression(p);
        expect(p, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }

    error(p, "Expected expression");
    return ast_new(NODE_INT_LITERAL, line); // dummy node to continue
}

static ASTNode *parse_unary(Parser *p) {
    int line = current(p)->line;

    if (match(p, TOKEN_MINUS)) {
        ASTNode *node = ast_new(NODE_UNARY_OP, line);
        strcpy(node->op, "-");
        ast_add_child(node, parse_unary(p));
        return node;
    }
    if (match(p, TOKEN_NOT)) {
        ASTNode *node = ast_new(NODE_UNARY_OP, line);
        strcpy(node->op, "!");
        ast_add_child(node, parse_unary(p));
        return node;
    }

    return parse_primary(p);
}

static ASTNode *parse_factor(Parser *p) {
    ASTNode *left = parse_unary(p);

    while (check(p, TOKEN_STAR) || check(p, TOKEN_SLASH) || check(p, TOKEN_PERCENT)) {
        int line = current(p)->line;
        char op[4];
        strcpy(op, current(p)->value);
        p->pos++;

        ASTNode *node = ast_new(NODE_BINARY_OP, line);
        strcpy(node->op, op);
        ast_add_child(node, left);
        ast_add_child(node, parse_unary(p));
        left = node;
    }
    return left;
}

static ASTNode *parse_term(Parser *p) {
    ASTNode *left = parse_factor(p);

    while (check(p, TOKEN_PLUS) || check(p, TOKEN_MINUS)) {
        int line = current(p)->line;
        char op[4];
        strcpy(op, current(p)->value);
        p->pos++;

        ASTNode *node = ast_new(NODE_BINARY_OP, line);
        strcpy(node->op, op);
        ast_add_child(node, left);
        ast_add_child(node, parse_factor(p));
        left = node;
    }
    return left;
}

static ASTNode *parse_comparison(Parser *p) {
    ASTNode *left = parse_term(p);

    while (check(p, TOKEN_LT) || check(p, TOKEN_GT) ||
           check(p, TOKEN_LTE) || check(p, TOKEN_GTE)) {
        int line = current(p)->line;
        char op[4];
        strcpy(op, current(p)->value);
        p->pos++;

        ASTNode *node = ast_new(NODE_BINARY_OP, line);
        strcpy(node->op, op);
        ast_add_child(node, left);
        ast_add_child(node, parse_term(p));
        left = node;
    }
    return left;
}

static ASTNode *parse_equality(Parser *p) {
    ASTNode *left = parse_comparison(p);

    while (check(p, TOKEN_EQ) || check(p, TOKEN_NEQ)) {
        int line = current(p)->line;
        char op[4];
        strcpy(op, current(p)->value);
        p->pos++;

        ASTNode *node = ast_new(NODE_BINARY_OP, line);
        strcpy(node->op, op);
        ast_add_child(node, left);
        ast_add_child(node, parse_comparison(p));
        left = node;
    }
    return left;
}

static ASTNode *parse_logical_and(Parser *p) {
    ASTNode *left = parse_equality(p);

    while (match(p, TOKEN_AND)) {
        int line = previous(p)->line;
        ASTNode *node = ast_new(NODE_BINARY_OP, line);
        strcpy(node->op, "&&");
        ast_add_child(node, left);
        ast_add_child(node, parse_equality(p));
        left = node;
    }
    return left;
}

static ASTNode *parse_expression(Parser *p) {
    ASTNode *left = parse_logical_and(p);

    while (match(p, TOKEN_OR)) {
        int line = previous(p)->line;
        ASTNode *node = ast_new(NODE_BINARY_OP, line);
        strcpy(node->op, "||");
        ast_add_child(node, left);
        ast_add_child(node, parse_logical_and(p));
        left = node;
    }
    return left;
}

/* ── Statement Parsing ───────────────────────────────────────────────── */

static ASTNode *parse_block(Parser *p) {
    int line = current(p)->line;
    expect(p, TOKEN_LBRACE, "Expected '{'");

    ASTNode *block = ast_new(NODE_BLOCK, line);
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        ASTNode *stmt = parse_statement(p);
        if (stmt) ast_add_child(block, stmt);
        if (p->had_error) break;
    }
    expect(p, TOKEN_RBRACE, "Expected '}'");
    return block;
}

static ASTNode *parse_var_decl(Parser *p) {
    int line = current(p)->line;
    p->pos++; // consume 'let'

    Token *name = expect(p, TOKEN_IDENTIFIER, "Expected variable name after 'let'");
    if (!name) return NULL;

    ASTNode *node = ast_new(NODE_VAR_DECL, line);
    strcpy(node->value, name->value);

    expect(p, TOKEN_ASSIGN, "Expected '=' in variable declaration");
    ast_add_child(node, parse_expression(p));
    expect(p, TOKEN_SEMICOLON, "Expected ';' after variable declaration");
    return node;
}

static ASTNode *parse_if(Parser *p) {
    int line = current(p)->line;
    p->pos++; // consume 'if'

    expect(p, TOKEN_LPAREN, "Expected '(' after 'if'");
    ASTNode *node = ast_new(NODE_IF, line);
    ast_add_child(node, parse_expression(p));  // condition
    expect(p, TOKEN_RPAREN, "Expected ')' after if condition");

    ast_add_child(node, parse_block(p));  // then-block

    if (match(p, TOKEN_ELSE)) {
        if (check(p, TOKEN_IF)) {
            // else if — parse as a block containing a single if
            ASTNode *else_block = ast_new(NODE_BLOCK, current(p)->line);
            ast_add_child(else_block, parse_if(p));
            ast_add_child(node, else_block);
        } else {
            ast_add_child(node, parse_block(p));  // else-block
        }
    }
    return node;
}

static ASTNode *parse_while(Parser *p) {
    int line = current(p)->line;
    p->pos++; // consume 'while'

    expect(p, TOKEN_LPAREN, "Expected '(' after 'while'");
    ASTNode *node = ast_new(NODE_WHILE, line);
    ast_add_child(node, parse_expression(p));  // condition
    expect(p, TOKEN_RPAREN, "Expected ')' after while condition");
    ast_add_child(node, parse_block(p));  // body
    return node;
}

static ASTNode *parse_print(Parser *p) {
    int line = current(p)->line;
    p->pos++; // consume 'print'

    expect(p, TOKEN_LPAREN, "Expected '(' after 'print'");
    ASTNode *node = ast_new(NODE_PRINT, line);
    ast_add_child(node, parse_expression(p));
    expect(p, TOKEN_RPAREN, "Expected ')' after print expression");
    expect(p, TOKEN_SEMICOLON, "Expected ';' after print statement");
    return node;
}

static ASTNode *parse_fn_decl(Parser *p) {
    int line = current(p)->line;
    p->pos++; // consume 'fn'

    Token *name = expect(p, TOKEN_IDENTIFIER, "Expected function name");
    if (!name) return NULL;

    ASTNode *node = ast_new(NODE_FN_DECL, line);
    strcpy(node->value, name->value);

    expect(p, TOKEN_LPAREN, "Expected '(' after function name");

    // Parse parameter list
    int param_cap = 4;
    node->params = malloc(sizeof(char *) * param_cap);
    node->param_count = 0;

    if (!check(p, TOKEN_RPAREN)) {
        Token *param = expect(p, TOKEN_IDENTIFIER, "Expected parameter name");
        if (param) {
            if (node->param_count >= param_cap) {
                param_cap *= 2;
                node->params = realloc(node->params, sizeof(char *) * param_cap);
            }
            node->params[node->param_count] = strdup(param->value);
            node->param_count++;
        }
        while (match(p, TOKEN_COMMA)) {
            param = expect(p, TOKEN_IDENTIFIER, "Expected parameter name");
            if (param) {
                if (node->param_count >= param_cap) {
                    param_cap *= 2;
                    node->params = realloc(node->params, sizeof(char *) * param_cap);
                }
                node->params[node->param_count] = strdup(param->value);
                node->param_count++;
            }
        }
    }
    expect(p, TOKEN_RPAREN, "Expected ')' after parameters");

    ast_add_child(node, parse_block(p));  // body
    return node;
}

static ASTNode *parse_return(Parser *p) {
    int line = current(p)->line;
    p->pos++; // consume 'return'

    ASTNode *node = ast_new(NODE_RETURN, line);
    if (!check(p, TOKEN_SEMICOLON)) {
        ast_add_child(node, parse_expression(p));
    }
    expect(p, TOKEN_SEMICOLON, "Expected ';' after return statement");
    return node;
}

static ASTNode *parse_statement(Parser *p) {
    if (p->had_error) return NULL;

    switch (current(p)->type) {
        case TOKEN_LET:    return parse_var_decl(p);
        case TOKEN_IF:     return parse_if(p);
        case TOKEN_WHILE:  return parse_while(p);
        case TOKEN_PRINT:  return parse_print(p);
        case TOKEN_FN:     return parse_fn_decl(p);
        case TOKEN_RETURN: return parse_return(p);
        case TOKEN_LBRACE: return parse_block(p);
        default: break;
    }

    // Assignment or expression statement
    if (check(p, TOKEN_IDENTIFIER)) {
        // Peek ahead for assignment
        if (p->pos + 1 < p->token_count && p->tokens[p->pos + 1].type == TOKEN_ASSIGN) {
            int line = current(p)->line;
            char name[256];
            strcpy(name, current(p)->value);
            p->pos += 2; // skip ident and =

            ASTNode *node = ast_new(NODE_ASSIGNMENT, line);
            strcpy(node->value, name);
            ast_add_child(node, parse_expression(p));
            expect(p, TOKEN_SEMICOLON, "Expected ';' after assignment");
            return node;
        }
    }

    // Expression statement (e.g., function call)
    int line = current(p)->line;
    ASTNode *expr = parse_expression(p);
    ASTNode *stmt = ast_new(NODE_EXPR_STMT, line);
    ast_add_child(stmt, expr);
    expect(p, TOKEN_SEMICOLON, "Expected ';' after expression");
    return stmt;
}

/* ── Top-Level Parser ────────────────────────────────────────────────── */

void parser_init(Parser *parser, Token *tokens, int token_count) {
    parser->tokens = tokens;
    parser->token_count = token_count;
    parser->pos = 0;
    parser->had_error = 0;
    parser->error_msg[0] = '\0';
}

ASTNode *parser_parse(Parser *parser) {
    ASTNode *program = ast_new(NODE_PROGRAM, 1);

    while (!check(parser, TOKEN_EOF)) {
        ASTNode *stmt = parse_statement(parser);
        if (stmt) ast_add_child(program, stmt);
        if (parser->had_error) break;
    }
    return program;
}

/* ── AST Pretty Printer ─────────────────────────────────────────────── */

const char *ast_node_type_name(NodeType type) {
    switch (type) {
        case NODE_PROGRAM:        return "Program";
        case NODE_VAR_DECL:       return "VarDecl";
        case NODE_ASSIGNMENT:     return "Assign";
        case NODE_IF:             return "If";
        case NODE_WHILE:          return "While";
        case NODE_PRINT:          return "Print";
        case NODE_BLOCK:          return "Block";
        case NODE_FN_DECL:        return "FnDecl";
        case NODE_RETURN:         return "Return";
        case NODE_EXPR_STMT:      return "ExprStmt";
        case NODE_FN_CALL:        return "FnCall";
        case NODE_BINARY_OP:      return "BinOp";
        case NODE_UNARY_OP:       return "UnaryOp";
        case NODE_INT_LITERAL:    return "Int";
        case NODE_STRING_LITERAL: return "String";
        case NODE_BOOL_LITERAL:   return "Bool";
        case NODE_IDENTIFIER:     return "Ident";
        default:                  return "Unknown";
    }
}

void ast_print(const ASTNode *node, int indent) {
    if (!node) return;

    for (int i = 0; i < indent; i++) printf("  ");

    printf("├─ %s", ast_node_type_name(node->type));
    if (node->value[0]) printf(" \"%s\"", node->value);
    if (node->op[0])    printf(" [%s]", node->op);
    if (node->param_count > 0) {
        printf(" (params:");
        for (int i = 0; i < node->param_count; i++) printf(" %s", node->params[i]);
        printf(")");
    }
    printf("\n");

    for (int i = 0; i < node->child_count; i++) {
        ast_print(node->children[i], indent + 1);
    }
}

void ast_dump_json(const ASTNode *node, FILE *out) {
    if (!node) {
        fprintf(out, "null");
        return;
    }
    fprintf(out, "{\"type\": \"%s\", \"line\": %d", ast_node_type_name(node->type), node->line);
    if (node->value[0]) {
        fprintf(out, ", \"value\": \"");
        for (const char *p = node->value; *p; p++) {
            if (*p == '"') fprintf(out, "\\\"");
            else if (*p == '\\') fprintf(out, "\\\\");
            else if (*p == '\n') fprintf(out, "\\n");
            else if (*p == '\r') fprintf(out, "\\r");
            else if (*p == '\t') fprintf(out, "\\t");
            else fputc(*p, out);
        }
        fprintf(out, "\"");
    }
    if (node->op[0]) {
        fprintf(out, ", \"op\": \"%s\"", node->op);
    }
    if (node->param_count > 0) {
        fprintf(out, ", \"params\": [");
        for (int i = 0; i < node->param_count; i++) {
            fprintf(out, "\"%s\"%s", node->params[i], (i + 1 < node->param_count) ? ", " : "");
        }
        fprintf(out, "]");
    }
    if (node->child_count > 0) {
        fprintf(out, ", \"children\": [");
        for (int i = 0; i < node->child_count; i++) {
            ast_dump_json(node->children[i], out);
            if (i + 1 < node->child_count) fprintf(out, ", ");
        }
        fprintf(out, "]");
    }
    fprintf(out, "}");
}
