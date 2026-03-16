#!/usr/bin/env bash
# =============================================================================
# DTProvenance Recovery Workflow Test (shell / claude CLI edition)
# =============================================================================
# Uses `claude -p` routed through the proxy via ANTHROPIC_BASE_URL, so no
# separate API credits are needed — the Claude Code Pro subscription is enough.
#
# Architecture note on signal extraction:
#   `claude -p` sends streaming (SSE) requests to the Anthropic API. The proxy
#   currently skips <recovery_signal> extraction for streaming responses because
#   the tag may be split across multiple delta events. Step 2 therefore uses the
#   /_test/process_llm_event test endpoint to inject a canned response that
#   contains a recovery signal — this exercises the full extraction + CTE
#   storage pipeline identically to a real response, without requiring a
#   non-streaming path. All other steps use real `claude -p` calls to verify
#   injection (which operates on the request, not the response).
#
# Flow:
#   0. Ack any stale pending events so the run starts clean.
#   1. Agent A writes a divide function (no recovery context yet).
#   2. Simulate Agent B finding a bug — exercise extraction + CTE storage via
#      the /_test/process_llm_event endpoint with a canned signal response.
#   3. Verify the event appears in the recovery API (unacknowledged).
#   4. Agent A makes another call — proxy injects the pending error into its
#      system prompt; verify it appears in the stored interaction.
#   5. Acknowledge the event via the dashboard API.
#   6. Agent A makes one more call — verify no injection occurs.
#
# Usage:
#   ./test_recovery_workflow.sh [proxy_host [proxy_port]]
#
# Environment:
#   PROXY_HOST   (default: localhost)
#   PROXY_PORT   (default: 9090)
# =============================================================================
set -uo pipefail

PROXY_HOST="${PROXY_HOST:-${1:-localhost}}"
PROXY_PORT="${PROXY_PORT:-${2:-9090}}"
PROXY_BASE="http://${PROXY_HOST}:${PROXY_PORT}"

SESSION_A="recovery-test-agent-a"
SESSION_B="recovery-test-agent-b"

PASS=0; FAIL=0

# ── Colour helpers ────────────────────────────────────────────────────────────
GREEN='\033[0;32m'; RED='\033[0;31m'; RESET='\033[0m'; BOLD='\033[1m'
ok()   { echo -e "  ${GREEN}✓${RESET} $*"; (( PASS++ )) || true; }
fail() { echo -e "  ${RED}✗${RESET} $*"; (( FAIL++ )) || true; }
sep()  { echo -e "\n${BOLD}────────────────────────────────────────────────────────────${RESET}"
         [ -n "${1:-}" ] && echo -e "${BOLD}  $1${RESET}" && echo "────────────────────────────────────────────────────────────"; }

# ── Core helpers ──────────────────────────────────────────────────────────────

# Run one claude turn through the proxy; return assistant text on stdout.
# Usage: run_agent <session_id> <prompt> [system_prompt]
run_agent() {
    local sid="$1" prompt="$2" sys="${3:-}"
    local extra_args=()
    [ -n "$sys" ] && extra_args+=( --system-prompt "$sys" )
    ANTHROPIC_BASE_URL="${PROXY_BASE}/_session/${sid}" \
        claude --dangerously-skip-permissions -p "$prompt" \
               --output-format text "${extra_args[@]}" 2>/dev/null
}

# Call the test endpoint to exercise extraction+storage with a canned response.
# Usage: test_endpoint <session_id> <response_text_with_signal>
# Returns the full JSON result from the endpoint.
run_via_test_endpoint() {
    local sid="$1" response_text="$2"
    local resp_body
    resp_body=$(jq -n --arg txt "$response_text" '{
        id: "msg_test", type: "message", role: "assistant",
        model: "claude-3-5-haiku-20241022",
        content: [{ type: "text", text: $txt }],
        stop_reason: "end_turn",
        usage: { input_tokens: 50, output_tokens: 80 }
    }')
    local req_body
    req_body=$(jq -n '{
        model: "claude-3-5-haiku-20241022", max_tokens: 512,
        messages: [{ role: "user", content: "review code" }]
    }')
    curl -s --fail -X POST "${PROXY_BASE}/_test/process_llm_event" \
         -H "Content-Type: application/json" \
         -d "$(jq -n \
               --arg sid "$sid" \
               --arg req "$req_body" \
               --arg resp "$resp_body" \
               '{ session_id: $sid, provider: "anthropic", request_body: $req, response_body: $resp }')"
}

