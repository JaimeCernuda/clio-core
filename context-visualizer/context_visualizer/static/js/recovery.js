/**
 * Recovery dashboard — cross-agent error recovery visualization.
 */
(function () {
  "use strict";

  let currentSession = null;
  let sessionPollTimer = null;
  let graphPollTimer = null;

  const EVENT_TYPE_COLOR = {
    error_report: "#ef4444",
    checkpoint_request: "#3b82f6",
    fix_deployed: "#22c55e",
    info: "#eab308",
  };

  // ── Session list ─────────────────────────────────────────────────────

  function countBadges(events) {
    const counts = {};
    for (const ev of events) {
      if (ev.acknowledged) continue;
      const t = ev.event_type || "info";
      counts[t] = (counts[t] || 0) + 1;
    }
    return counts;
  }

  function renderBadges(counts) {
    const typeClass = {
      error_report: "badge-error",
      checkpoint_request: "badge-checkpoint",
      fix_deployed: "badge-fix",
      info: "badge-info",
    };
    return Object.entries(counts)
      .map(([t, n]) => `<span class="evt-badge ${typeClass[t] || 'badge-info'}">${n}</span>`)
      .join("");
  }

  function loadSessions() {
    Promise.all([
      fetch("/api/sessions").then((r) => r.json()),
      fetch("/api/recovery/graph").then((r) => r.json()),
    ])
      .then(([sessData, graphData]) => {
        const sessions = sessData.sessions || [];
        const eventMap = {};
        for (const node of graphData.nodes || []) {
          eventMap[node.id] = { count: node.event_count || 0, events: [] };
        }

        // Fetch pending event counts for each session that has recovery activity
        const fetches = sessions
          .filter((s) => eventMap[s.session_id])
          .map((s) =>
            fetch(`/api/session/${encodeURIComponent(s.session_id)}/recovery_events`)
              .then((r) => r.json())
              .then((d) => ({ sid: s.session_id, events: d.events || [] }))
              .catch(() => ({ sid: s.session_id, events: [] }))
          );

        return Promise.all(fetches).then((results) => {
          const eventsPerSession = {};
          for (const r of results) {
            eventsPerSession[r.sid] = r.events;
          }
          renderSessionList(sessions, eventsPerSession);
        });
      })
      .catch(() => {
        document.getElementById("session-list").innerHTML =
          '<li class="no-data">Failed to load sessions</li>';
      });
  }

  function renderSessionList(sessions, eventsPerSession) {
    const list = document.getElementById("session-list");
    if (!sessions.length) {
      list.innerHTML = '<li class="no-data">No sessions yet</li>';
      return;
    }
    list.innerHTML = sessions
      .map((s) => {
        const events = eventsPerSession[s.session_id] || [];
        const badges = renderBadges(countBadges(events));
        const active = s.session_id === currentSession ? " active" : "";
        return `<li class="session-item${active}" data-id="${s.session_id}">
          <div class="session-info">
            <div class="session-id">${s.session_id}</div>
            <div class="session-count">${s.count || 0} interaction${s.count !== 1 ? "s" : ""}</div>
          </div>
          ${badges ? `<div class="badge-group">${badges}</div>` : ""}
        </li>`;
      })
      .join("");

    list.querySelectorAll(".session-item").forEach((el) => {
      el.addEventListener("click", () => selectSession(el.dataset.id));
    });
  }

  // ── Sankey graph ─────────────────────────────────────────────────────

  function loadRecoveryGraph() {
    fetch("/api/recovery/graph")
      .then((r) => r.json())
      .then(renderSankey)
      .catch(() => {
        document.getElementById("sankey-chart").innerHTML =
          '<div class="no-data">Failed to load graph</div>';
      });
  }

  function renderSankey(data) {
    const nodes = data.nodes || [];
    const edges = data.edges || [];
    const container = document.getElementById("sankey-chart");

    if (!nodes.length) {
      container.innerHTML = '<div class="no-data">No recovery events yet</div>';
      return;
    }

    const nodeLabels = nodes.map((n) => n.label || n.id);
    const nodeIndex = Object.fromEntries(nodes.map((n, i) => [n.id, i]));

    const sources = [];
    const targets = [];
    const values = [];
    const linkColors = [];

    for (const edge of edges) {
      const si = nodeIndex[edge.source];
      const ti = nodeIndex[edge.target];
      if (si === undefined || ti === undefined) continue;
      sources.push(si);
      targets.push(ti);
      values.push(edge.count || 1);
      linkColors.push("rgba(239,68,68,0.35)");
    }

    const trace = {
      type: "sankey",
      orientation: "h",
      node: {
        pad: 15,
        thickness: 20,
        line: { color: "#444", width: 0.5 },
        label: nodeLabels,
        color: nodes.map((n) =>
          n.event_count > 0 ? "#1e3a5f" : "#1a1a2e"
        ),
        font: { color: "#eee", size: 11 },
      },
      link: {
        source: sources,
        target: targets,
        value: values,
        color: linkColors,
      },
    };

    const layout = {
      paper_bgcolor: "rgba(0,0,0,0)",
      plot_bgcolor: "rgba(0,0,0,0)",
      font: { color: "#eee" },
      margin: { l: 10, r: 10, t: 10, b: 10 },
    };

    Plotly.react(container, [trace], layout, { responsive: true, displayModeBar: false });
  }

  // ── Session detail (timeline + table) ────────────────────────────────

  function selectSession(sid) {
    currentSession = sid;

    // Update active state in list
    document.querySelectorAll(".session-item").forEach((el) => {
      el.classList.toggle("active", el.dataset.id === sid);
    });

    document.getElementById("timeline-session").textContent = sid;
    document.getElementById("timeline-chart").innerHTML =
      '<div class="no-data">Loading...</div>';
    document.getElementById("event-table-wrap").innerHTML =
      '<div class="no-data">Loading...</div>';

    fetch(`/api/session/${encodeURIComponent(sid)}/recovery_events`)
      .then((r) => r.json())
      .then((data) => {
        const events = data.events || [];
        renderTimeline(events);
        renderEventTable(sid, events);
      })
      .catch(() => {
        document.getElementById("timeline-chart").innerHTML =
          '<div class="no-data">Failed to load events</div>';
        document.getElementById("event-table-wrap").innerHTML =
          '<div class="no-data">Failed to load events</div>';
      });
  }

  function renderTimeline(events) {
    const container = document.getElementById("timeline-chart");
    if (!events.length) {
      container.innerHTML = '<div class="no-data">No events for this session</div>';
      return;
    }

    const types = [...new Set(events.map((e) => e.event_type || "info"))];
    const traces = types.map((t) => {
      const evs = events.filter((e) => (e.event_type || "info") === t);
      return {
        type: "scatter",
        mode: "markers",
        name: t,
        x: evs.map((e) => e.timestamp || ""),
        y: evs.map(() => t),
        text: evs.map((e) => {
          const p = e.payload || {};
          return `${e.source_session_id || "?"} → ${e.target_session_id || "?"}<br>${p.description || ""}`;
        }),
        hoverinfo: "text+x",
        marker: {
          size: 10,
          color: EVENT_TYPE_COLOR[t] || "#aaa",
          opacity: evs.map((e) => (e.acknowledged ? 0.35 : 1.0)),
        },
      };
    });

    const layout = {
      paper_bgcolor: "rgba(0,0,0,0)",
      plot_bgcolor: "rgba(0,0,0,0)",
      xaxis: { color: "#666", gridcolor: "#222" },
      yaxis: { color: "#888", automargin: true },
      margin: { l: 10, r: 10, t: 10, b: 40 },
      legend: { font: { color: "#aaa", size: 10 }, bgcolor: "rgba(0,0,0,0)" },
      showlegend: true,
    };

    Plotly.react(container, traces, layout, { responsive: true, displayModeBar: false });
  }

  function renderEventTable(sid, events) {
    const wrap = document.getElementById("event-table-wrap");
    if (!events.length) {
      wrap.innerHTML = '<div class="no-data">No events</div>';
      return;
    }

    const rows = events
      .map((ev) => {
        const p = ev.payload || {};
        const acked = ev.acknowledged;
        const tdClass = acked ? ' class="acked"' : "";
        const src = ev.source_session_id || "";
        const seq = ev.source_sequence_id != null ? ev.source_sequence_id : "";
        const chkLink =
          src && seq !== ""
            ? `<a class="chk-link" href="/api/checkpoint/${encodeURIComponent(src)}/${seq}" target="_blank">view</a>`
            : "";
        return `<tr>
          <td${tdClass}>${ev.event_type || ""}</td>
          <td${tdClass}>${src}</td>
          <td${tdClass}>${ev.target_session_id || ""}</td>
          <td${tdClass}>${p.description || ""}</td>
          <td${tdClass}>${(ev.timestamp || "").replace("T", " ").replace("Z", "")}</td>
          <td>${chkLink}</td>
          <td>${
            acked
              ? '<span style="color:#555;">✓</span>'
              : `<button class="ack-btn" data-sid="${sid}" data-blob="${ev.blob_name}">Ack</button>`
          }</td>
        </tr>`;
      })
      .join("");

    wrap.innerHTML = `<table class="event-table">
      <thead><tr>
        <th>Type</th><th>Source</th><th>Target</th><th>Description</th>
        <th>Timestamp</th><th>Checkpoint</th><th>Action</th>
      </tr></thead>
      <tbody>${rows}</tbody>
    </table>`;

    wrap.querySelectorAll(".ack-btn").forEach((btn) => {
      btn.addEventListener("click", () => ackEvent(btn.dataset.sid, btn.dataset.blob));
    });
  }

  function ackEvent(sid, blob) {
    fetch(`/api/session/${encodeURIComponent(sid)}/recovery_events/${encodeURIComponent(blob)}/ack`, {
      method: "POST",
    })
      .then(() => selectSession(sid))
      .catch(() => {});
  }

  // ── Init and polling ─────────────────────────────────────────────────

  function init() {
    loadSessions();
    loadRecoveryGraph();

    sessionPollTimer = setInterval(loadSessions, 5000);
    graphPollTimer = setInterval(loadRecoveryGraph, 10000);
  }

  document.addEventListener("DOMContentLoaded", init);
})();
