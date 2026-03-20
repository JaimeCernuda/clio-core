/**
 * Call Graph visualization — vanilla JS port of GraphView.tsx.
 * Renders aggregate (column-based topology) and sequential (horizontal timeline) views.
 * Supports multi-session "Full workflow" mode that merges parent + child (subagent) sessions.
 */
(function () {
  "use strict";

  // ── State ────────────────────────────────────────────────────────────────
  let currentSession = null;
  let viewMode = "aggregate";   // "aggregate" | "sequential"
  let workflowScope = false;    // false = this session only, true = full workflow
  let callGraphData = null;     // {nodes, edges, timeline}
  let toolSequenceData = null;  // array of ToolCallStep (with sessionId, isSubagent)
  let selectedStep = null;      // selected LLM card step (sequential view)
  let currentSessionCount = 0; // last known interaction count for auto-refresh

  // ── Aggregate view constants ──────────────────────────────────────────────
  const NODE_W = 130, NODE_H = 44, COL_GAP = 160, ROW_GAP = 64, PADDING = 32;
  // subagent at col 0 (stacks with agent), rest follow original TypeScript order
  const TYPE_ORDER = { agent: 0, subagent: 0, tool: 1, proxy: 2, provider: 3, model: 4 };
  const TYPE_COLORS = {
    agent:    { fill: "#1e3a5f", stroke: "#3b82f6", text: "#93c5fd" },
    subagent: { fill: "#2d1b4e", stroke: "#8b5cf6", text: "#c4b5fd" },
    tool:     { fill: "#3b1f00", stroke: "#f97316", text: "#fdba74" },
    proxy:    { fill: "#1a2e1a", stroke: "#22c55e", text: "#86efac" },
    provider: { fill: "#2d1b4e", stroke: "#a855f7", text: "#d8b4fe" },
    model:    { fill: "#1e3030", stroke: "#14b8a6", text: "#5eead4" },
  };

  // ── Sequential view constants ─────────────────────────────────────────────
  const LLM_W = 170, LLM_H = 90, LLM_CALL_H = 44;
  const TOOL_W = 140, TOOL_H = 66;
  const AGENT_W = 130, AGENT_H = 44;
  const H_GAP = 55, V_GAP = 14, SEQ_PAD = 32;
  // Extra top space to accommodate session labels in workflow mode
  const SESSION_LABEL_H = 18;

  // ── Helper functions ──────────────────────────────────────────────────────

  function esc(str) {
    return String(str)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function fmtMs(ms) {
    if (ms == null) return "—";
    return ms >= 1000 ? (ms / 1000).toFixed(2) + "s" : Math.round(ms) + "ms";
  }

  function latencyColor(avgMs) {
    if (avgMs == null) return "#374151";
    if (avgMs < 500) return "#166534";
    if (avgMs < 2000) return "#854d0e";
    return "#7f1d1d";
  }

  function edgeColor(errorRate) {
    if (errorRate > 0.1) return "#ef4444";
    if (errorRate > 0.01) return "#f59e0b";
    return "#22c55e";
  }

  function edgeWidth(callCount) {
    return Math.min(8, Math.max(1.5, 1.5 + Math.log10(Math.max(1, callCount)) * 2));
  }

  function edgePath(x1, y1, x2, y2) {
    const cx1 = x1 + (x2 - x1) * 0.45;
    const cx2 = x1 + (x2 - x1) * 0.55;
    return `M ${x1} ${y1} C ${cx1} ${y1} ${cx2} ${y2} ${x2} ${y2}`;
  }

  function isToolResultError(content) {
    const head = content.slice(0, 100).toLowerCase().trimStart();
    return (
      head.startsWith("error:") || head.startsWith("error ") ||
      head.includes("does not exist") || head.includes("no such file") ||
      head.includes("permission denied") || head.includes("not found.") ||
      head.startsWith("traceback (")
    );
  }

  function markerDefs() {
    return ["22c55e", "f59e0b", "ef4444", "4b5563", "f97316", "8b5cf6"].map(c =>
      `<marker id="arrow-${c}" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto">` +
      `<polygon points="0 0, 8 3, 0 6" fill="#${c}"/></marker>`
    ).join("");
  }

  // ── Aggregate layout ──────────────────────────────────────────────────────

  function computeAggLayout(nodes) {
    const cols = new Map();
    for (const n of nodes) {
      const col = TYPE_ORDER[n.type] ?? 5;
      if (!cols.has(col)) cols.set(col, []);
      cols.get(col).push(n);
    }
    const layout = [];
    for (const col of Array.from(cols.keys()).sort((a, b) => a - b)) {
      const group = cols.get(col);
      const x = PADDING + col * (NODE_W + COL_GAP);
      const totalH = group.length * NODE_H + (group.length - 1) * (ROW_GAP - NODE_H);
      const startY = PADDING + Math.max(0, (200 - totalH) / 2);
      group.forEach((node, i) => layout.push({ node, x, y: startY + i * ROW_GAP }));
    }
    return layout;
  }

  // ── Sequential layout ─────────────────────────────────────────────────────

  function computeSeqLayout(toolSequence) {
    // Build global result map — covers all sessions (cross-session tool IDs are unique)
    const globalResultMap = new Map();
    for (const step of toolSequence) {
      for (const r of step.toolResults) {
        if (r.toolCallId) globalResultMap.set(r.toolCallId, r.content);
      }
    }

    // Detect if there are multiple sessions (workflow mode)
    const hasMultipleSessions = toolSequence.some(s => s.isSubagent);
    const topPad = SEQ_PAD + (hasMultipleSessions ? SESSION_LABEL_H : 0);

    const maxParallel = Math.max(1, ...toolSequence.map(s => s.toolCalls.length));
    const maxToolsH = maxParallel * TOOL_H + Math.max(0, maxParallel - 1) * V_GAP;
    const contentH = Math.max(LLM_H, AGENT_H, maxToolsH);
    const svgH = topPad + SEQ_PAD + contentH;
    const centerY = topPad + contentH / 2;

    const agentX = SEQ_PAD;
    const agentY = centerY - AGENT_H / 2;
    let curX = agentX + AGENT_W + H_GAP;

    const llmCards = [];
    const toolGroups = [];
    // Session boundary info: [{sessionId, startX, isSubagent}]
    const sessionBoundaries = [];
    let prevSessionId = null;

    toolSequence.forEach((step, si) => {
      const sid = step.sessionId || "";
      if (sid !== prevSessionId) {
        sessionBoundaries.push({ sessionId: sid, startX: curX, isSubagent: !!step.isSubagent });
        prevSessionId = sid;
      }
      llmCards.push({ step, x: curX, y: centerY - LLM_H / 2, stepNum: si + 1 });
      curX += LLM_W + H_GAP;

      if (step.toolCalls.length > 0) {
        const count = step.toolCalls.length;
        const totalH = count * TOOL_H + Math.max(0, count - 1) * V_GAP;
        const startY = centerY - totalH / 2;
        const nodes = step.toolCalls.map((tc, j) => {
          const content = tc.id != null ? (globalResultMap.get(tc.id) ?? null) : null;
          return {
            id: `tool-${si}-${j}`,
            name: tc.name ?? "?",
            x: curX,
            y: startY + j * (TOOL_H + V_GAP),
            hasResult: content != null,
            resultContent: content,
            isResultError: content != null && isToolResultError(content),
            stepIndex: si,
          };
        });
        toolGroups.push({ nodes, stepIndex: si, parallelCount: count });
        curX += TOOL_W + H_GAP;
      }
    });

    const svgW = curX - H_GAP + SEQ_PAD;

    const edges = [];
    if (llmCards.length > 0) {
      edges.push({
        x1: agentX + AGENT_W - 2, y1: centerY,
        x2: llmCards[0].x + 10, y2: llmCards[0].y + LLM_H / 2,
        color: "#22c55e", dashed: false,
      });
    }

    for (let i = 0; i < llmCards.length; i++) {
      const card = llmCards[i];
      const group = toolGroups.find(g => g.stepIndex === i);
      if (group) {
        for (const tn of group.nodes) {
          edges.push({
            x1: card.x + LLM_W - 2, y1: card.y + LLM_H / 2,
            x2: tn.x + 10, y2: tn.y + TOOL_H / 2,
            color: "#f97316", dashed: false,
          });
        }
        if (i + 1 < llmCards.length) {
          const nextCard = llmCards[i + 1];
          for (const tn of group.nodes) {
            edges.push({
              x1: tn.x + TOOL_W - 2, y1: tn.y + TOOL_H / 2,
              x2: nextCard.x + 10, y2: nextCard.y + LLM_H / 2,
              color: "#22c55e", dashed: false,
            });
          }
        }
      } else if (i + 1 < llmCards.length) {
        // Different session = dashed purple; same session = dashed gray
        const nextStep = llmCards[i + 1].step;
        const sameSession = card.step.sessionId === nextStep.sessionId;
        edges.push({
          x1: card.x + LLM_W - 2, y1: card.y + LLM_H / 2,
          x2: llmCards[i + 1].x + 10, y2: llmCards[i + 1].y + LLM_H / 2,
          color: sameSession ? "#4b5563" : "#8b5cf6", dashed: true,
        });
      }
    }

    return { llmCards, toolGroups, agentX, agentY, edges, svgW, svgH, centerY, sessionBoundaries, topPad };
  }

  // ── SVG builders ──────────────────────────────────────────────────────────

  function buildAggregateSVG(data) {
    const { nodes, edges } = data;
    const layout = computeAggLayout(nodes);

    const posMap = new Map();
    for (const { node, x, y } of layout) {
      posMap.set(node.id, { x, y, cx: x + NODE_W / 2, cy: y + NODE_H / 2 });
    }

    const svgW = layout.length > 0 ? Math.max(...layout.map(l => l.x + NODE_W)) + PADDING * 2 : 400;
    const svgH = layout.length > 0 ? Math.max(...layout.map(l => l.y + NODE_H)) + PADDING * 2 : 200;

    let edgesSvg = "";
    for (const edge of edges) {
      const from = posMap.get(edge.from);
      const to = posMap.get(edge.to);
      if (!from || !to) continue;
      const color = edgeColor(edge.errorRate);
      const mid = `arrow-${color.replace("#", "")}`;
      const x1 = from.x + NODE_W - 2;
      const y1 = from.cy;
      const x2 = to.x + 10;
      const y2 = to.cy;
      edgesSvg += `<path d="${edgePath(x1, y1, x2, y2)}" stroke="${color}"` +
        ` stroke-width="${edgeWidth(edge.callCount)}" fill="none" opacity="0.7"` +
        ` marker-end="url(#${mid})" class="cg-edge" style="cursor:pointer;"` +
        ` data-from="${esc(edge.from)}" data-to="${esc(edge.to)}"` +
        ` data-calls="${edge.callCount}" data-error-rate="${edge.errorRate}"` +
        ` data-avg-lat="${edge.avgLatencyMs ?? ""}" data-tokens="${edge.totalTokens}"` +
        ` data-cost="${edge.totalCostUsd}"/>`;
    }

    let nodesSvg = "";
    for (const { node, x, y } of layout) {
      const c = TYPE_COLORS[node.type] || TYPE_COLORS.agent;
      const bgColor = latencyColor(node.metrics.avgLatencyMs);
      const label = node.label.length > 14 ? node.label.slice(0, 13) + "…" : node.label;
      const avgLat = fmtMs(node.metrics.avgLatencyMs);
      nodesSvg += `<g class="cg-node" style="cursor:pointer;"` +
        ` data-id="${esc(node.id)}" data-label="${esc(node.label)}" data-type="${esc(node.type)}"` +
        ` data-calls="${node.metrics.callCount}" data-error-rate="${node.metrics.errorRate}"` +
        ` data-avg-lat="${node.metrics.avgLatencyMs ?? ""}" data-p95-lat="${node.metrics.p95LatencyMs ?? ""}"` +
        ` data-tokens="${node.metrics.totalTokens}" data-cost="${node.metrics.totalCostUsd}">` +
        `<rect x="${x}" y="${y}" width="${NODE_W}" height="${NODE_H}" rx="8" fill="${c.fill}" stroke="${c.stroke}" stroke-width="1.5"/>` +
        `<rect x="${x + 4}" y="${y + NODE_H - 5}" width="${NODE_W - 8}" height="3" rx="1.5" fill="${bgColor}" opacity="0.6"/>` +
        `<text x="${x + NODE_W / 2}" y="${y + 16}" text-anchor="middle" font-size="11" font-weight="600" fill="${c.text}">${esc(label)}</text>` +
        `<text x="${x + NODE_W / 2}" y="${y + 30}" text-anchor="middle" font-size="9" fill="#6b7280">${node.metrics.callCount} calls · ${avgLat}</text>` +
        `</g>`;
    }

    return `<svg width="${svgW}" height="${svgH}" id="cg-svg"` +
      ` style="display:block;background:#0d1117;border-radius:8px;border:1px solid #1f2937;">` +
      `<defs>${markerDefs()}</defs>${edgesSvg}${nodesSvg}</svg>`;
  }

  function buildSequentialSVG(toolSequence, selectedId) {
    if (!toolSequence.length) return null;
    const sl = computeSeqLayout(toolSequence);
    const { llmCards, toolGroups, agentX, agentY, edges, svgW, svgH, centerY, sessionBoundaries, topPad } = sl;

    // ── Edges
    const edgesSvg = edges.map(e => {
      const mid = `arrow-${e.color.replace("#", "")}`;
      return `<path d="${edgePath(e.x1, e.y1, e.x2, e.y2)}" stroke="${e.color}" stroke-width="1.5"` +
        `${e.dashed ? ' stroke-dasharray="4 3"' : ''} fill="none" opacity="0.7" marker-end="url(#${mid})"/>`;
    }).join("");

    // ── Session boundary markers (workflow mode)
    let boundariesSvg = "";
    for (let i = 0; i < sessionBoundaries.length; i++) {
      const b = sessionBoundaries[i];
      const labelColor = b.isSubagent ? "#c4b5fd" : "#60a5fa";
      const labelText = b.isSubagent ? `↳ Subagent: ${b.sessionId}` : b.sessionId || "Main session";
      // Vertical divider before each session (except the first)
      if (i > 0) {
        const divX = b.startX - H_GAP / 2;
        boundariesSvg += `<line x1="${divX}" y1="${topPad * 0.5}" x2="${divX}" y2="${svgH - SEQ_PAD * 0.5}"` +
          ` stroke="${labelColor}" stroke-width="0.6" stroke-dasharray="3 2" opacity="0.5"/>`;
      }
      boundariesSvg += `<text x="${b.startX}" y="${topPad - 4}" font-size="9" fill="${labelColor}" opacity="0.85">${esc(labelText)}</text>`;
    }

    // ── Parallel group rects
    const parallelSvg = toolGroups.filter(g => g.parallelCount > 1).map(g => {
      const ns = g.nodes;
      const rx = ns[0].x - 8, ry = ns[0].y - 8;
      const rw = TOOL_W + 16, rh = ns[ns.length - 1].y + TOOL_H - ns[0].y + 16;
      return `<g><rect x="${rx}" y="${ry}" width="${rw}" height="${rh}" rx="10"` +
        ` fill="#3b1f00" fill-opacity="0.4" stroke="#f97316" stroke-opacity="0.3" stroke-width="1"/>` +
        `<text x="${rx + rw / 2}" y="${ry - 4}" text-anchor="middle" font-size="9" fill="#f97316" fill-opacity="0.7">parallel</text></g>`;
    }).join("");

    // ── Agent node
    const ac = TYPE_COLORS.agent;
    const agentSvg = `<g>` +
      `<rect x="${agentX}" y="${agentY}" width="${AGENT_W}" height="${AGENT_H}" rx="8" fill="${ac.fill}" stroke="${ac.stroke}" stroke-width="1.5"/>` +
      `<text x="${agentX + AGENT_W / 2}" y="${agentY + 17}" text-anchor="middle" font-size="11" font-weight="600" fill="${ac.text}">Agent</text>` +
      `<text x="${agentX + AGENT_W / 2}" y="${agentY + 31}" text-anchor="middle" font-size="9" fill="#6b7280">start</text></g>`;

    // ── LLM cards
    const RESP_H = LLM_H - LLM_CALL_H;
    const cardsSvg = llmCards.map(({ step, x, y, stepNum }) => {
      const isSel = step.interactionId === selectedId && step.sessionId === (selectedStep ? selectedStep.sessionId : null);
      const isError = !!(step.error || (step.statusCode != null && step.statusCode >= 400));
      const statusColor = isError ? "#ef4444" : "#22c55e";
      // Subagent cards use a slightly different color scheme (purple-tinted)
      const isSub = !!step.isSubagent;
      const callFill = isSub ? "#241538" : "#1e3a5f";
      const respFill = isSub ? "#1a1030" : "#1e3030";
      const callStroke = isSel ? (isSub ? "#c4b5fd" : "#60a5fa") : (isSub ? "#7c3aed" : "#1d4ed8");
      const respStroke = isSel ? (isSub ? "#a78bfa" : "#2dd4bf") : (isSub ? "#5b21b6" : "#0f766e");
      const textColor = isSub ? "#c4b5fd" : "#93c5fd";
      const modelLabel = step.model
        ? (step.model.length > 19 ? step.model.slice(0, 18) + "…" : step.model)
        : step.provider;
      const respPreview = step.responseText
        ? step.responseText.slice(0, 20) + (step.responseText.length > 20 ? "…" : "")
        : isError ? ((step.error || "Error").slice(0, 20)) : "—";
      const outStr = step.outputTokens != null ? `  ·  ${step.outputTokens} out` : "";

      return `<g class="cg-llm-card" style="cursor:pointer;"` +
        ` data-iid="${esc(step.interactionId)}" data-sid="${esc(step.sessionId || "")}" data-step="${stepNum}">` +
        `<rect x="${x}" y="${y}" width="${LLM_W}" height="${LLM_CALL_H}" rx="8" fill="${callFill}" stroke="${callStroke}" stroke-width="${isSel ? 2 : 1.5}"/>` +
        `<rect x="${x}" y="${y + LLM_CALL_H - 8}" width="${LLM_W}" height="8" fill="${callFill}"/>` +
        `<circle cx="${x + LLM_W - 10}" cy="${y + 10}" r="4" fill="${statusColor}"/>` +
        `<text x="${x + 8}" y="${y + 12}" font-size="8" fill="#4b5563">Call #${stepNum} · via interceptor</text>` +
        `<text x="${x + LLM_W / 2}" y="${y + 27}" text-anchor="middle" font-size="11" font-weight="600" fill="${textColor}">${esc(modelLabel)}</text>` +
        (step.inputTokens != null ? `<text x="${x + LLM_W / 2}" y="${y + 39}" text-anchor="middle" font-size="9" fill="#4b5563">${step.inputTokens.toLocaleString()} tokens in</text>` : "") +
        `<line x1="${x}" y1="${y + LLM_CALL_H}" x2="${x + LLM_W}" y2="${y + LLM_CALL_H}" stroke="#374151" stroke-width="1"/>` +
        `<rect x="${x}" y="${y + LLM_CALL_H}" width="${LLM_W}" height="${RESP_H}" rx="8" fill="${respFill}" stroke="${respStroke}" stroke-width="${isSel ? 2 : 1.5}"/>` +
        `<rect x="${x}" y="${y + LLM_CALL_H}" width="${LLM_W}" height="8" fill="${respFill}"/>` +
        `<text x="${x + 8}" y="${y + LLM_CALL_H + 14}" font-size="9" fill="#6b7280">${esc(fmtMs(step.latencyMs))}${esc(outStr)}</text>` +
        `<text x="${x + LLM_W / 2}" y="${y + LLM_CALL_H + 28}" text-anchor="middle" font-size="9" fill="#5eead4" font-style="italic">${esc(respPreview)}</text>` +
        (step.toolCalls.length > 0 ? `<text x="${x + LLM_W / 2}" y="${y + LLM_CALL_H + 41}" text-anchor="middle" font-size="9" fill="#f97316">→ ${step.toolCalls.length} tool call${step.toolCalls.length !== 1 ? "s" : ""}</text>` : "") +
        (isSel ? `<rect x="${x - 2}" y="${y - 2}" width="${LLM_W + 4}" height="${LLM_H + 4}" rx="10" fill="none" stroke="${isSub ? "#c4b5fd" : "#60a5fa"}" stroke-width="2" stroke-dasharray="4 2" opacity="0.6"/>` : "") +
        `</g>`;
    }).join("");

    // ── Tool nodes
    const toolsSvg = toolGroups.flatMap(g => g.nodes).map(tn => {
      const stroke = !tn.hasResult ? "#f97316" : tn.isResultError ? "#ef4444" : "#22c55e";
      const nameColor = !tn.hasResult ? "#fdba74" : tn.isResultError ? "#fca5a5" : "#86efac";
      const statusColor = !tn.hasResult ? "#f97316" : tn.isResultError ? "#ef4444" : "#22c55e";
      const resultColor = tn.isResultError ? "#f87171" : "#6ee7b7";
      const DIVIDER_Y = tn.y + 36;
      const name = tn.name.length > 16 ? tn.name.slice(0, 15) + "…" : tn.name;
      const statusLabel = !tn.hasResult ? "⋯ pending" : tn.isResultError ? "⚠ error result" : "✓ executed";

      let previewSvg = "";
      if (tn.resultContent) {
        const preview = tn.resultContent.replace(/\s+/g, " ").trim().slice(0, 52);
        const line1 = preview.slice(0, 26);
        const line2 = preview.length > 26 ? preview.slice(26) + (tn.resultContent.length > 52 ? "…" : "") : "";
        previewSvg = `<line x1="${tn.x + 6}" y1="${DIVIDER_Y}" x2="${tn.x + TOOL_W - 6}" y2="${DIVIDER_Y}" stroke="${stroke}" stroke-width="0.5" stroke-opacity="0.4"/>` +
          (line1 ? `<text x="${tn.x + TOOL_W / 2}" y="${DIVIDER_Y + 12}" text-anchor="middle" font-size="8" fill="${resultColor}" font-style="italic">${esc(line1)}</text>` : "") +
          (line2 ? `<text x="${tn.x + TOOL_W / 2}" y="${DIVIDER_Y + 23}" text-anchor="middle" font-size="8" fill="${resultColor}" font-style="italic">${esc(line2)}</text>` : "");
      }

      return `<g>` +
        `<rect x="${tn.x}" y="${tn.y}" width="${TOOL_W}" height="${TOOL_H}" rx="8" fill="#3b1f00" stroke="${stroke}" stroke-width="1.5"/>` +
        `<circle cx="${tn.x + TOOL_W - 10}" cy="${tn.y + 10}" r="4" fill="${statusColor}"/>` +
        `<text x="${tn.x + TOOL_W / 2}" y="${tn.y + 14}" text-anchor="middle" font-size="10" font-weight="600" fill="${nameColor}">${esc(name)}</text>` +
        `<text x="${tn.x + TOOL_W / 2}" y="${tn.y + 27}" text-anchor="middle" font-size="9" fill="${statusColor}">${esc(statusLabel)}</text>` +
        previewSvg + `</g>`;
    }).join("");

    return `<svg width="${svgW}" height="${svgH}" id="cg-svg"` +
      ` style="display:block;background:#0d1117;border-radius:8px;border:1px solid #1f2937;">` +
      `<defs>${markerDefs()}</defs>${edgesSvg}${parallelSvg}${boundariesSvg}${agentSvg}${cardsSvg}${toolsSvg}</svg>`;
  }

  // ── Render ────────────────────────────────────────────────────────────────

  function renderCallGraph() {
    const container = document.getElementById("cg-svg-container");
    if (!container) return;

    if (!callGraphData && !toolSequenceData) {
      container.innerHTML = '<div class="no-data">Select a session to view call graph</div>';
      return;
    }

    const useSeq = viewMode === "sequential" && toolSequenceData && toolSequenceData.length > 0;

    const legendAgg = document.getElementById("cg-legend-agg");
    const legendSeq = document.getElementById("cg-legend-seq");
    if (legendAgg) legendAgg.style.display = useSeq ? "none" : "flex";
    if (legendSeq) legendSeq.style.display = useSeq ? "flex" : "none";

    let svgHtml = "";
    if (useSeq) {
      const selectedId = selectedStep ? selectedStep.interactionId : null;
      svgHtml = buildSequentialSVG(toolSequenceData, selectedId) || '<div class="no-data">No LLM calls found</div>';
    } else if (callGraphData && callGraphData.nodes) {
      svgHtml = buildAggregateSVG(callGraphData);
    } else {
      container.innerHTML = '<div class="no-data">No data available</div>';
      return;
    }

    container.innerHTML = svgHtml +
      `<div id="cg-tooltip" style="display:none;position:absolute;z-index:20;` +
      `background:#1f2937;border:1px solid #374151;border-radius:6px;` +
      `padding:8px 12px;font-size:11px;color:#d1d5db;pointer-events:none;max-width:220px;"></div>`;

    const svg = document.getElementById("cg-svg");
    const tooltip = document.getElementById("cg-tooltip");
    if (!svg || !tooltip) return;

    if (useSeq) {
      attachSeqEvents(svg);
    } else {
      attachAggEvents(svg, tooltip, container);
    }
  }

  function attachSeqEvents(svg) {
    svg.querySelectorAll(".cg-llm-card").forEach(el => {
      el.addEventListener("click", () => {
        const iid = el.dataset.iid;
        const sid = el.dataset.sid;
        const stepNum = parseInt(el.dataset.step);
        const step = toolSequenceData.find(s => s.interactionId === iid && (s.sessionId || "") === sid);
        if (!step) return;
        const isAlreadySelected = selectedStep &&
          selectedStep.interactionId === iid &&
          (selectedStep.sessionId || "") === sid;
        selectedStep = isAlreadySelected ? null : { ...step, stepNum };
        renderCallGraph();
        renderDetailPanel();
      });
    });
  }

  function attachAggEvents(svg, tooltip, container) {
    function positionTooltip(e) {
      const cr = container.getBoundingClientRect();
      let left = e.clientX - cr.left + 12;
      let top = e.clientY - cr.top - 10;
      const tw = tooltip.offsetWidth || 220;
      if (left + tw > cr.width) left = e.clientX - cr.left - tw - 8;
      tooltip.style.left = left + "px";
      tooltip.style.top = top + "px";
    }

    svg.querySelectorAll(".cg-node").forEach(el => {
      el.addEventListener("mouseenter", e => {
        const d = el.dataset;
        const avgLat = d.avgLat ? parseFloat(d.avgLat) : null;
        const p95Lat = d.p95Lat ? parseFloat(d.p95Lat) : null;
        const errPct = (parseFloat(d.errorRate) * 100).toFixed(1);
        const errColor = parseFloat(d.errorRate) > 0.05 ? "#f87171" : "#e5e7eb";
        tooltip.innerHTML =
          `<div style="font-weight:600;color:#f3f4f6;margin-bottom:4px;">${esc(d.label)}</div>` +
          `<div style="color:#9ca3af;text-transform:capitalize;margin-bottom:4px;">${esc(d.type)}</div>` +
          `<div>Calls: <span style="color:#e5e7eb;">${d.calls}</span></div>` +
          `<div>Avg latency: <span style="color:#e5e7eb;">${fmtMs(avgLat)}</span></div>` +
          `<div>p95: <span style="color:#e5e7eb;">${fmtMs(p95Lat)}</span></div>` +
          `<div>Tokens: <span style="color:#e5e7eb;">${parseInt(d.tokens).toLocaleString()}</span></div>` +
          `<div>Cost: <span style="color:#e5e7eb;">$${parseFloat(d.cost).toFixed(4)}</span></div>` +
          `<div>Error rate: <span style="color:${errColor};">${errPct}%</span></div>`;
        tooltip.style.display = "block";
        positionTooltip(e);
      });
      el.addEventListener("mousemove", positionTooltip);
      el.addEventListener("mouseleave", () => { tooltip.style.display = "none"; });
    });

    svg.querySelectorAll(".cg-edge").forEach(el => {
      el.addEventListener("mouseenter", e => {
        const d = el.dataset;
        const avgLat = d.avgLat ? parseFloat(d.avgLat) : null;
        const errPct = (parseFloat(d.errorRate) * 100).toFixed(1);
        const errColor = parseFloat(d.errorRate) > 0.05 ? "#f87171" : "#e5e7eb";
        tooltip.innerHTML =
          `<div style="font-weight:600;color:#f3f4f6;margin-bottom:4px;">${esc(d.from)} → ${esc(d.to)}</div>` +
          `<div>Calls: <span style="color:#e5e7eb;">${d.calls}</span></div>` +
          `<div>Avg latency: <span style="color:#e5e7eb;">${fmtMs(avgLat)}</span></div>` +
          `<div>Tokens: <span style="color:#e5e7eb;">${parseInt(d.tokens).toLocaleString()}</span></div>` +
          `<div>Cost: <span style="color:#e5e7eb;">$${parseFloat(d.cost).toFixed(4)}</span></div>` +
          `<div>Error rate: <span style="color:${errColor};">${errPct}%</span></div>`;
        tooltip.style.display = "block";
        positionTooltip(e);
      });
      el.addEventListener("mousemove", positionTooltip);
      el.addEventListener("mouseleave", () => { tooltip.style.display = "none"; });
    });
  }

  // ── Detail panel ──────────────────────────────────────────────────────────

  function renderDetailPanel() {
    const panel = document.getElementById("cg-detail-panel");
    if (!panel) return;

    if (!selectedStep) {
      panel.style.display = "none";
      panel.innerHTML = "";
      return;
    }

    const step = selectedStep;
    const isError = !!(step.error || (step.statusCode != null && step.statusCode >= 400));
    const time = step.timestamp ? new Date(step.timestamp).toLocaleTimeString() : "";
    const sessionNote = step.isSubagent ? ` <span style="color:#c4b5fd;font-size:0.85em;">[subagent: ${esc(step.sessionId)}]</span>` : "";

    const headerHtml =
      `<div class="cg-detail-header">` +
      `<div><span class="cg-detail-title">Call #${step.stepNum}</span>` +
      `<span class="cg-detail-time">${esc(time)}</span>` +
      `<span class="${isError ? "cg-detail-status-error" : "cg-detail-status-ok"}">${step.statusCode ?? "?"} ${isError ? "error" : "ok"}</span>` +
      sessionNote + `</div>` +
      `<button class="cg-detail-close" id="cg-close-btn">×</button></div>`;

    let bodyHtml = "";
    if (isError) {
      bodyHtml = `<div class="cg-detail-section"><div class="cg-detail-label">Error</div>` +
        `<pre class="cg-detail-error">${esc(step.error ?? "HTTP " + step.statusCode)}</pre></div>`;
    } else {
      bodyHtml +=
        `<div class="cg-detail-metrics">` +
        `<div><div class="cg-detail-label">Model</div><div class="cg-detail-value-blue">${esc(step.model ?? step.provider)}</div></div>` +
        `<div><div class="cg-detail-label">Latency</div><div>${esc(fmtMs(step.latencyMs))}</div></div>` +
        `<div><div class="cg-detail-label">Tokens in</div><div>${step.inputTokens != null ? step.inputTokens.toLocaleString() : "—"}</div></div>` +
        `<div><div class="cg-detail-label">Tokens out</div><div>${step.outputTokens != null ? step.outputTokens.toLocaleString() : "—"}</div></div>` +
        `</div>`;

      if (step.systemPromptPreview) {
        bodyHtml += `<div class="cg-detail-section"><div class="cg-detail-label">System prompt (preview)</div>` +
          `<pre class="cg-detail-system">${esc(step.systemPromptPreview)}</pre></div>`;
      }

      if (step.toolResults.length > 0) {
        bodyHtml += `<div class="cg-detail-section"><div class="cg-detail-label">Tool results received (${step.toolResults.length})</div>` +
          step.toolResults.map(tr => {
            const idStr = tr.toolCallId ? `[${tr.toolCallId.slice(0, 8)}…] ` : "";
            const content = tr.content.slice(0, 100) + (tr.content.length > 100 ? "…" : "");
            return `<div class="cg-tool-result">${esc(idStr + content)}</div>`;
          }).join("") + `</div>`;
      }

      if (step.responseText) {
        bodyHtml += `<div class="cg-detail-section"><div class="cg-detail-label">Response</div>` +
          `<pre class="cg-detail-response">${esc(step.responseText)}</pre></div>`;
      }

      if (step.toolCalls.length > 0) {
        bodyHtml += `<div class="cg-detail-section"><div class="cg-detail-label">Tool calls made (${step.toolCalls.length})</div>` +
          step.toolCalls.map(tc => {
            const args = Object.entries(tc.input || {}).slice(0, 2).map(([k, v]) => `${k}=${JSON.stringify(v)}`).join(", ");
            return `<div class="cg-tool-call">${esc(tc.name ?? "?")}` +
              (args ? ` <span class="cg-tool-input">(${esc(args)})</span>` : "") + `</div>`;
          }).join("") + `</div>`;
      }
    }

    panel.innerHTML = headerHtml + bodyHtml;
    panel.style.display = "block";

    document.getElementById("cg-close-btn")?.addEventListener("click", () => {
      selectedStep = null;
      panel.style.display = "none";
      panel.innerHTML = "";
      renderCallGraph();
    });
  }

  // ── Data loading ──────────────────────────────────────────────────────────

  function scopeParam() {
    return workflowScope ? "?scope=workflow" : "";
  }

  async function loadCallGraph(sessionId, silent) {
    const container = document.getElementById("cg-svg-container");
    if (!silent && container) container.innerHTML = '<div class="no-data">Loading…</div>';
    if (!silent) {
      selectedStep = null;
      callGraphData = null;
      toolSequenceData = null;
    }

    const sp = scopeParam();
    try {
      const [graphRes, seqRes] = await Promise.all([
        fetch(`/api/session/${encodeURIComponent(sessionId)}/call-graph${sp}`),
        fetch(`/api/session/${encodeURIComponent(sessionId)}/tool-sequence${sp}`),
      ]);
      callGraphData = await graphRes.json();
      const seqJson = await seqRes.json();
      toolSequenceData = seqJson.steps || [];
    } catch (_) {
      if (container && !silent) container.innerHTML = '<div class="no-data">Error loading call graph</div>';
      return;
    }

    renderCallGraph();
    renderDetailPanel();

    // Update "last updated" label
    const updLabel = document.getElementById("cg-updated-label");
    if (updLabel) updLabel.textContent = `Updated ${new Date().toLocaleTimeString()}`;
  }

  // ── Session list + auto-refresh ───────────────────────────────────────────

  function groupSessions(sessions) {
    const byId = Object.fromEntries(sessions.map(s => [s.session_id, s]));
    const groups = [];
    for (const s of sessions) {
      const m = s.session_id.match(/^(.+)\.(\d+)$/);
      if (m && byId[m[1]]) { /* child */ } else { groups.push({ ...s, children: [] }); }
    }
    for (const s of sessions) {
      const m = s.session_id.match(/^(.+)\.(\d+)$/);
      if (m && byId[m[1]]) {
        const parent = groups.find(g => g.session_id === m[1]);
        if (parent) parent.children.push(s);
      }
    }
    return groups;
  }

  function loadSessions() {
    fetch("/api/sessions")
      .then(r => r.json())
      .then(data => {
        const list = document.getElementById("cg-session-list");
        if (!list) return;
        const sessions = data.sessions || [];

        // Auto-refresh: if current session's interaction count increased, re-fetch silently
        if (currentSession) {
          const sess = sessions.find(s => s.session_id === currentSession);
          if (sess && sess.count !== currentSessionCount) {
            currentSessionCount = sess.count;
            loadCallGraph(currentSession, /* silent */ true);
          }
        }

        if (!sessions.length) {
          list.innerHTML = '<li class="no-data">No sessions yet</li>';
          return;
        }
        const groups = groupSessions(sessions);
        list.innerHTML = groups.map(g => {
          const activeClass = g.session_id === currentSession ? " active" : "";
          let html = `<li class="session-item${activeClass}" data-id="${esc(g.session_id)}">` +
            `<div class="session-id">${esc(g.session_id)}</div>` +
            `<div class="session-count">${g.count} interaction${g.count !== 1 ? "s" : ""}` +
            (g.children.length > 0 ? ` · ${g.children.length} subagent${g.children.length !== 1 ? "s" : ""}` : "") +
            `</div></li>`;
          for (const child of g.children) {
            const ca = child.session_id === currentSession ? " active" : "";
            html += `<li class="session-item session-child${ca}" data-id="${esc(child.session_id)}">` +
              `<div class="session-id">${esc(child.session_id)}</div>` +
              `<div class="session-count">${child.count} interaction${child.count !== 1 ? "s" : ""}</div></li>`;
          }
          return html;
        }).join("");

        list.querySelectorAll(".session-item").forEach(el => {
          el.addEventListener("click", () => {
            const newId = el.dataset.id;
            // If clicking already-selected session, just refresh data
            if (newId === currentSession) {
              loadCallGraph(currentSession, false);
              return;
            }
            currentSession = newId;
            currentSessionCount = 0;
            selectedStep = null;
            list.querySelectorAll(".session-item").forEach(e =>
              e.classList.toggle("active", e.dataset.id === currentSession)
            );
            const label = document.getElementById("cg-session-label");
            if (label) label.textContent = currentSession;
            loadCallGraph(currentSession, false);
          });
        });
      })
      .catch(() => {});
  }

  // ── Init ──────────────────────────────────────────────────────────────────

  const modeAgg = document.getElementById("cg-mode-agg");
  const modeSeq = document.getElementById("cg-mode-seq");
  const scopeSession = document.getElementById("cg-scope-session");
  const scopeWorkflow = document.getElementById("cg-scope-workflow");
  const refreshBtn = document.getElementById("cg-refresh-btn");

  if (modeAgg) modeAgg.addEventListener("click", () => {
    viewMode = "aggregate";
    modeAgg.classList.add("active");
    if (modeSeq) modeSeq.classList.remove("active");
    renderCallGraph();
  });

  if (modeSeq) modeSeq.addEventListener("click", () => {
    viewMode = "sequential";
    if (modeSeq) modeSeq.classList.add("active");
    if (modeAgg) modeAgg.classList.remove("active");
    renderCallGraph();
  });

  if (scopeSession) scopeSession.addEventListener("click", () => {
    if (workflowScope === false) return;
    workflowScope = false;
    scopeSession.classList.add("active");
    if (scopeWorkflow) scopeWorkflow.classList.remove("active");
    if (currentSession) loadCallGraph(currentSession, false);
  });

  if (scopeWorkflow) scopeWorkflow.addEventListener("click", () => {
    if (workflowScope === true) return;
    workflowScope = true;
    scopeWorkflow.classList.add("active");
    if (scopeSession) scopeSession.classList.remove("active");
    if (currentSession) loadCallGraph(currentSession, false);
  });

  if (refreshBtn) refreshBtn.addEventListener("click", () => {
    if (currentSession) loadCallGraph(currentSession, false);
  });

  loadSessions();
  setInterval(loadSessions, 5000);

  const connStatus = document.getElementById("conn-status");
  if (connStatus) connStatus.textContent = "Connected";
})();
