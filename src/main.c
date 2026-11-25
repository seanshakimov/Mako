/*
 * ╔═══════════════════════════════════════════════════════════════════╗
 * ║                     MAKO COMPILER v0.1.0                        ║
 * ║          AI-Assisted Compiler & Learning Platform               ║
 * ╚═══════════════════════════════════════════════════════════════════╝
 *
 * Usage:
 *   mako <source.kv>                   Compile & run
 *   mako <source.kv> --tokens          Show token stream
 *   mako <source.kv> --ast             Show AST
 *   mako <source.kv> --emit-c          Show generated C code
 *   mako <source.kv> -o <output>       Compile to binary
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "codegen.h"

#define MAKO_VERSION "0.1.0"

/* ── File Reading ────────────────────────────────────────────────────── */

static char *read_source_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open file '%s'\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

/* ── Banner ──────────────────────────────────────────────────────────── */

static void print_banner(void) {
    printf("\n");
    printf("  ███╗   ███╗ █████╗ ██╗  ██╗ ██████╗ \n");
    printf("  ████╗ ████║██╔══██╗██║ ██╔╝██╔═══██╗\n");
    printf("  ██╔████╔██║███████║█████╔╝ ██║   ██║\n");
    printf("  ██║╚██╔╝██║██╔══██║██╔═██╗ ██║   ██║\n");
    printf("  ██║ ╚═╝ ██║██║  ██║██║  ██╗╚██████╔╝\n");
    printf("  ╚═╝     ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝\n");
    printf("  Compiler v%s\n\n", MAKO_VERSION);
}

static void print_usage(void) {
    print_banner();
    printf("  Usage:\n");
    printf("    mako <file.mako>              Compile & run a Mako program\n");
    printf("    mako <file.mako> --tokens     Show the token stream (lexer output)\n");
    printf("    mako <file.mako> --ast        Show the abstract syntax tree\n");
    printf("    mako <file.mako> --emit-c     Show the generated C code\n");
    printf("    mako <file.mako> --json       Output compilation stages as JSON\n");
    printf("    mako <file.mako> -o <out>     Compile to a named binary\n");
    printf("    mako --version                Show version\n");
    printf("    mako --help                   Show this help\n\n");
}

