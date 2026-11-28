/* ═══════════════════════════════════════════════════════════════════════
   Mako Playground — Client Application with Active AI Code Companion
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

  // AI Watcher Elements
  const aiWatcherBanner = document.getElementById("aiWatcherBanner");
  const watcherTitle = document.getElementById("watcherTitle");
  const watcherMsg = document.getElementById("watcherMsg");
  const watcherDesc = document.getElementById("watcherDesc");
  const applyFixBtn = document.getElementById("applyFixBtn");
  const explainFixBtn = document.getElementById("explainFixBtn");
  const dismissBannerBtn = document.getElementById("dismissBannerBtn");
  const watcherStatusPill = document.getElementById("watcherStatusPill");
  const watcherStatusText = document.getElementById("watcherStatusText");

  // AI Assistant Drawer Elements
  const workbench = document.querySelector(".workbench");
  const aiToggleBtn = document.getElementById("aiToggleBtn");
  const aiDrawer = document.getElementById("aiDrawer");
  const closeDrawerBtn = document.getElementById("closeDrawerBtn");
  const chatMessages = document.getElementById("chatMessages");
  const chatInput = document.getElementById("chatInput");
  const sendChatBtn = document.getElementById("sendChatBtn");
  const promptChips = document.querySelectorAll(".prompt-chip");

  // API Key Modal Elements
  const apiKeySettingsBtn = document.getElementById("apiKeySettingsBtn");
  const apiKeyModal = document.getElementById("apiKeyModal");
  const closeModalBtn = document.getElementById("closeModalBtn");
  const apiKeyInput = document.getElementById("apiKeyInput");
  const saveKeyBtn = document.getElementById("saveKeyBtn");
  const clearKeyBtn = document.getElementById("clearKeyBtn");

  const tabItems = document.querySelectorAll(".tab-item");
  const tabViews = document.querySelectorAll(".tab-view");

  let examplesCache = {};
  let currentExampleKey = "hello.mako";
  let activeDiagnosticData = null;
  let watcherDebounceTimer = null;
  let isBannerDismissed = false;

  // ── API Key Management ───────────────────────────────────────────────
  function getApiKey() {
    return localStorage.getItem("mako_gemini_key") || "";
  }

  apiKeySettingsBtn.addEventListener("click", () => {
    apiKeyInput.value = getApiKey();
    apiKeyModal.classList.remove("hidden");
    apiKeyInput.focus();
  });

  closeModalBtn.addEventListener("click", () => {
    apiKeyModal.classList.add("hidden");
  });

  saveKeyBtn.addEventListener("click", () => {
    const key = apiKeyInput.value.trim();
    if (key) {
      localStorage.setItem("mako_gemini_key", key);
    } else {
      localStorage.removeItem("mako_gemini_key");
    }
    apiKeyModal.classList.add("hidden");
    appendAiMessage("Gemini API key updated. Enhanced natural language models are now active!");
  });

  clearKeyBtn.addEventListener("click", () => {
    localStorage.removeItem("mako_gemini_key");
    apiKeyInput.value = "";
    apiKeyModal.classList.add("hidden");
    appendAiMessage("Using built-in Mako heuristic compiler assistant.");
  });

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

  codeEditor.addEventListener("input", () => {
    updateGutter();
    isBannerDismissed = false;
    scheduleWatcherCheck();
  });

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

  // ── Active Code Watcher (Debounced Real-Time Linter) ─────────────────
  function scheduleWatcherCheck() {
    clearTimeout(watcherDebounceTimer);
    setWatcherStatus("checking", "Watcher: Analyzing...");

    watcherDebounceTimer = setTimeout(async () => {
      const code = codeEditor.value;
      if (!code.trim()) {
        hideWatcherBanner();
        setWatcherStatus("clean", "Watcher: Empty");
        return;
      }

      try {
        const res = await fetch("/api/ai/diagnose", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ code, api_key: getApiKey() })
        });
        const data = await res.json();
        handleWatcherResult(data);
      } catch (err) {
        setWatcherStatus("clean", "Watcher: Idle");
      }
    }, 600);
  }

  function setWatcherStatus(state, text) {
    watcherStatusPill.className = `watcher-status-pill ${state}`;
    watcherStatusText.textContent = text;
  }

  function handleWatcherResult(data) {
    activeDiagnosticData = data;

    if (data.has_issue) {
      setWatcherStatus("issue", `Watcher: ${data.title || 'Issue Detected'}`);
      if (!isBannerDismissed) {
        showWatcherBanner(data);
      }
    } else {
      setWatcherStatus("clean", "Watcher: Clean");
      hideWatcherBanner();
    }
  }

  function showWatcherBanner(data) {
    watcherTitle.textContent = data.title || "AI Watcher";
    watcherMsg.textContent = data.message || "Potential syntax issue detected";
    watcherDesc.textContent = data.explanation || "";

    aiWatcherBanner.className = `ai-watcher-banner ${data.type || 'error'}`;
    if (data.suggested_code) {
      applyFixBtn.classList.remove("hidden");
    } else {
      applyFixBtn.classList.add("hidden");
    }

    aiWatcherBanner.classList.remove("hidden");
  }

  function hideWatcherBanner() {
    aiWatcherBanner.classList.add("hidden");
  }

  dismissBannerBtn.addEventListener("click", () => {
    isBannerDismissed = true;
    hideWatcherBanner();
  });

  applyFixBtn.addEventListener("click", () => {
    if (activeDiagnosticData && activeDiagnosticData.suggested_code) {
      codeEditor.value = activeDiagnosticData.suggested_code;
      updateGutter();
      updateCursorInfo();
      hideWatcherBanner();
      setWatcherStatus("clean", "Watcher: Fix Applied");
      executeCode();
    }
  });

  explainFixBtn.addEventListener("click", () => {
    openDrawer();
    if (activeDiagnosticData) {
      sendChatMessage(`Explain this issue in detail: "${activeDiagnosticData.message}". Why does Mako require this?`);
    }
  });

  // ── AI Drawer Navigation ─────────────────────────────────────────────
  function openDrawer() {
    aiDrawer.classList.remove("hidden");
    workbench.classList.add("with-drawer");
    chatInput.focus();
  }

  function closeDrawer() {
    aiDrawer.classList.add("hidden");
    workbench.classList.remove("with-drawer");
  }

  aiToggleBtn.addEventListener("click", () => {
    if (aiDrawer.classList.contains("hidden")) {
      openDrawer();
    } else {
      closeDrawer();
    }
  });

  closeDrawerBtn.addEventListener("click", closeDrawer);

  // ── Chat Messaging ───────────────────────────────────────────────────
  async function sendChatMessage(userText) {
    const text = userText || chatInput.value.trim();
    if (!text) return;

    appendUserMessage(text);
    chatInput.value = "";

    const loadingId = appendAiMessage("Thinking...");

    try {
      const res = await fetch("/api/ai/chat", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          message: text,
          code: codeEditor.value,
          api_key: getApiKey()
        })
      });
      const data = await res.json();
      updateAiMessage(loadingId, data.reply || "No response received.");
    } catch (err) {
      updateAiMessage(loadingId, `Could not reach AI assistant: ${err.message}`);
    }
  }

  sendChatBtn.addEventListener("click", () => sendChatMessage());
  chatInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter" && !e.shiftKey) {
      e.preventDefault();
      sendChatMessage();
    }
  });

  promptChips.forEach(chip => {
    chip.addEventListener("click", () => {
      openDrawer();
      sendChatMessage(chip.getAttribute("data-prompt"));
    });
  });

  function appendUserMessage(text) {
    const div = document.createElement("div");
    div.className = "chat-bubble user";
    div.textContent = text;
    chatMessages.appendChild(div);
    chatMessages.scrollTop = chatMessages.scrollHeight;
  }

  function appendAiMessage(rawContent) {
    const id = "msg-" + Date.now();
    const div = document.createElement("div");
    div.className = "chat-bubble ai";
    div.id = id;
    div.innerHTML = formatMarkdown(rawContent);
    chatMessages.appendChild(div);
    chatMessages.scrollTop = chatMessages.scrollHeight;
    attachCodeActionListeners(div);
    return id;
  }

  function updateAiMessage(id, rawContent) {
    const el = document.getElementById(id);
    if (el) {
      el.innerHTML = formatMarkdown(rawContent);
      attachCodeActionListeners(el);
      chatMessages.scrollTop = chatMessages.scrollHeight;
    }
  }

  function attachCodeActionListeners(container) {
    container.querySelectorAll(".btn-insert-code").forEach(btn => {
      btn.addEventListener("click", () => {
        const rawCode = decodeURIComponent(btn.getAttribute("data-code"));
        codeEditor.value = rawCode;
        updateGutter();
        updateCursorInfo();
        executeCode();
        btn.textContent = "Inserted!";
        setTimeout(() => { btn.textContent = "Insert"; }, 1500);
      });
    });

    container.querySelectorAll(".btn-copy-code").forEach(btn => {
      btn.addEventListener("click", () => {
        const rawCode = decodeURIComponent(btn.getAttribute("data-code"));
        navigator.clipboard.writeText(rawCode);
        btn.textContent = "Copied!";
        setTimeout(() => { btn.textContent = "Copy"; }, 1500);
      });
    });
  }

  function formatMarkdown(text) {
    if (!text) return "";

    // Escape HTML first
    let escaped = escapeHtml(text);

    // Code blocks ```mako ... ```
    escaped = escaped.replace(/```(?:mako|c)?\n([\s\S]*?)```/g, (match, code) => {
      const cleanCode = code.trim();
      const encoded = encodeURIComponent(cleanCode);
      return `
        <div class="ai-code-block">
          <div class="ai-code-header">
            <span>Mako</span>
            <div class="ai-code-actions">
              <button class="btn-ghost btn-sm btn-copy-code" data-code="${encoded}">Copy</button>
              <button class="btn-primary btn-sm btn-insert-code" data-code="${encoded}">Insert</button>
            </div>
          </div>
          <pre><code>${cleanCode}</code></pre>
        </div>
      `;
    });

    // Headers
    escaped = escaped.replace(/^### (.*$)/gim, "<h3>$1</h3>");
    escaped = escaped.replace(/^## (.*$)/gim, "<h3>$1</h3>");

    // Bold
    escaped = escaped.replace(/\*\*(.*?)\*\*/g, "<strong>$1</strong>");

    // Inline code `...`
    escaped = escaped.replace(/`([^`]+)`/g, "<code>$1</code>");

    // Line breaks
    escaped = escaped.replace(/\n\n+/g, "</p><p>");
    escaped = escaped.replace(/\n/g, "<br>");

    return `<p>${escaped}</p>`;
  }

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
      scheduleWatcherCheck();
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
      scheduleWatcherCheck();
    }
  });

  clearBtn.addEventListener("click", () => {
    codeEditor.value = "";
    codeEditor.focus();
    updateGutter();
    updateCursorInfo();
    scheduleWatcherCheck();
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

    renderTokens(data.tokens || []);
    renderAST(data.ast);

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
