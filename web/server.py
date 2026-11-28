#!/usr/bin/env python3
"""
Mako Compiler - Web Playground Backend Server with Real-Time AI Watcher
Powers the interactive playground and AI Code Companion.
"""

import os
import sys
import re
import json
import tempfile
import subprocess
import mimetypes
import urllib.request
import urllib.error
from http.server import HTTPServer, BaseHTTPRequestHandler
from pathlib import Path

# Paths
ROOT_DIR = Path(__file__).resolve().parent.parent
STATIC_DIR = Path(__file__).resolve().parent / "static"
EXAMPLES_DIR = ROOT_DIR / "examples"
MAKO_BIN = ROOT_DIR / "mako"

PORT = int(os.environ.get("PORT", 8080))
DEFAULT_GEMINI_KEY = os.environ.get("GEMINI_API_KEY", "")


class MakoRequestHandler(BaseHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()

    def do_GET(self):
        url_path = self.path.split("?")[0]

        if url_path == "/api/examples":
            self.handle_get_examples()
            return

        if url_path == "/api/ai/status":
            self.send_json(200, {
                "has_env_key": bool(DEFAULT_GEMINI_KEY),
                "model": "gemini-1.5-flash"
            })
            return

        # Static file serving
        if url_path in ("", "/"):
            file_path = STATIC_DIR / "index.html"
        else:
            safe_rel = url_path.lstrip("/")
            file_path = (STATIC_DIR / safe_rel).resolve()
            if not str(file_path).startswith(str(STATIC_DIR)):
                self.send_error(403, "Access denied")
                return

        if file_path.is_file():
            mime_type, _ = mimetypes.guess_type(str(file_path))
            mime_type = mime_type or "application/octet-stream"
            try:
                with open(file_path, "rb") as f:
                    content = f.read()
                self.send_response(200)
                self.send_header("Content-Type", mime_type)
                self.send_header("Content-Length", str(len(content)))
                self.end_headers()
                self.wfile.write(content)
            except Exception as e:
                self.send_error(500, f"Error reading file: {e}")
        else:
            self.send_error(404, "File not found")

    def do_POST(self):
        url_path = self.path.split("?")[0]

        if url_path == "/api/compile":
            self.handle_compile()
        elif url_path == "/api/ai/diagnose":
            self.handle_ai_diagnose()
        elif url_path == "/api/ai/chat":
            self.handle_ai_chat()
        else:
            self.send_error(404, "Endpoint not found")

    def read_json_body(self):
        try:
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length).decode("utf-8")
            return json.loads(body)
        except Exception:
            return None

    def send_json(self, status_code, data):
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.end_headers()
        self.wfile.write(json.dumps(data, indent=2).encode("utf-8"))

    def handle_get_examples(self):
        examples = {}
        if EXAMPLES_DIR.exists():
            for f in sorted(EXAMPLES_DIR.glob("*.mako")):
                try:
                    with open(f, "r", encoding="utf-8") as file:
                        examples[f.name] = {
                            "name": f.stem.replace("_", " ").title(),
                            "filename": f.name,
                            "code": file.read()
                        }
                except Exception:
                    pass
        self.send_json(200, examples)

    def run_compiler(self, code):
        """Compiles code via mako --json and returns parsed dictionary result."""
        if not MAKO_BIN.exists():
            try:
                subprocess.run(["make"], cwd=str(ROOT_DIR), check=True)
            except Exception as e:
                return {"success": False, "error": f"Compiler build failed: {e}", "stage": "build"}

        with tempfile.NamedTemporaryFile(suffix=".mako", mode="w", delete=False) as tmp:
            tmp.write(code)
            tmp_path = tmp.name

        try:
            result = subprocess.run(
                [str(MAKO_BIN), tmp_path, "--json"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=8
            )
            raw = result.stdout.strip()
            try:
                return json.loads(raw)
            except Exception:
                return {
                    "success": False,
                    "stage": "runtime",
                    "error": raw or result.stderr.strip() or "Unknown compiler failure"
                }
        except subprocess.TimeoutExpired:
            return {
                "success": False,
                "stage": "runtime",
                "error": "Execution timed out (>8s). Infinite loop suspected."
            }
        except Exception as e:
            return {"success": False, "stage": "internal", "error": str(e)}
        finally:
            if os.path.exists(tmp_path):
                os.remove(tmp_path)

    def handle_compile(self):
        data = self.read_json_body()
        if not data or "code" not in data:
            self.send_json(400, {"success": False, "error": "Missing 'code' in request body"})
            return
        result = self.run_compiler(data["code"])
        self.send_json(200, result)

    def handle_ai_diagnose(self):
        """Active watcher endpoint: detects syntax/logic errors and computes 1-click fixes."""
        data = self.read_json_body() or {}
        code = data.get("code", "")
        api_key = data.get("api_key") or DEFAULT_GEMINI_KEY

        if not code.strip():
            self.send_json(200, {"has_issue": False, "message": "Editor is empty."})
            return

        # Step 1: Run compiler diagnostic pass
        compiler_res = self.run_compiler(code)

        if compiler_res.get("success"):
            # Code compiles. Check for potential algorithmic warnings or optimizations
            opt_issue = self.heuristic_optimization_check(code)
            if opt_issue:
                self.send_json(200, opt_issue)
                return
            self.send_json(200, {"has_issue": False, "message": "Code compiles cleanly."})
            return

        # Compiler reported an error
        error_msg = compiler_res.get("error", "Unknown error")
        stage = compiler_res.get("stage", "compiler")

        # Step 2: If Gemini API key is available, query Gemini for a fix
        if api_key:
            gemini_fix = self.query_gemini_fix(code, error_msg, stage, api_key)
            if gemini_fix and gemini_fix.get("suggested_code"):
                self.send_json(200, gemini_fix)
                return

        # Step 3: Fast heuristic rule-based diagnostic engine (works 100% offline)
        heuristic_fix = self.heuristic_fix_engine(code, error_msg)
        self.send_json(200, heuristic_fix)

    def query_gemini_fix(self, code, error_msg, stage, api_key):
        """Uses Gemini API to diagnose compiler error and suggest fixed code."""
        prompt = f"""You are the Mako AI Compiler Companion. Mako is an educational C-transpiled language.
Syntax rules:
- Variable: let name = expr;
- Function: fn name(param1, param2) {{ ... }}
- Condition: if (cond) {{ ... }} else {{ ... }}
- Loop: while (cond) {{ ... }}
- Output: print(expr);
- Statements require semicolons ';'.

The student's code failed at stage '{stage}' with error:
{error_msg}

Student Code:
```mako
{code}
```

Identify the exact issue, explain it in 1 sentence, and output the corrected full Mako code.
Output STRICTLY a valid JSON object matching this schema:
{{
  "has_issue": true,
  "type": "error",
  "title": "<short title like 'Missing Semicolon'>",
  "message": "<1 sentence explanation of what went wrong>",
  "explanation": "<short hint on why Mako syntax requires this>",
  "suggested_code": "<full corrected Mako code ready for editor>"
}}"""
        try:
            url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key={api_key}"
            payload = {
                "contents": [{"parts": [{"text": prompt}]}],
                "generationConfig": {"temperature": 0.1, "responseMimeType": "application/json"}
            }
            req = urllib.request.Request(
                url,
                data=json.dumps(payload).encode("utf-8"),
                headers={"Content-Type": "application/json"}
            )
            with urllib.request.urlopen(req, timeout=5) as resp:
                result = json.loads(resp.read().decode("utf-8"))
                text = result["candidates"][0]["content"]["parts"][0]["text"]
                return json.loads(text)
        except Exception as e:
            print(f"[Gemini Error] {e}")
            return None

    def heuristic_fix_engine(self, code, error_msg):
        """Deterministic diagnostic engine detecting common compiler errors and repairing them."""
        lines = code.split("\n")
        fixed_lines = list(lines)
        has_issue = True
        title = "Syntax Error"
        message = error_msg
        explanation = "The Mako compiler halted while verifying statement grammar."

        # Case 1: Missing semicolon
        if "Expected ';'" in error_msg:
            title = "Missing Semicolon"
            candidate_line = -1
            match = re.search(r"line (\d+)", error_msg)
            reported_line = int(match.group(1)) - 1 if match else -1

            # Compilers often report missing ';' at the start of the next line.
            # Scan backwards from reported_line for the actual unclosed statement.
            start_scan = min(len(lines) - 1, reported_line if reported_line >= 0 else len(lines) - 1)
            for idx in range(start_scan, -1, -1):
                s = lines[idx].strip()
                if s and not s.startswith("//") and not s.endswith(";") and not s.endswith("{") and not s.endswith("}"):
                    candidate_line = idx
                    break

            if candidate_line == -1:
                # Fallback: scan from top
                for idx, line in enumerate(lines):
                    s = line.strip()
                    if s and not s.startswith("//") and not s.endswith(";") and not s.endswith("{") and not s.endswith("}"):
                        candidate_line = idx
                        break

            if candidate_line != -1:
                fixed_lines[candidate_line] = fixed_lines[candidate_line].rstrip() + ";"
                message = f"Missing semicolon ';' at line {candidate_line + 1}."
                explanation = "In Mako, statements like 'let', 'print', and assignments must conclude with a semicolon."

        # Case 2: Missing parentheses around if or while condition
        # e.g., if x > 5 { or while i < 10 {
        elif "Expected '(' after 'if'" in error_msg or "Expected '(' after 'while'" in error_msg:
            title = "Missing Condition Parentheses"
            for idx, line in enumerate(fixed_lines):
                m_if = re.match(r"^(\s*if\s+)([^(].*?)(\s*\{.*)$", line)
                if m_if:
                    fixed_lines[idx] = f"{m_if.group(1)}({m_if.group(2).strip()}){m_if.group(3)}"
                    message = f"Enclose the condition in parentheses at line {idx + 1}."
                    explanation = "Mako requires if and while conditions to be enclosed in parentheses: if (condition) { ... }"
                    break
                m_wh = re.match(r"^(\s*while\s+)([^(].*?)(\s*\{.*)$", line)
                if m_wh:
                    fixed_lines[idx] = f"{m_wh.group(1)}({m_wh.group(2).strip()}){m_wh.group(3)}"
                    message = f"Enclose the while condition in parentheses at line {idx + 1}."
                    explanation = "Mako requires while conditions to be enclosed in parentheses: while (condition) { ... }"
                    break

        # Case 3: Unbalanced braces
        open_braces = code.count("{")
        close_braces = code.count("}")
        if open_braces > close_braces:
            title = "Unclosed Block"
            diff = open_braces - close_braces
            fixed_lines.append("}" * diff)
            message = f"Missing {diff} closing brace '}}'."
            explanation = "Every block opened with '{' must be paired with a corresponding closing '}'."

        # Case 4: Unterminated string literal
        elif "Unterminated string literal" in error_msg:
            title = "Unclosed String"
            for idx, line in enumerate(fixed_lines):
                if line.count('"') % 2 != 0:
                    fixed_lines[idx] = line + '"'
                    message = f"String literal was not closed on line {idx + 1}."
                    explanation = "Double quotes must be closed on the same line in Mako string literals."
                    break

        # Case 5: Single = inside if/while condition
        elif "==" not in code and "if" in code:
            for idx, line in enumerate(fixed_lines):
                if "if" in line and "=" in line and "==" not in line and "!=" not in line and "<=" not in line and ">=" not in line:
                    fixed_lines[idx] = re.sub(r"([a-zA-Z0-9_]+)\s*=\s*([a-zA-Z0-9_]+)", r"\1 == \2", line)
                    title = "Assignment in Condition"
                    message = f"Found '=' in condition at line {idx + 1}. Did you mean '=='?"
                    explanation = "Use '==' for comparison; '=' is used for assignment."
                    break

        suggested_code = "\n".join(fixed_lines)
        if suggested_code == code:
            # If no automated fix was generated, return informative explanation
            suggested_code = None

        return {
            "has_issue": True,
            "type": "error",
            "title": title,
            "message": message,
            "explanation": explanation,
            "suggested_code": suggested_code
        }

    def heuristic_optimization_check(self, code):
        """Analyzes valid code for algorithmic inefficiencies or educational tips."""
        # Check for naive exponential recursive Fibonacci
        if "fn fib(" in code and "fib(n - 1) + fib(n - 2)" in code:
            return {
                "has_issue": True,
                "type": "warning",
                "title": "Performance Warning: Exponential Recursion",
                "message": "Recursive Fibonacci runs in O(2^N) time and triggers heavy call stack overhead.",
                "explanation": "Translating this to an iterative loop reduces time complexity to O(N) and memory to O(1).",
                "suggested_code": """// Optimized Iterative Fibonacci — O(N) Time, O(1) Space
fn fib(n) {
    if (n <= 1) {
        return n;
    }
    let a = 0;
    let b = 1;
    let i = 2;
    while (i <= n) {
        let temp = a + b;
        a = b;
        b = temp;
        i = i + 1;
    }
    return b;
}

let i = 0;
while (i < 15) {
    print(fib(i));
    i = i + 1;
}"""
            }
        return None

    def handle_ai_chat(self):
        """Interactive Replit-style coding coach and assistant endpoint."""
        data = self.read_json_body() or {}
        user_message = data.get("message", "").strip()
        code = data.get("code", "")
        api_key = data.get("api_key") or DEFAULT_GEMINI_KEY

        if not user_message:
            self.send_json(400, {"error": "Empty message"})
            return

        # Query Gemini API if key is available
        if api_key:
            reply = self.query_gemini_chat(user_message, code, api_key)
            if reply:
                self.send_json(200, {"reply": reply})
                return

        # Intelligent local knowledge tutor
        reply = self.local_tutor_response(user_message, code)
        self.send_json(200, {"reply": reply})

    def query_gemini_chat(self, user_msg, code, api_key):
        system_instruction = f"""You are the Mako Compiler AI Assistant — a pair programming companion and compiler expert similar to Replit Agent, guiding students and developers.
Mako is an educational compiled language that transpiles to ISO C99.
Key Syntax:
- Variables: let x = 42;
- Functions: fn name(a, b) {{ return a + b; }}
- Conditionals: if (a > b) {{ ... }} else {{ ... }}
- Loops: while (condition) {{ ... }}
- Printing: print(expr); (prints strings or long long integers with newline)
- Types: 64-bit signed integers (long long), booleans, string literals.

Current Code in Editor:
```mako
{code}
```

Guidelines:
- Give concise, highly technical yet accessible explanations.
- When providing code, use markdown ```mako code blocks.
- Emphasize compiler concepts: AST structure, intermediate representation, and runtime optimization.
"""
        try:
            url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key={api_key}"
            payload = {
                "contents": [
                    {"parts": [{"text": f"{system_instruction}\n\nUser Question: {user_msg}"}]}
                ],
                "generationConfig": {"temperature": 0.4}
            }
            req = urllib.request.Request(
                url,
                data=json.dumps(payload).encode("utf-8"),
                headers={"Content-Type": "application/json"}
            )
            with urllib.request.urlopen(req, timeout=10) as resp:
                result = json.loads(resp.read().decode("utf-8"))
                return result["candidates"][0]["content"]["parts"][0]["text"]
        except Exception as e:
            print(f"[Gemini Chat Error] {e}")
            return None

    def local_tutor_response(self, user_msg, code):
        """Rich local knowledge base for Mako compiler and programming concepts."""
        msg_lower = user_msg.lower()

        # If the user asked to fix errors or diagnose
        if any(w in msg_lower for w in ["fix", "error", "repair", "issue"]):
            compiler_res = self.run_compiler(code)
            if not compiler_res.get("success"):
                err_msg = compiler_res.get("error", "")
                diag = self.heuristic_fix_engine(code, err_msg)
                if diag.get("suggested_code"):
                    return f"""### AI Code Repair

I analyzed your code and found a compiler issue: **{diag.get('title')}**
- **Diagnosis**: {diag.get('message')}
- **Why it matters**: {diag.get('explanation')}

Here is the corrected Mako code:
```mako
{diag.get('suggested_code')}
```
Click **Insert** above to automatically update your editor!"""
                else:
                    return f"""### Compiler Diagnostic

The compiler reported this error:
> `{err_msg}`

Please inspect your syntax at the reported line. Ensure all statements end with `;` and braces `{{ ... }}` are balanced."""
            else:
                return "Your code currently compiles with zero errors! Everything looks clean. You can ask me to optimize it or explain any language construct."

        if "comment" in msg_lower:
            # Annotate code with comments
            annotated_lines = []
            for line in code.splitlines():
                s = line.strip()
                if s.startswith("let "):
                    annotated_lines.append(f"// Variable declaration\n{line}")
                elif s.startswith("fn "):
                    annotated_lines.append(f"// Function definition\n{line}")
                elif s.startswith("while "):
                    annotated_lines.append(f"// Iterative loop\n{line}")
                elif s.startswith("if "):
                    annotated_lines.append(f"// Conditional branch\n{line}")
                elif s.startswith("print("):
                    annotated_lines.append(f"// Output to standard console\n{line}")
                else:
                    annotated_lines.append(line)
            annotated_code = "\n".join(annotated_lines)
            return f"""### Annotated Mako Source

Here is your program with descriptive line comments:

```mako
{annotated_code}
```
Click **Insert** to replace your editor with these comments!"""

        if "optimize" in msg_lower:
            return """### Mako Optimization Analysis

Here are actionable optimizations for Mako programs:

1. **Avoid Exponential Recursion**:
   Naive recursive branching (e.g. `fib(n - 1) + fib(n - 2)`) duplicates work across $O(2^N)$ stack frames. Replace recursion with iterative `while` loops.

2. **Loop Invariant Hoisting**:
   Expressions inside a `while` loop that do not depend on the loop counter should be declared with `let` outside the loop.

3. **Sample Iterative Fibonacci:**
```mako
fn fib_fast(n) {
    if (n <= 1) { return n; }
    let prev = 0;
    let curr = 1;
    let i = 2;
    while (i <= n) {
        let nxt = prev + curr;
        prev = curr;
        curr = nxt;
        i = i + 1;
    }
    return curr;
}
print(fib_fast(20));
```"""

        if "ast" in msg_lower or "syntax tree" in msg_lower or "how it works" in msg_lower:
            return """### How Mako Constructs the Abstract Syntax Tree (AST)

1. **Lexer**: Scans raw characters into typed tokens like `LET`, `IDENT ("x")`, `ASSIGN`, `INT (10)`, `SEMICOLON`.
2. **Parser**: Implements a recursive descent parser with precedence climbing:
   - Primary expressions (`10`, `"hello"`, variables)
   - Binary operations (`+`, `-`, `*`, `/`, `%`)
   - Statements (`VarDecl`, `If`, `While`, `Print`, `FnDecl`)
3. **AST Node Structure**:
   Each node retains its `NodeType`, source line number, and a dynamic array of child nodes (e.g. `If` node holds `[condition, then-block, else-block]`).
4. Check the **Syntax Tree** tab in the inspector to see your live AST!"""

        if "function" in msg_lower or "fn" in msg_lower:
            return """### Functions in Mako

Functions are declared using the `fn` keyword with comma-separated parameters:

```mako
fn multiply(a, b) {
    return a * b;
}

fn clamp(val, low, high) {
    if (val < low) {
        return low;
    } else if (val > high) {
        return high;
    }
    return val;
}

print(multiply(6, 7));       // 42
print(clamp(15, 0, 10));     // 10
```
- Parameters and return values evaluate as 64-bit signed integers (`long long` in C)."""

        if "loop" in msg_lower or "while" in msg_lower:
            return """### Loops in Mako

Mako provides `while` loops with standard boolean conditions:

```mako
let counter = 10;
while (counter > 0) {
    print(counter);
    counter = counter - 1;
}
print("Liftoff!");
```
**Tip**: Always ensure the loop variable is mutated inside the body block to prevent infinite execution."""

        # Default helpful assistant response
        return f"""I inspected your code. Here are key points for Mako development:

- **Current Line Count**: {len(code.splitlines())} lines.
- **Transpiler Target**: Clean ISO C99 compiled with `-O2`.
- **Supported Statements**: `let`, `fn`, `if/else`, `while`, `return`, `print`.

You can ask me to:
- **"Optimize my code"** — improve algorithmic performance
- **"Explain AST"** — break down compiler tree representation
- **"Write a [feature]"** — generate new Mako functions or algorithms"""


def run_server():
    server_address = ("", PORT)
    httpd = HTTPServer(server_address, MakoRequestHandler)
    print(f"\n🦈 Mako Playground Server running at http://localhost:{PORT}")
    print(f"👉 Open http://localhost:{PORT} in your web browser!\n")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server...")
        httpd.server_close()


if __name__ == "__main__":
    run_server()