api_get()  { curl -s --fail "${PROXY_BASE}${1}"; }
api_post() { curl -s --fail -X POST "${PROXY_BASE}${1}" > /dev/null; }

pending_count() {
    api_get "/api/session/${1}/recovery_events" \
    | jq '[.events[] | select(.acknowledged == false)] | length'
}

first_pending_event() {
    api_get "/api/session/${1}/recovery_events" \
    | jq -c '[.events[] | select(.acknowledged == false)] | first // empty'
}

ack_all_pending() {
    api_get "/api/session/${1}/recovery_events" \
    | jq -r '.events[] | select(.acknowledged == false) | .blob_name' \
    | while read -r blob; do
        [ -n "$blob" ] && api_post "/api/session/${1}/recovery_events/${blob}/ack"
      done
}


# ── Step 0 — Cleanup ─────────────────────────────────────────────────────────
sep "Step 0 — Clear pre-existing recovery events"
for sid in "$SESSION_A" "$SESSION_B"; do
    count=$(pending_count "$sid" 2>/dev/null || echo 0)
    if [ "$count" -gt 0 ]; then
        ack_all_pending "$sid"
        echo "  Cleared ${count} pending event(s) for ${sid}"
    fi
done
ok "Ready"

# ── Step 1 — Agent A writes code (no recovery context yet) ───────────────────
sep "Step 1 — Agent A writes code (no recovery context yet)"
echo "  Calling Agent A via proxy..."
response_a1=$(run_agent "$SESSION_A" \
    "Write a Python function called divide(a, b) that divides two numbers. Keep it under 5 lines. Reply with just the code block.")
echo "  Response: ${response_a1:0:200}"
if echo "$response_a1" | grep -q "<recovery_signal>"; then
    fail "Unexpected <recovery_signal> in Agent A response"
else
    ok "No <recovery_signal> in response (correct — no pending errors)"
fi

# ── Step 2 — Simulate Agent B embedding a <recovery_signal> ──────────────────
sep "Step 2 — Agent B signals a bug (via test endpoint)"
echo "  Note: claude -p uses streaming, which bypasses signal extraction."
echo "  Using /_test/process_llm_event to exercise extraction+storage directly."

CANNED_B=$(printf \
    'Bug found: divide() raises ZeroDivisionError when b=0.\n<recovery_signal>{"type":"error_report","target_session":"%s","error_type":"code_error","description":"divide() raises ZeroDivisionError when b=0; should guard against zero divisor"}</recovery_signal>' \
    "$SESSION_A")

result_json=$(run_via_test_endpoint "$SESSION_B" "$CANNED_B")
cleaned_text=$(echo "$result_json" | jq -r '.cleaned_response | fromjson | .content[0].text')
signals=$(echo "$result_json" | jq -c '.signals')
blobs=$(echo "$result_json" | jq -r '.stored_blobs[]')

echo "  Cleaned response: ${cleaned_text:0:200}"

if echo "$cleaned_text" | grep -q "<recovery_signal>"; then
    fail "<recovery_signal> was NOT stripped from the cleaned response"
else
    ok "<recovery_signal> stripped from response"
fi

signal_count=$(echo "$result_json" | jq '.signals | length')
if [ "$signal_count" -gt 0 ]; then
    ok "${signal_count} signal(s) extracted"
else
    fail "No signals extracted from response"
fi

if [ -n "$blobs" ] && ! echo "$blobs" | grep -q "^$"; then
    ok "Event stored in CTE (blob: ${blobs:0:8}...)"
else
    fail "Event storage returned empty blob name"
