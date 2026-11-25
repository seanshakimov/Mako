#!/usr/bin/env python3
"""
Mako Compiler - Web Playground Backend Server
A lightweight, dependency-free HTTP server that powers the interactive web playground.
"""

import os
import sys
import json
import tempfile
import subprocess
import mimetypes
from http.server import HTTPServer, BaseHTTPRequestHandler
from pathlib import Path

# Paths
ROOT_DIR = Path(__file__).resolve().parent.parent
STATIC_DIR = Path(__file__).resolve().parent / "static"
EXAMPLES_DIR = ROOT_DIR / "examples"
MAKO_BIN = ROOT_DIR / "mako"

PORT = int(os.environ.get("PORT", 8080))


class MakoRequestHandler(BaseHTTPRequestHandler):
    def end_headers(self):
        # Enable CORS for local development
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()

    def do_GET(self):
        url_path = self.path.split("?")[0]

        # API: list available examples
        if url_path == "/api/examples":
            self.handle_get_examples()
            return

        # Static file serving
        if url_path == "/" or url_path == "":
            file_path = STATIC_DIR / "index.html"
        else:
            safe_rel = url_path.lstrip("/")
            file_path = (STATIC_DIR / safe_rel).resolve()
            # Prevent directory traversal
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
        else:
            self.send_error(404, "Endpoint not found")

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

        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.end_headers()
        self.wfile.write(json.dumps(examples, indent=2).encode("utf-8"))

    def handle_compile(self):
        try:
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length).decode("utf-8")
            data = json.loads(body)
            code = data.get("code", "")
        except Exception as e:
            self.send_response(400)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"success": False, "error": f"Invalid request: {e}"}).encode("utf-8"))
            return

        if not MAKO_BIN.exists():
            # Attempt to build Mako if binary is missing
            try:
                subprocess.run(["make"], cwd=str(ROOT_DIR), check=True)
            except Exception as e:
                self.send_response(500)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"success": False, "error": f"Compiler binary missing and build failed: {e}"}).encode("utf-8"))
                return

        # Write code to a temporary file and run mako --json
        with tempfile.NamedTemporaryFile(suffix=".mako", mode="w", delete=False) as tmp:
            tmp.write(code)
            tmp_path = tmp.name

        try:
            result = subprocess.run(
                [str(MAKO_BIN), tmp_path, "--json"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=10
            )

            raw_out = result.stdout.strip()
            # Sometimes stderr has warnings, let's parse JSON from stdout
            try:
                parsed = json.loads(raw_out)
            except Exception:
                parsed = {
                    "success": False,
                    "stage": "runtime",
                    "error": raw_out or result.stderr.strip() or "Unknown compiler error"
                }

            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.end_headers()
            self.wfile.write(json.dumps(parsed).encode("utf-8"))

        except subprocess.TimeoutExpired:
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({
                "success": False,
                "stage": "runtime",
                "error": "Execution timed out (infinite loop protection: >10s)"
            }).encode("utf-8"))
        except Exception as e:
            self.send_response(500)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"success": False, "error": str(e)}).encode("utf-8"))
        finally:
            if os.path.exists(tmp_path):
                os.remove(tmp_path)


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