static void print_escaped_json_string(const char *str, FILE *out) {
    if (!str) return;
    for (const char *p = str; *p; p++) {
        if (*p == '"') fprintf(out, "\\\"");
        else if (*p == '\\') fprintf(out, "\\\\");
        else if (*p == '\n') fprintf(out, "\\n");
        else if (*p == '\r') fprintf(out, "\\r");
        else if (*p == '\t') fprintf(out, "\\t");
        else fputc(*p, out);
    }
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "--version") == 0) {
        printf("mako %s\n", MAKO_VERSION);
        return 0;
    }
    if (strcmp(argv[1], "--help") == 0) {
        print_usage();
        return 0;
    }

    // Parse flags
    const char *source_file = argv[1];
    int show_tokens = 0, show_ast = 0, show_c = 0, show_json = 0;
    const char *output_name = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0)      show_tokens = 1;
        else if (strcmp(argv[i], "--ast") == 0)     show_ast = 1;
        else if (strcmp(argv[i], "--emit-c") == 0)  show_c = 1;
        else if (strcmp(argv[i], "--json") == 0)    show_json = 1;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_name = argv[++i];
        }
    }

    // Read source
    char *source = read_source_file(source_file);
    if (!source) {
        if (show_json) {
            printf("{\"success\": false, \"stage\": \"init\", \"error\": \"Cannot read source file\"}\n");
            return 0;
        }
        return 1;
    }

    if (!show_json) {
        print_banner();
        printf("  📂 Compiling: %s\n\n", source_file);
    }

    // ── STAGE 1: Lexing ──────────────────────────────────────────────
    if (!show_json) printf("  ⚡ Stage 1: Lexing (tokenization)...\n");
    Lexer lexer;
    lexer_init(&lexer, source);
    lexer_tokenize(&lexer);

    // Check for lexer errors
    for (int i = 0; i < lexer.token_count; i++) {
        if (lexer.tokens[i].type == TOKEN_ERROR) {
            if (show_json) {
                printf("{\"success\": false, \"stage\": \"lexer\", \"error\": \"Line %d: ", lexer.tokens[i].line);
                print_escaped_json_string(lexer.tokens[i].value, stdout);
                printf("\", \"tokens\": ");
                lexer_dump_json(&lexer, stdout);
                printf("}\n");
                lexer_free(&lexer);
                free(source);
                return 0;
            }
            fprintf(stderr, "  ❌ Lexer error at line %d: %s\n",
                    lexer.tokens[i].line, lexer.tokens[i].value);
            lexer_free(&lexer);
            free(source);
            return 1;
        }
    }
    if (!show_json) printf("  ✅ Lexed %d tokens\n", lexer.token_count);

    if (show_tokens && !show_json) {
        printf("\n");
        lexer_dump_tokens(&lexer);
        printf("\n");
    }

    // ── STAGE 2: Parsing ─────────────────────────────────────────────
    if (!show_json) printf("  ⚡ Stage 2: Parsing (building AST)...\n");
    Parser parser;
    parser_init(&parser, lexer.tokens, lexer.token_count);
    ASTNode *ast = parser_parse(&parser);

    if (parser.had_error) {
        if (show_json) {
            printf("{\"success\": false, \"stage\": \"parser\", \"error\": \"");
            print_escaped_json_string(parser.error_msg, stdout);
            printf("\", \"tokens\": ");
            lexer_dump_json(&lexer, stdout);
            printf("}\n");
            ast_free(ast);
            lexer_free(&lexer);
            free(source);
            return 0;
        }
        fprintf(stderr, "  ❌ %s\n", parser.error_msg);
        ast_free(ast);
        lexer_free(&lexer);
        free(source);
        return 1;
    }
    if (!show_json) printf("  ✅ AST built (%d top-level nodes)\n", ast->child_count);

    if (show_ast && !show_json) {
        printf("\n  🌳 Abstract Syntax Tree:\n\n");
        ast_print(ast, 2);
        printf("\n");
    }

    // ── STAGE 3: Code Generation ─────────────────────────────────────
    if (!show_json) printf("  ⚡ Stage 3: Code generation (Mako → C)...\n");

    // Write generated C to a temp file
    char c_path[512];
    snprintf(c_path, sizeof(c_path), "/tmp/mako_output.c");
    FILE *c_out = fopen(c_path, "w");
    if (!c_out) {
        if (show_json) {
            printf("{\"success\": false, \"stage\": \"codegen\", \"error\": \"Cannot create temp file for C output\"}\n");
            ast_free(ast);
            lexer_free(&lexer);
            free(source);
            return 0;
        }
        fprintf(stderr, "  ❌ Cannot create temp file for C output\n");
        ast_free(ast);
        lexer_free(&lexer);
        free(source);
        return 1;
    }

    CodeGen cg;
    codegen_init(&cg, c_out);
    codegen_generate(&cg, ast);
    fclose(c_out);

    if (cg.had_error) {
        if (show_json) {
            printf("{\"success\": false, \"stage\": \"codegen\", \"error\": \"Code generation failed\", \"tokens\": ");
            lexer_dump_json(&lexer, stdout);
            printf(", \"ast\": ");
            ast_dump_json(ast, stdout);
            printf("}\n");
            ast_free(ast);
            lexer_free(&lexer);
            free(source);
            return 0;
        }
        fprintf(stderr, "  ❌ Code generation failed\n");
        ast_free(ast);
        lexer_free(&lexer);
        free(source);
        return 1;
    }
    if (!show_json) printf("  ✅ C code generated\n");

    // Read generated C for show_c or show_json
    char *c_code = read_source_file(c_path);

    if (show_c && !show_json && c_code) {
        printf("\n  📄 Generated C Code:\n");
        printf("  ─────────────────────────────────────────\n");
        printf("%s", c_code);
        printf("  ─────────────────────────────────────────\n\n");
    }

    // ── STAGE 4: Compile C → Binary ─────────────────────────────────
    if (!show_json) printf("  ⚡ Stage 4: Compiling C → native binary...\n");

    char bin_path[512];
    if (output_name) {
        snprintf(bin_path, sizeof(bin_path), "%s", output_name);
    } else {
        snprintf(bin_path, sizeof(bin_path), "/tmp/mako_program");
    }

    char compile_cmd[1024];
    snprintf(compile_cmd, sizeof(compile_cmd), "cc -O2 -o %s %s > /tmp/mako_compile.log 2>&1", bin_path, c_path);
    int result = system(compile_cmd);
    if (result != 0) {
        if (show_json) {
            char *compile_err = read_source_file("/tmp/mako_compile.log");
            printf("{\"success\": false, \"stage\": \"compile\", \"error\": \"");
            print_escaped_json_string(compile_err ? compile_err : "C compilation failed", stdout);
            printf("\", \"tokens\": ");
            lexer_dump_json(&lexer, stdout);
            printf(", \"ast\": ");
            ast_dump_json(ast, stdout);
            printf(", \"c_code\": \"");
            print_escaped_json_string(c_code, stdout);
            printf("\"}\n");
            if (compile_err) free(compile_err);
            if (c_code) free(c_code);
            ast_free(ast);
            lexer_free(&lexer);
            free(source);
            return 0;
        }
        fprintf(stderr, "  ❌ C compilation failed (exit code %d)\n", result);
        if (c_code) free(c_code);
        ast_free(ast);
        lexer_free(&lexer);
        free(source);
        return 1;
    }
    if (!show_json) printf("  ✅ Binary compiled: %s\n\n", bin_path);

    // ── STAGE 5: Run ─────────────────────────────────────────────────
    if (show_json) {
        char run_cmd[512];
        snprintf(run_cmd, sizeof(run_cmd), "%s > /tmp/mako_run.log 2>&1", bin_path);
        int run_result = system(run_cmd);
        char *run_output = read_source_file("/tmp/mako_run.log");

        printf("{\"success\": true, \"stage\": \"complete\", \"tokens\": ");
        lexer_dump_json(&lexer, stdout);
        printf(", \"ast\": ");
        ast_dump_json(ast, stdout);
        printf(", \"c_code\": \"");
        print_escaped_json_string(c_code, stdout);
        printf("\", \"output\": \"");
        print_escaped_json_string(run_output ? run_output : "", stdout);
        printf("\", \"exit_code\": %d}\n", run_result);

        if (run_output) free(run_output);
    } else if (!output_name) {
        printf("  🚀 Running program:\n");
        printf("  ═══════════════════════════════════════════\n");
        char run_cmd[512];
        snprintf(run_cmd, sizeof(run_cmd), "%s", bin_path);
        int run_result = system(run_cmd);
        printf("  ═══════════════════════════════════════════\n");
        printf("  Exit code: %d\n\n", run_result);
    }

    // Cleanup
    if (c_code) free(c_code);
    ast_free(ast);
    lexer_free(&lexer);
    free(source);
    return 0;
}