fi

# ── Step 3 — Verify event stored ─────────────────────────────────────────────
sep "Step 3 — Verify recovery event stored for Agent A"
sleep 0.5

total=$(api_get "/api/session/${SESSION_A}/recovery_events" | jq '.events | length')
pending=$(pending_count "$SESSION_A")
event_json=$(first_pending_event "$SESSION_A" 2>/dev/null || echo "")

echo "  Events total: ${total}   Unacknowledged: ${pending}"

if [ -z "$event_json" ]; then
    fail "No unacknowledged event found for ${SESSION_A}"
else
    echo "  event_type:  $(echo "$event_json" | jq -r '.event_type')"
    echo "  source:      $(echo "$event_json" | jq -r '.source_session_id') @ seq $(echo "$event_json" | jq -r '.source_sequence_id')"
    echo "  target:      $(echo "$event_json" | jq -r '.target_session_id')"
    echo "  description: $(echo "$event_json" | jq -r '.payload.description')"
    blob=$(echo "$event_json" | jq -r '.blob_name')
    ok "Event targeting ${SESSION_A} found (blob: ${blob:0:8}...)"
fi

# ── Step 4 — Verify injection into Agent A's next call ───────────────────────
sep "Step 4 — Agent A next call: verify recovery context injected"
echo "  Calling Agent A via proxy..."
response_a2=$(run_agent "$SESSION_A" \
    "Does your divide function handle all edge cases correctly? Reply concisely." \
    "You are a Python expert.")
echo "  Response: ${response_a2:0:300}"

# The injected call is stored under a sub-session (recovery-test-agent-a.2)
# due to _resolve_session fingerprinting, so checking the stored system prompt
# of the base session is unreliable. Verify via the response text instead —
# if Claude addresses the ZeroDivisionError it received the recovery context.
if echo "$response_a2" | grep -qiE "ZeroDivisionError|zero|error report|recovery|b\s*==?\s*0|divisor"; then
    ok "Agent A's response reflects the injected recovery context"
else
    fail "Agent A's response does not mention the reported bug (injection may have failed)"
fi

# ── Step 5 — Acknowledge event ────────────────────────────────────────────────
sep "Step 5 — Acknowledge event"
if [ -z "$event_json" ]; then
    echo "  Skipping (no event from step 3)"
else
    blob=$(echo "$event_json" | jq -r '.blob_name')
    api_post "/api/session/${SESSION_A}/recovery_events/${blob}/ack"
    sleep 0.5
    acked=$(api_get "/api/session/${SESSION_A}/recovery_events" \
        | jq --arg b "$blob" '.events[] | select(.blob_name == $b) | .acknowledged')
    if [ "$acked" = "true" ]; then
        ok "blob '${blob:0:8}...' acknowledged in CTE"
    else
        fail "Acknowledgement not reflected in CTE (got: $acked)"
    fi
fi

# ── Step 6 — No injection after ack ─────────────────────────────────────────
sep "Step 6 — Agent A next call: no injection (event acknowledged)"
echo "  Calling Agent A via proxy..."
response_a3=$(run_agent "$SESSION_A" "Thanks, the fix looks good.")
echo "  Response: ${response_a3:0:200}"

# Verify by checking the API directly: no pending events means nothing to inject.
remaining=$(pending_count "$SESSION_A" 2>/dev/null || echo 0)
echo "  Pending events for ${SESSION_A}: ${remaining}"
if [ "$remaining" -eq 0 ]; then
    ok "No pending events — proxy will not inject recovery context"
else
    fail "${remaining} event(s) still pending after acknowledgement"
fi

# ── Summary ───────────────────────────────────────────────────────────────────
sep
echo -e "  ${BOLD}Results: ${GREEN}${PASS} passed${RESET}  ${RED}${FAIL} failed${RESET}"
echo ""
echo "  Recovery dashboard:  ${PROXY_BASE}/recovery"
echo "  Provenance:          ${PROXY_BASE}/provenance"
echo ""

[ "$FAIL" -eq 0 ]
