/* ═══════════════════════════════════════════════════════════════════════
   Mako Playground — Client Application
   ═══════════════════════════════════════════════════════════════════════ */

document.addEventListener("DOMContentLoaded", () => {
  // Theme Management
  const htmlEl = document.documentElement;
  const themeToggle = document.getElementById("themeToggle");

  function initTheme() {
    const savedTheme = localStorage.getItem("mako_theme");
    if (savedTheme) {
      htmlEl.setAttribute("data-theme", savedTheme);
    } else {
      // Default to dark with purple hues
      htmlEl.setAttribute("data-theme", "dark");
    }
  }

  themeToggle.addEventListener("click", () => {
    const currentTheme = htmlEl.getAttribute("data-theme") || "dark";
    const nextTheme = currentTheme === "dark" ? "light" : "dark";
    htmlEl.setAttribute("data-theme", nextTheme);
    localStorage.setItem("mako_theme", nextTheme);
  });

  initTheme();

  // Core DOM Elements
  const codeEditor = document.getElementById("codeEditor");
  const lineGutter = document.getElementById("lineGutter");
  const cursorInfo = document.getElementById("cursorInfo");
  const exampleSelect = document.getElementById("exampleSelect");
  const runBtn = document.getElementById("runBtn");
  const clearBtn = document.getElementById("clearBtn");
  const resetBtn = document.getElementById("resetBtn");
  const currentFilename = document.getElementById("currentFilename");

  // Output Elements
  const statusBadge = document.getElementById("statusBadge");
  const statusLabel = document.getElementById("statusLabel");
  const durationMetric = document.getElementById("durationMetric");
  const consoleOutput = document.getElementById("consoleOutput");
  const tokensTableBody = document.getElementById("tokensTableBody");
  const tokenCount = document.getElementById("tokenCount");
  const astContainer = document.getElementById("astContainer");
  const astCount = document.getElementById("astCount");
  const cCodeBlock = document.getElementById("cCodeBlock");
  const copyConsoleBtn = document.getElementById("copyConsoleBtn");
  const copyCodeBtn = document.getElementById("copyCodeBtn");

  const tabItems = document.querySelectorAll(".tab-item");
  const tabViews = document.querySelectorAll(".tab-view");

  let examplesCache = {};
  let currentExampleKey = "hello.mako";

  // ── Tab Navigation ───────────────────────────────────────────────────
  tabItems.forEach(tab => {
    tab.addEventListener("click", () => {
      tabItems.forEach(t => {
        t.classList.remove("active");
        t.setAttribute("aria-selected", "false");
      });
      tabViews.forEach(v => v.classList.remove("active"));

      tab.classList.add("active");
      tab.setAttribute("aria-selected", "true");

      const targetId = tab.getAttribute("data-tab");
      const targetView = document.getElementById(targetId);
      if (targetView) targetView.classList.add("active");
    });
  });

  // ── Gutter Line Synchronization ─────────────────────────────────────
  function updateGutter() {
    const lines = codeEditor.value.split("\n").length;
    let gutterText = "";
    for (let i = 1; i <= lines; i++) {
      gutterText += i + "\n";
    }
    lineGutter.textContent = gutterText;
  }

  function updateCursorInfo() {
    const pos = codeEditor.selectionStart;
    const textBefore = codeEditor.value.substring(0, pos);
    const lines = textBefore.split("\n");
    const line = lines.length;
    const col = lines[lines.length - 1].length + 1;
    cursorInfo.textContent = `Ln ${line}, Col ${col}`;
  }

  codeEditor.addEventListener("input", updateGutter);
  codeEditor.addEventListener("keyup", updateCursorInfo);
  codeEditor.addEventListener("click", updateCursorInfo);

  codeEditor.addEventListener("scroll", () => {
    lineGutter.scrollTop = codeEditor.scrollTop;
  });

  // ── Indentation & Shortcuts ──────────────────────────────────────────
  codeEditor.addEventListener("keydown", (e) => {
    // ⌘ + Enter or Ctrl + Enter: Compile & Run
    if ((e.metaKey || e.ctrlKey) && e.key === "Enter") {
      e.preventDefault();
      executeCode();
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
      updateGutter();
    }
  });

  // ── Load Examples ───────────────────────────────────────────────────
  async function loadExamples() {
    try {
      const res = await fetch("/api/examples");
      if (!res.ok) throw new Error("Could not load examples");
      examplesCache = await res.json();

      exampleSelect.innerHTML = "";
      for (const [filename, ex] of Object.entries(examplesCache)) {
        const opt = document.createElement("option");
        opt.value = filename;
        opt.textContent = filename;
        exampleSelect.appendChild(opt);
      }

      if (examplesCache["hello.mako"]) {
        setExample("hello.mako");
      } else if (Object.keys(examplesCache).length > 0) {
        setExample(Object.keys(examplesCache)[0]);
      }
    } catch (err) {
      console.warn("Failed to fetch examples from API:", err);
      // Fallback
      codeEditor.value = `// hello.mako
print("Hello, World!");
print("Welcome to Mako!");

let x = 10;
let y = 20;
let sum = x + y;
print(sum);
`;
      updateGutter();
    }
  }

  function setExample(filename) {
    if (examplesCache[filename]) {
      currentExampleKey = filename;
      exampleSelect.value = filename;
      currentFilename.textContent = filename;
      codeEditor.value = examplesCache[filename].code;
      updateGutter();
      updateCursorInfo();
      executeCode();
    }
  }

  exampleSelect.addEventListener("change", (e) => {
    setExample(e.target.value);
  });

  resetBtn.addEventListener("click", () => {
    if (examplesCache[currentExampleKey]) {
      codeEditor.value = examplesCache[currentExampleKey].code;
      updateGutter();
      updateCursorInfo();
      executeCode();
    }
  });

  clearBtn.addEventListener("click", () => {
    codeEditor.value = "";
    codeEditor.focus();
    updateGutter();
    updateCursorInfo();
  });

  runBtn.addEventListener("click", executeCode);

  // ── Copy Buttons ─────────────────────────────────────────────────────
  copyConsoleBtn.addEventListener("click", () => {
    navigator.clipboard.writeText(consoleOutput.textContent);
    copyConsoleBtn.textContent = "Copied";
    setTimeout(() => { copyConsoleBtn.textContent = "Copy"; }, 1500);
  });

  copyCodeBtn.addEventListener("click", () => {
    navigator.clipboard.writeText(cCodeBlock.textContent);
    copyCodeBtn.textContent = "Copied";
    setTimeout(() => { copyCodeBtn.textContent = "Copy"; }, 1500);
  });

  // ── Code Execution ───────────────────────────────────────────────────
  async function executeCode() {
    const code = codeEditor.value;
    runBtn.classList.add("running");

    statusLabel.textContent = "Compiling...";
    const dot = statusBadge.querySelector(".status-dot");
    dot.className = "status-dot ready";
    durationMetric.textContent = "—";

    const startTime = performance.now();

    try {
      const res = await fetch("/api/compile", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ code })
      });

      const data = await res.json();
      const durationMs = Math.round(performance.now() - startTime);
      durationMetric.textContent = `${durationMs} ms`;

      renderCompilation(data);
    } catch (err) {
      dot.className = "status-dot error";
      statusLabel.textContent = "Execution Error";
      consoleOutput.className = "console-output error-text";
      consoleOutput.textContent = `Server communication failure: ${err.message}`;
    } finally {
      runBtn.classList.remove("running");
    }
  }

  function renderCompilation(data) {
    const dot = statusBadge.querySelector(".status-dot");

    if (data.success) {
      dot.className = "status-dot success";
      statusLabel.textContent = `Exit Code: ${data.exit_code ?? 0}`;
      consoleOutput.className = "console-output success-text";
      consoleOutput.textContent = data.output || "(Program completed with no output)";
    } else {
      dot.className = "status-dot error";
      statusLabel.textContent = `Error in ${data.stage || 'runtime'}`;
      consoleOutput.className = "console-output error-text";
      consoleOutput.textContent = data.error || "Compilation failed.";
    }

    // Render Tokens Table
    renderTokens(data.tokens || []);

    // Render AST View
    renderAST(data.ast);

    // Render C Source
    if (data.c_code) {
      cCodeBlock.textContent = data.c_code;
    } else {
      cCodeBlock.textContent = "// Target code generation was not reached due to a compilation error.";
    }
  }

  function renderTokens(tokens) {
    tokenCount.textContent = tokens.length;
    tokensTableBody.innerHTML = "";

    if (tokens.length === 0) {
      tokensTableBody.innerHTML = `<tr><td colspan="4" class="empty-notice">No tokens emitted.</td></tr>`;
      return;
    }

    tokens.forEach((t, i) => {
      const tr = document.createElement("tr");

      let category = "ident";
      const type = t.type;
      if (["LET", "IF", "ELSE", "WHILE", "FN", "RETURN", "PRINT"].includes(type)) {
        category = "keyword";
      } else if (["INT", "STR", "TRUE", "FALSE"].includes(type)) {
        category = "literal";
      } else if (["PLUS", "MINUS", "STAR", "SLASH", "PERCENT", "ASSIGN", "EQ", "NEQ", "LT", "GT", "LTE", "GTE", "AND", "OR", "NOT"].includes(type)) {
        category = "op";
      } else if (["LPAREN", "RPAREN", "LBRACE", "RBRACE", "SEMICOLON", "COMMA", "EOF"].includes(type)) {
        category = "delim";
      } else if (type === "ERROR") {
        category = "error";
      }

      tr.innerHTML = `
        <td style="color: var(--text-tertiary);">${i + 1}</td>
        <td style="color: var(--text-secondary);">${t.line}:${t.col}</td>
        <td><span class="token-pill ${category}">${type}</span></td>
        <td><code>${escapeHtml(t.value)}</code></td>
      `;
      tokensTableBody.appendChild(tr);
    });
  }

  function renderAST(astNode) {
    astContainer.innerHTML = "";
    if (!astNode) {
      astCount.textContent = "0";
      astContainer.innerHTML = `<div class="empty-notice">No syntax tree available.</div>`;
      return;
    }

    let nodeCount = 0;
    function buildNodeDom(node) {
      if (!node) return "";
      nodeCount++;

      let html = `<div class="ast-entry">`;
      html += `<div class="ast-node-card">`;
      html += `<span class="ast-type">${escapeHtml(node.type)}</span>`;
      if (node.value) {
        html += `<span class="ast-value">"${escapeHtml(node.value)}"</span>`;
      }
      if (node.op) {
        html += `<span class="ast-op">[${escapeHtml(node.op)}]</span>`;
      }
      if (node.params && node.params.length > 0) {
        html += `<span class="ast-params">(${node.params.map(p => escapeHtml(p)).join(", ")})</span>`;
      }
      html += `<span class="ast-line">L${node.line}</span>`;
      html += `</div>`;

      if (node.children && node.children.length > 0) {
        node.children.forEach(child => {
          html += buildNodeDom(child);
        });
      }
      html += `</div>`;
      return html;
    }

    astContainer.innerHTML = buildNodeDom(astNode);
    astCount.textContent = nodeCount;
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

  loadExamples();
});
