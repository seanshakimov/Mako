/* ═══════════════════════════════════════════════════════════════════════
   Mako Playground — Interactive Frontend Application
   ═══════════════════════════════════════════════════════════════════════ */

document.addEventListener("DOMContentLoaded", () => {
  // DOM Elements
  const codeEditor = document.getElementById("codeEditor");
  const lineNumbers = document.getElementById("lineNumbers");
  const cursorPos = document.getElementById("cursorPos");
  const exampleSelect = document.getElementById("exampleSelect");
  const runBtn = document.getElementById("runBtn");
  const clearBtn = document.getElementById("clearBtn");
  const resetBtn = document.getElementById("resetBtn");
  const currentFilename = document.getElementById("currentFilename");

  // Output Elements
  const outputStatus = document.getElementById("outputStatus");
  const statusMessage = document.getElementById("statusMessage");
  const execTime = document.getElementById("execTime");
  const terminalOutput = document.getElementById("terminalOutput");
  const tokensList = document.getElementById("tokensList");
  const tokenCount = document.getElementById("tokenCount");
  const astTree = document.getElementById("astTree");
  const astCount = document.getElementById("astCount");
  const cCodeView = document.getElementById("cCodeView");
  const tabBtns = document.querySelectorAll(".tab-btn");
  const tabPanes = document.querySelectorAll(".tab-pane");

  let examplesCache = {};
  let currentExampleKey = "hello.mako";

  // ── Tab Navigation ───────────────────────────────────────────────────
  tabBtns.forEach(btn => {
    btn.addEventListener("click", () => {
      tabBtns.forEach(b => b.classList.remove("active"));
      tabPanes.forEach(p => p.classList.remove("active"));
      btn.classList.add("active");
      const targetId = btn.getAttribute("data-tab");
      const targetPane = document.getElementById(targetId);
      if (targetPane) targetPane.classList.add("active");
    });
  });

  // ── Line Numbers & Cursor Tracking ──────────────────────────────────
  function updateLineNumbers() {
    const lines = codeEditor.value.split("\n").length;
    let numStr = "";
    for (let i = 1; i <= lines; i++) {
      numStr += i + "\n";
    }
    lineNumbers.textContent = numStr;
  }

  function updateCursorPos() {
    const pos = codeEditor.selectionStart;
    const lines = codeEditor.value.substring(0, pos).split("\n");
    const line = lines.length;
    const col = lines[lines.length - 1].length + 1;
    cursorPos.textContent = `Line ${line}, Col ${col}`;
  }

  codeEditor.addEventListener("input", updateLineNumbers);
  codeEditor.addEventListener("keyup", updateCursorPos);
  codeEditor.addEventListener("click", updateCursorPos);

  // Sync scroll between line numbers and textarea
  codeEditor.addEventListener("scroll", () => {
    lineNumbers.scrollTop = codeEditor.scrollTop;
  });

  // ── Tab Key Indentation Support ─────────────────────────────────────
  codeEditor.addEventListener("keydown", (e) => {
    // ⌘+Enter or Ctrl+Enter to compile
    if ((e.metaKey || e.ctrlKey) && e.key === "Enter") {
      e.preventDefault();
      compileCode();
      return;
    }

    // Tab key inserts 4 spaces
    if (e.key === "Tab") {
      e.preventDefault();
      const start = codeEditor.selectionStart;
      const end = codeEditor.selectionEnd;
      const val = codeEditor.value;
      codeEditor.value = val.substring(0, start) + "    " + val.substring(end);
      codeEditor.selectionStart = codeEditor.selectionEnd = start + 4;
      updateLineNumbers();
    }
  });

  // ── Load Examples ───────────────────────────────────────────────────
  async function loadExamples() {
    try {
      const res = await fetch("/api/examples");
      if (!res.ok) throw new Error("Failed to fetch examples");
      examplesCache = await res.json();

      exampleSelect.innerHTML = "";
      for (const [filename, ex] of Object.entries(examplesCache)) {
        const opt = document.createElement("option");
        opt.value = filename;
        opt.textContent = `${ex.name} (${filename})`;
        exampleSelect.appendChild(opt);
      }

      if (examplesCache["hello.mako"]) {
        setExample("hello.mako");
      } else if (Object.keys(examplesCache).length > 0) {
        setExample(Object.keys(examplesCache)[0]);
      }
    } catch (err) {
      console.warn("Could not load examples via API:", err);
      // Fallback default
      codeEditor.value = `// Welcome to Mako Playground!
print("Hello, World!");

let x = 10;
let y = 20;
let sum = x + y;
print(sum);
`;
      updateLineNumbers();
    }
  }

  function setExample(filename) {
    if (examplesCache[filename]) {
      currentExampleKey = filename;
      exampleSelect.value = filename;
      currentFilename.textContent = filename;
      codeEditor.value = examplesCache[filename].code;
      updateLineNumbers();
      updateCursorPos();
      compileCode();
    }
  }

  exampleSelect.addEventListener("change", (e) => {
    setExample(e.target.value);
  });

  resetBtn.addEventListener("click", () => {
    if (examplesCache[currentExampleKey]) {
      codeEditor.value = examplesCache[currentExampleKey].code;
      updateLineNumbers();
      updateCursorPos();
      compileCode();
    }
  });

  clearBtn.addEventListener("click", () => {
    codeEditor.value = "";
    codeEditor.focus();
    updateLineNumbers();
    updateCursorPos();
  });

  runBtn.addEventListener("click", compileCode);

  // ── Compilation & Rendering ─────────────────────────────────────────
  async function compileCode() {
    const code = codeEditor.value;
    runBtn.classList.add("running");
    runBtn.innerHTML = `<span class="btn-icon">⏳</span> Compiling...`;

    statusMessage.textContent = "Compiling pipeline...";
    const indicator = outputStatus.querySelector(".status-indicator");
    indicator.className = "status-indicator ready";
    execTime.textContent = "";

    const startTime = performance.now();

    try {
      const res = await fetch("/api/compile", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ code })
      });

      const data = await res.json();
      const elapsed = Math.round(performance.now() - startTime);
      execTime.textContent = `${elapsed}ms`;

      renderResults(data);
    } catch (err) {
      indicator.className = "status-indicator error";
      statusMessage.textContent = "Compilation Failed";
      terminalOutput.className = "terminal-body error-text";
      terminalOutput.textContent = `Network / Server Error: ${err.message}`;
    } finally {
      runBtn.classList.remove("running");
      runBtn.innerHTML = `<span class="btn-icon">▶</span> Run Code`;
    }
  }

  function renderResults(data) {
    const indicator = outputStatus.querySelector(".status-indicator");

    if (data.success) {
      indicator.className = "status-indicator success";
      statusMessage.textContent = "Compilation & Execution Succeeded";
      terminalOutput.className = "terminal-body";
      terminalOutput.textContent = data.output || "(Program produced no output)";
    } else {
      indicator.className = "status-indicator error";
      statusMessage.textContent = `Error in Stage: ${data.stage || 'unknown'}`;
      terminalOutput.className = "terminal-body error-text";
      terminalOutput.textContent = data.error || "Compilation failed";
    }

    // Render Tokens
    renderTokens(data.tokens || []);

    // Render AST
    renderAST(data.ast);

    // Render C Code
    if (data.c_code) {
      cCodeView.textContent = data.c_code;
    } else {
      cCodeView.textContent = "// Code generation was not reached due to prior stage error.";
    }
  }

  function renderTokens(tokens) {
    tokenCount.textContent = tokens.length;
    tokensList.innerHTML = "";

    if (tokens.length === 0) {
      tokensList.innerHTML = `<tr><td colspan="4" class="empty-state">No tokens emitted</td></tr>`;
      return;
    }

    tokens.forEach((t, idx) => {
      const tr = document.createElement("tr");

      let badgeClass = "ident";
      const type = t.type;
      if (["LET", "IF", "ELSE", "WHILE", "FN", "RETURN", "PRINT"].includes(type)) {
        badgeClass = "keyword";
      } else if (["INT", "STR", "TRUE", "FALSE"].includes(type)) {
        badgeClass = "literal";
      } else if (["PLUS", "MINUS", "STAR", "SLASH", "PERCENT", "ASSIGN", "EQ", "NEQ", "LT", "GT", "LTE", "GTE", "AND", "OR", "NOT"].includes(type)) {
        badgeClass = "op";
      } else if (["LPAREN", "RPAREN", "LBRACE", "RBRACE", "SEMICOLON", "COMMA", "EOF"].includes(type)) {
        badgeClass = "delim";
      } else if (type === "ERROR") {
        badgeClass = "error";
      }

      tr.innerHTML = `
        <td style="color: var(--text-dim);">${idx + 1}</td>
        <td style="color: var(--text-muted);">${t.line}:${t.col}</td>
        <td><span class="token-badge ${badgeClass}">${type}</span></td>
        <td style="color: #e6edf3;"><code>${escapeHtml(t.value)}</code></td>
      `;
      tokensList.appendChild(tr);
    });
  }

  function renderAST(astNode) {
    astTree.innerHTML = "";
    if (!astNode) {
      astCount.textContent = "0";
      astTree.innerHTML = `<div class="empty-state">No AST constructed</div>`;
      return;
    }

    let count = 0;
    function buildNodeHtml(node) {
      if (!node) return "";
      count++;

      let html = `<div class="ast-node">`;
      html += `<div class="ast-node-header">`;
      html += `<span class="ast-type">${escapeHtml(node.type)}</span>`;
      if (node.value) {
        html += `<span class="ast-val">"${escapeHtml(node.value)}"</span>`;
      }
      if (node.op) {
        html += `<span class="ast-op">[${escapeHtml(node.op)}]</span>`;
      }
      if (node.params && node.params.length > 0) {
        html += `<span style="color:#c084fc;">(params: ${node.params.map(p => escapeHtml(p)).join(", ")})</span>`;
      }
      html += `<span class="ast-line">L${node.line}</span>`;
      html += `</div>`;

      if (node.children && node.children.length > 0) {
        node.children.forEach(child => {
          html += buildNodeHtml(child);
        });
      }
      html += `</div>`;
      return html;
    }

    astTree.innerHTML = buildNodeHtml(astNode);
    astCount.textContent = count;
  }

  function escapeHtml(str) {
    if (!str) return "";
    return String(str)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;")
      .replace(/'/g, "&#039;");
  }

  // Initial load
  loadExamples();
});
