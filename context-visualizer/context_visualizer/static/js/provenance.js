/**
 * Provenance dashboard — SSE-powered live context graph visualization.
 */
(function () {
  "use strict";

  let currentSession = null;
  let eventSource = null;
  let allNodes = [];
  let sessionPollTimer = null;

  // Badge class mapping
  const BADGE_CLASS = {
    conversation_start: "badge-start",
    continuation: "badge-continuation",
    tool_execution: "badge-tool",
    compression: "badge-compression",
  };

  // ── Session list ─────────────────────────────────────────────────────
  function loadSessions() {
    fetch("/api/sessions")
      .then((r) => r.json())
      .then((data) => {
        const list = document.getElementById("session-list");
        const sessions = data.sessions || [];
        if (sessions.length === 0) {
          list.innerHTML = '<li class="no-data">No sessions yet</li>';
          return;
        }
        list.innerHTML = sessions
          .map(
            (s) =>
              `<li class="session-item ${s.session_id === currentSession ? "active" : ""}"
               data-id="${s.session_id}">
             <div class="session-id">${s.session_id}</div>
             <div class="session-count">${s.count} interaction${s.count !== 1 ? "s" : ""}</div>
           </li>`
          )
          .join("");
        list.querySelectorAll(".session-item").forEach((el) => {
          el.addEventListener("click", () => selectSession(el.dataset.id));
        });
      })
      .catch(() => {});
  }

  function selectSession(sessionId) {
    currentSession = sessionId;
    // Update active state
    document.querySelectorAll(".session-item").forEach((el) => {
      el.classList.toggle("active", el.dataset.id === sessionId);
    });
    // Reset state and start SSE
    allNodes = [];
    startSSE(sessionId);
    // Also load full graph (in case SSE misses early events)
    loadFullGraph(sessionId);
  }

  function loadFullGraph(sessionId) {
    fetch(`/api/session/${sessionId}/graph`)
      .then((r) => r.json())
      .then((data) => {
        const nodes = data.nodes || [];
        // Merge with existing (avoid duplicates)
        const seenSeqs = new Set(allNodes.map((n) => n.sequence_id));
        for (const n of nodes) {
          if (!seenSeqs.has(n.sequence_id)) {
            allNodes.push(n);
            seenSeqs.add(n.sequence_id);
          }
        }
        allNodes.sort((a, b) => a.sequence_id - b.sequence_id);
        updateChart();
        updateTimeline();
      })
      .catch(() => {});
  }

  // ── SSE ──────────────────────────────────────────────────────────────
  function startSSE(sessionId) {
    if (eventSource) {
      eventSource.close();
    }
    document.getElementById("live-dot").style.display = "inline-block";
    eventSource = new EventSource(`/api/session/${sessionId}/stream`);
    eventSource.onmessage = function (e) {
      if (sessionId !== currentSession) return;
      try {
        const node = JSON.parse(e.data);
        // Avoid duplicates
        if (!allNodes.find((n) => n.sequence_id === node.sequence_id)) {
          allNodes.push(node);
          allNodes.sort((a, b) => a.sequence_id - b.sequence_id);
          updateChart();
          updateTimeline();
        }
      } catch (err) {
        // ignore parse errors
      }
    };
    eventSource.onerror = function () {
      document.getElementById("live-dot").style.display = "none";
    };
  }

  // ── Plotly chart ─────────────────────────────────────────────────────
  function updateChart() {
    const container = document.getElementById("token-chart");
    if (allNodes.length === 0) {
      container.innerHTML = '<div class="no-data">No graph data</div>';
      return;
    }

    const turns = allNodes.map((n) => n.turn_number || n.sequence_id);
    const inputTokens = allNodes.map((n) => n.total_input_tokens || 0);
    const outputTokens = allNodes.map((n) => n.total_output_tokens || 0);
    const costs = allNodes.map((n) => n.cost_usd || 0);

    const traces = [
      {
        x: turns,
        y: inputTokens,
        name: "Input Tokens",
        type: "scatter",
        mode: "lines+markers",
        line: { color: "#42a5f5" },
      },
      {
        x: turns,
        y: outputTokens,
        name: "Output Tokens",
        type: "scatter",
        mode: "lines+markers",
        line: { color: "#66bb6a" },
      },
      {
        x: turns,
        y: costs,
        name: "Cost ($)",
        type: "scatter",
        mode: "lines+markers",
        yaxis: "y2",
        line: { color: "#ffa726", dash: "dot" },
      },
    ];

    const layout = {
      title: { text: `Context Graph: ${currentSession}`, font: { color: "#eee", size: 14 } },
      paper_bgcolor: "#1a1a2e",
      plot_bgcolor: "#1a1a2e",
      font: { color: "#ccc" },
      xaxis: { title: "Turn", gridcolor: "#2a2a3e" },
      yaxis: { title: "Tokens", gridcolor: "#2a2a3e" },
      yaxis2: { title: "Cost ($)", overlaying: "y", side: "right", gridcolor: "#2a2a3e" },
      legend: { orientation: "h", y: -0.2 },
      margin: { t: 40, b: 60, l: 60, r: 60 },
    };

    Plotly.react(container, traces, layout, { responsive: true });
  }

  // ── Event timeline ───────────────────────────────────────────────────
  function updateTimeline() {
    const list = document.getElementById("event-list");
    if (allNodes.length === 0) {
      list.innerHTML = '<div class="no-data">No events</div>';
      return;
    }

    // Show most recent first
    const sorted = [...allNodes].reverse();
    list.innerHTML = sorted
      .map((n) => {
        const badge = BADGE_CLASS[n.event_type] || "badge-continuation";
        const deltaIn = n.delta_input_tokens || 0;
        const deltaOut = n.delta_output_tokens || 0;
        const sign = (v) => (v >= 0 ? "+" : "") + v;
        return `<div class="event-item">
        <span class="event-seq">#${n.sequence_id}</span>
        <span class="event-badge ${badge}">${n.event_type || "unknown"}</span>
        <span class="event-details">
          in: ${sign(deltaIn)} / out: ${sign(deltaOut)} tokens
          ${n.latency_ms ? ` | ${Math.round(n.latency_ms)}ms` : ""}
          ${n.model ? ` | ${n.model}` : ""}
        </span>
      </div>`;
      })
      .join("");
  }

  // ── Init ─────────────────────────────────────────────────────────────
  loadSessions();
  sessionPollTimer = setInterval(loadSessions, 5000);

  // Update connection status
  const connStatus = document.getElementById("conn-status");
  if (connStatus) connStatus.textContent = "Connected";
})();
