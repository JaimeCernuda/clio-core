/**
 * Provenance dashboard — chronological event flow with tool calls and sub-agent sections.
 */
(function () {
  "use strict";

  let currentSession = null;
  let eventSource = null;
  let allNodes = [];           // context graph diff nodes (for analysis charts)
  let allInteractions = [];    // full interaction records (for tool call names + response text)
  let childData = {};          // {session_id: {nodes, interactions}}
  let sessionPollTimer = null;
  let analysisCache = {};
  let analysisLoading = false;
  let activeChartsTab = "overview";
  let knownSessions = [];

  const BADGE_CLASS = {
    conversation_start: "badge-start",
    continuation: "badge-continuation",
    tool_execution: "badge-tool",
    compression: "badge-compression",
  };

  const BADGE_LABEL = {
    conversation_start: "start",
    continuation: "continuation",
    tool_execution: "tool call",
    compression: "compression",
  };

  // ── Session list ─────────────────────────────────────────────────────

  function groupSessions(sessions) {
    const byId = Object.fromEntries(sessions.map((s) => [s.session_id, s]));
    const groups = [];
    for (const s of sessions) {
      const m = s.session_id.match(/^(.+)\.(\d+)$/);
      if (m && byId[m[1]]) {
        // child — attached to parent below
      } else {
        groups.push({ ...s, children: [] });
      }
    }
    for (const s of sessions) {
      const m = s.session_id.match(/^(.+)\.(\d+)$/);
      if (m && byId[m[1]]) {
        const parent = groups.find((g) => g.session_id === m[1]);
        if (parent) parent.children.push(s);
      }
    }
    return groups;
  }

  function renderSessionItem(s, isChild) {
    const cls = [
      "session-item",
      isChild ? "session-child" : "",
      s.session_id === currentSession ? "active" : "",
    ].filter(Boolean).join(" ");
    return `<li class="${cls}" data-id="${s.session_id}">
      <div class="session-id">${s.session_id}</div>
      <div class="session-count">${s.count} interaction${s.count !== 1 ? "s" : ""}</div>
    </li>`;
  }

  function loadSessions() {
    fetch("/api/sessions")
      .then((r) => r.json())
      .then((data) => {
        const list = document.getElementById("session-list");
        const sessions = data.sessions || [];
        knownSessions = sessions;
        if (sessions.length === 0) {
          list.innerHTML = '<li class="no-data">No sessions yet</li>';
          return;
        }
        const groups = groupSessions(sessions);
        list.innerHTML = groups
          .map((g) => {
            let html = renderSessionItem(g, false);
            for (const child of g.children) html += renderSessionItem(child, true);
            return html;
          })
          .join("");
        list.querySelectorAll(".session-item").forEach((el) => {
          el.addEventListener("click", () => selectSession(el.dataset.id));
        });
      })
      .catch(() => {});
  }

  function selectSession(sessionId) {
    currentSession = sessionId;
    document.querySelectorAll(".session-item").forEach((el) => {
      el.classList.toggle("active", el.dataset.id === sessionId);
    });
    const flowSession = document.getElementById("flow-session");
    if (flowSession) flowSession.textContent = sessionId;

    allNodes = [];
    allInteractions = [];
    childData = {};

    startSSE(sessionId);
    loadFullGraph(sessionId);
    loadInteractions(sessionId);
    loadChildSessions(sessionId);
    analysisCache = {};
    loadAnalysis(sessionId);
  }

  function getChildSessionIds(parentId) {
    const prefix = parentId + ".";
    return knownSessions
      .filter((s) => s.session_id.startsWith(prefix))
      .map((s) => s.session_id);
  }

  function loadChildSessions(parentId) {
    const children = getChildSessionIds(parentId);
    for (const cid of children) {
      childData[cid] = { nodes: [], interactions: [] };
      fetch(`/api/session/${encodeURIComponent(cid)}/graph`)
        .then((r) => r.json())
        .then((d) => {
          childData[cid].nodes = parseItems(d.nodes || []);
          renderEventFlow();
        })
        .catch(() => {});
      fetch(`/api/session/${encodeURIComponent(cid)}/interactions`)
        .then((r) => r.json())
        .then((d) => {
          childData[cid].interactions = parseItems(d.interactions || []);
          renderEventFlow();
        })
        .catch(() => {});
    }
  }

  function parseItems(items) {
    return items.map((item) => {
      if (typeof item === "string") {
        try { return JSON.parse(item); } catch { return item; }
      }
      return item;
    });
  }

  // ── Data loading ──────────────────────────────────────────────────────

  function loadFullGraph(sessionId) {
    fetch(`/api/session/${encodeURIComponent(sessionId)}/graph`)
      .then((r) => r.json())
      .then((data) => {
        const nodes = parseItems(data.nodes || []);
        const seenSeqs = new Set(allNodes.map((n) => n.sequence_id));
        for (const n of nodes) {
          if (!seenSeqs.has(n.sequence_id)) {
            allNodes.push(n);
            seenSeqs.add(n.sequence_id);
          }
        }
        allNodes.sort((a, b) => a.sequence_id - b.sequence_id);
        renderEventFlow();
      })
      .catch(() => {});
  }

  function loadInteractions(sessionId) {
    fetch(`/api/session/${encodeURIComponent(sessionId)}/interactions`)
      .then((r) => r.json())
      .then((data) => {
        allInteractions = parseItems(data.interactions || []);
        allInteractions.sort((a, b) => (a.sequence_id || 0) - (b.sequence_id || 0));
        renderEventFlow();
      })
      .catch(() => {});
  }

  // ── SSE ───────────────────────────────────────────────────────────────

  function startSSE(sessionId) {
    if (eventSource) eventSource.close();
    document.getElementById("live-dot").style.display = "inline-block";
    eventSource = new EventSource(`/api/session/${encodeURIComponent(sessionId)}/stream`);
    eventSource.onmessage = function (e) {
      if (sessionId !== currentSession) return;
      try {
        const node = JSON.parse(e.data);
        if (!allNodes.find((n) => n.sequence_id === node.sequence_id)) {
          allNodes.push(node);
          allNodes.sort((a, b) => a.sequence_id - b.sequence_id);
          renderEventFlow();
        }
      } catch { /* ignore */ }
    };
    eventSource.onerror = function () {
      document.getElementById("live-dot").style.display = "none";
    };
  }

  // ── Event flow rendering ──────────────────────────────────────────────

  function fmtTokens(n) {
    if (!n && n !== 0) return "";
    if (Math.abs(n) >= 1000) return (n / 1000).toFixed(1) + "k";
    return String(n);
  }

  function fmtLatency(ms) {
    if (!ms) return "";
    return ms >= 1000 ? (ms / 1000).toFixed(1) + "s" : Math.round(ms) + "ms";
  }

  function truncate(s, len) {
    if (!s) return "";
    s = String(s);
    return s.length > len ? s.slice(0, len) + "…" : s;
  }

  function renderNode(node, interaction, isChild) {
    const seq = node.sequence_id;
    const eventType = node.event_type || "continuation";
    const badge = BADGE_CLASS[eventType] || "badge-continuation";
    const label = BADGE_LABEL[eventType] || eventType;
    const model = truncate(node.model || "", 36);
    const deltaIn = node.delta_input_tokens || node.delta_effective_input_tokens || 0;
    const deltaOut = node.delta_output_tokens || 0;
    const lat = fmtLatency(node.latency_ms);
    const stopReason = node.stop_reason || "";
    const childClass = isChild ? " ef-child-item" : "";

    // Tool calls from interaction
    let toolsHtml = "";
    const toolCalls = interaction?.response?.tool_calls || [];
    if (toolCalls.length) {
      const toolRows = toolCalls.map((tc) => {
        const inputStr = typeof tc.input === "object"
          ? JSON.stringify(tc.input)
          : String(tc.input || "");
        return `<div class="ef-tool">
          <span>🔧</span>
          <span class="ef-tool-name">${tc.name || "?"}</span>
          <span class="ef-tool-input">${truncate(inputStr, 90)}</span>
        </div>`;
      }).join("");
      toolsHtml = `<div class="ef-tools">${toolRows}</div>`;
    } else if (node.tool_call_count > 0) {
      // Graph node knows count but we don't have names yet
      toolsHtml = `<div class="ef-tools">
        <div class="ef-tool"><span>🔧</span><span class="ef-model">${node.tool_call_count} tool call${node.tool_call_count !== 1 ? "s" : ""}</span></div>
      </div>`;
    }

    // Response text preview (collapsed, shown on click)
    const respText = interaction?.response?.text || "";
    const detailHtml = respText
      ? `<div class="ef-detail" style="display:none;">
           <div class="ef-detail-text">${truncate(respText, 600)}</div>
         </div>`
      : "";

    return `<div class="ef-item${childClass}" data-seq="${seq}">
      <div class="ef-row">
        <span class="ef-seq">#${seq}</span>
        <span class="ef-badge ${badge}">${label}</span>
        <span class="ef-model">${model}</span>
        ${stopReason ? `<span class="ef-stop">${stopReason}</span>` : ""}
        <span class="ef-metrics">
          ${deltaIn !== 0 ? `<span class="ef-tok-in">↑${fmtTokens(deltaIn)}</span>` : ""}
          ${deltaOut ? `<span class="ef-tok-out">↓${fmtTokens(deltaOut)}</span>` : ""}
          ${lat ? `<span class="ef-lat">${lat}</span>` : ""}
        </span>
        ${(toolsHtml || detailHtml) ? `<span class="ef-chevron">▶</span>` : ""}
      </div>
      ${toolsHtml}
      ${detailHtml}
    </div>`;
  }

  function renderEventFlow() {
    const container = document.getElementById("event-flow");
    if (!container) return;
    if (!allNodes.length) {
      container.innerHTML = '<div class="no-data">No events yet</div>';
      return;
    }

    // Build interaction lookup by sequence_id
    const iMap = {};
    for (const ix of allInteractions) iMap[ix.sequence_id] = ix;

    // Render main session nodes
    let html = allNodes.map((n) => renderNode(n, iMap[n.sequence_id], false)).join("");

    // Render child sessions
    for (const [cid, cdata] of Object.entries(childData)) {
      if (!cdata.nodes.length) continue;
      const sorted = [...cdata.nodes].sort((a, b) => a.sequence_id - b.sequence_id);
      const ciMap = {};
      for (const ix of cdata.interactions) ciMap[ix.sequence_id] = ix;
      const childItems = sorted.map((n) => renderNode(n, ciMap[n.sequence_id], true)).join("");
      html += `<div class="ef-subagent-header" data-cid="${cid}">
        ↳ Sub-agent: <strong>${cid}</strong> &nbsp;(${sorted.length} event${sorted.length !== 1 ? "s" : ""}) ▶
      </div>
      <div class="ef-subagent-body collapsed" id="sub-${CSS.escape(cid)}">${childItems}</div>`;
    }

    container.innerHTML = html;

    // Click handlers: expand/collapse detail on each event item
    container.querySelectorAll(".ef-item").forEach((el) => {
      el.querySelector(".ef-row")?.addEventListener("click", () => {
        const isOpen = el.classList.toggle("open");
        const detail = el.querySelector(".ef-detail");
        if (detail) detail.style.display = isOpen ? "block" : "none";
      });
    });

    // Click handlers: expand/collapse sub-agent sections
    container.querySelectorAll(".ef-subagent-header").forEach((hdr) => {
      hdr.addEventListener("click", () => {
        const cid = hdr.dataset.cid;
        const body = document.getElementById(`sub-${CSS.escape(cid)}`);
        if (body) body.classList.toggle("collapsed");
      });
    });
  }

  // ── Analysis charts ───────────────────────────────────────────────────

  const DARK_LAYOUT = {
    paper_bgcolor: "#1e1e2e",
    plot_bgcolor: "#1e1e2e",
    font: { color: "#ccc", size: 10 },
    margin: { t: 28, b: 44, l: 48, r: 16 },
    legend: { orientation: "h", y: -0.35, font: { size: 9 } },
    xaxis: { gridcolor: "#2a2a3e", zerolinecolor: "#333" },
    yaxis: { gridcolor: "#2a2a3e", zerolinecolor: "#333" },
  };

  function loadAnalysis(sessionId) {
    if (analysisLoading) return;
    analysisLoading = true;
    const children = getChildSessionIds(sessionId);
    const url = children.length > 0
      ? `/api/session/${encodeURIComponent(sessionId)}/analysis?include=${children.map(encodeURIComponent).join(",")}`
      : `/api/session/${encodeURIComponent(sessionId)}/analysis`;
    fetch(url)
      .then((r) => r.json())
      .then((data) => {
        analysisCache[sessionId] = data;
        analysisLoading = false;
        renderAllCharts(data);
      })
      .catch(() => { analysisLoading = false; });
  }

  function renderAllCharts(data) {
    renderContextGrowth(data.context_growth);
    renderLatency(data.latency);
    renderTokenUsage(data.token_usage);
    renderCumulativeCost(data.cumulative_cost);
    renderLatencyHistogram(data.latency_histogram);
    if (activeChartsTab === "context-detail") {
      renderContextComposition(data.composition);
      renderTokenDeltas(data.token_deltas);
    }
  }

  function renderContextGrowth(d) {
    if (!d) return;
    const traces = Object.entries(d.series).map(([model, pts]) => ({
      x: pts.steps, y: pts.values, name: model, type: "scatter", mode: "lines+markers",
    }));
    Plotly.react("chart-context-growth", traces,
      Object.assign({}, DARK_LAYOUT, { title: { text: "Context Window Growth", font: { color: "#eee", size: 11 } } }),
      { responsive: true });
  }

  function renderLatency(d) {
    if (!d) return;
    const traces = [
      { x: d.steps, y: d.latency_ms, name: "Latency (ms)", type: "scatter", mode: "lines+markers", line: { color: "#ff9800" } },
      { x: d.steps, y: d.ttft_ms, name: "TTFT (ms)", type: "scatter", mode: "lines+markers", line: { color: "#26c6da" } },
    ];
    Plotly.react("chart-latency", traces,
      Object.assign({}, DARK_LAYOUT, { title: { text: "Latency Over Time", font: { color: "#eee", size: 11 } } }),
      { responsive: true });
  }

  function renderTokenUsage(d) {
    const el = document.getElementById("chart-token-usage");
    if (!el || !d || !d.steps.length) { if (el) el.innerHTML = ""; return; }
    const sign = (v) => (v >= 0 ? "+" : "") + v.toLocaleString();
    const rows = d.steps.map((step, i) => {
      const totalIn = d.delta_input[i] || 0;
      const totalOut = d.delta_output[i] || 0;
      return `<tr>
        <td>#${step}</td>
        <td class="tok-in">${sign(totalIn)}</td>
        <td class="tok-out">${sign(totalOut)}</td>
        <td>${(totalIn + totalOut).toLocaleString()}</td>
      </tr>`;
    }).join("");
    el.innerHTML = `
      <h4>Token Usage Per Step</h4>
      <div class="token-stats-scroll">
        <table class="token-stats-table">
          <thead><tr>
            <th>Step</th><th>Input &Delta;</th><th>Output &Delta;</th><th>Total</th>
          </tr></thead>
          <tbody>${rows}</tbody>
        </table>
      </div>`;
  }

  function renderCumulativeCost(d) {
    if (!d) return;
    Plotly.react("chart-cumulative-cost",
      [{ x: d.steps, y: d.values, name: "Cost ($)", type: "scatter", mode: "lines+markers", line: { color: "#ffa726" } }],
      Object.assign({}, DARK_LAYOUT, { title: { text: "Cumulative Cost", font: { color: "#eee", size: 11 } } }),
      { responsive: true });
  }

  function renderLatencyHistogram(d) {
    if (!d || !d.values.length) return;
    Plotly.react("chart-latency-histogram",
      [{ x: d.values, type: "histogram", nbinsx: 20, marker: { color: "#42a5f5" }, name: "Latency" }],
      Object.assign({}, DARK_LAYOUT, {
        title: { text: "Latency Histogram", font: { color: "#eee", size: 11 } },
        shapes: [{ type: "line", x0: d.mean, x1: d.mean, y0: 0, y1: 1, yref: "paper", line: { color: "#ff9800", dash: "dash" } }],
      }),
      { responsive: true });
  }

  function renderContextComposition(d) {
    if (!d) return;
    const colorMap = {
      system_prompt: "#7e57c2", user_messages: "#42a5f5",
      assistant_messages: "#66bb6a", tool_calls: "#ffa726", tool_results: "#ef5350",
    };
    const traces = Object.entries(colorMap).map(([key, color]) => ({
      x: d.steps, y: d[key], name: key.replace(/_/g, " "),
      type: "scatter", mode: "lines", stackgroup: "one", fill: "tonexty", line: { color },
    }));
    Plotly.react("chart-context-composition", traces,
      Object.assign({}, DARK_LAYOUT, {
        title: { text: "Context Composition", font: { color: "#eee", size: 11 } },
        margin: { t: 28, b: 60, l: 48, r: 16 },
      }),
      { responsive: true });
  }

  function renderTokenDeltas(d) {
    if (!d) return;
    const EVENT_COLORS = {
      conversation_start: "#4caf50", continuation: "#42a5f5",
      tool_execution: "#ff9800", compression: "#e91e63",
    };
    const colors = d.event_types.map((et) => EVENT_COLORS[et] || EVENT_COLORS.continuation);
    Plotly.react("chart-token-deltas",
      [{ x: d.steps, y: d.values, type: "bar", marker: { color: colors }, name: "Token Delta" }],
      Object.assign({}, DARK_LAYOUT, { title: { text: "Token Deltas", font: { color: "#eee", size: 11 } } }),
      { responsive: true });
  }

  // ── Init ──────────────────────────────────────────────────────────────

  document.querySelectorAll(".charts-tab-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      activeChartsTab = btn.dataset.tab;
      document.querySelectorAll(".charts-tab-btn").forEach((b) =>
        b.classList.toggle("active", b === btn)
      );
      document.querySelectorAll(".charts-tab-content").forEach((c) =>
        c.classList.toggle("active", c.id === "tab-" + btn.dataset.tab)
      );
      window.dispatchEvent(new Event("resize"));
      if (activeChartsTab === "context-detail" && currentSession && analysisCache[currentSession]) {
        renderContextComposition(analysisCache[currentSession].composition);
        renderTokenDeltas(analysisCache[currentSession].token_deltas);
      }
    });
  });

  document.getElementById("charts-refresh")?.addEventListener("click", () => {
    if (!currentSession) return;
    delete analysisCache[currentSession];
    loadAnalysis(currentSession);
  });

  loadSessions();
  sessionPollTimer = setInterval(loadSessions, 5000);

  const connStatus = document.getElementById("conn-status");
  if (connStatus) connStatus.textContent = "Connected";
})();
