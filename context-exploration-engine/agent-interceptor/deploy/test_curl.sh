#!/bin/bash
# Quick diagnostic test: curl directly to proxy (no claude-agent-sdk)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
CONF_DIR="$SCRIPT_DIR/../demo"
PROXY_PORT=9090

echo "=== DTProvenance Curl Diagnostic ==="
echo "Repo:  $REPO_ROOT"
echo "Build: $BUILD_DIR"
echo "Host:  $(hostname)"
echo ""

# Environment
source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_SERVER_CONF="$CONF_DIR/wrp_conf.yaml"

# Clean up
pkill -f dt_demo_server 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || true
sleep 2

# Start server
echo "[1] Starting server..."
"$BUILD_DIR/bin/dt_demo_server" &
SERVER_PID=$!
sleep 8

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Server failed to start"
  exit 1
fi
echo "[1] Server running (PID=$SERVER_PID)"

cleanup() {
  echo "[cleanup] Stopping server..."
  kill $SERVER_PID 2>/dev/null || true
  wait $SERVER_PID 2>/dev/null || true
}
trap cleanup EXIT

# Verify proxy responds
echo ""
echo "[2] Checking proxy port..."
if curl -s -o /dev/null -w "%{http_code}" "http://localhost:$PROXY_PORT/" 2>/dev/null; then
  echo " -> Proxy responding"
else
  echo " -> Proxy NOT responding!"
  exit 1
fi

# Test: Direct curl to proxy with a small Anthropic API request
# This uses the real Anthropic API key from env
echo ""
echo "[3] Testing proxy → Anthropic dispatch..."
echo "    Sending POST to http://localhost:$PROXY_PORT/_session/curl-test/v1/messages"

# Get the API key
API_KEY="${ANTHROPIC_API_KEY:-}"
if [ -z "$API_KEY" ]; then
  echo "    WARNING: No ANTHROPIC_API_KEY set. Trying without key (may get 401)."
fi

RESPONSE=$(curl -s -w "\n---HTTP_STATUS:%{http_code}---\nTIME_TOTAL:%{time_total}s" \
  -X POST "http://localhost:$PROXY_PORT/_session/curl-test/v1/messages" \
  -H "Content-Type: application/json" \
  -H "anthropic-version: 2023-06-01" \
  ${API_KEY:+-H "x-api-key: $API_KEY"} \
  --connect-timeout 10 \
  --max-time 120 \
  -d '{
    "model": "claude-sonnet-4-6",
    "max_tokens": 32,
    "messages": [{"role": "user", "content": "What is 2+2? Reply with just the number."}]
  }' 2>&1)

echo "$RESPONSE"

# Extract HTTP status
HTTP_STATUS=$(echo "$RESPONSE" | grep "HTTP_STATUS:" | sed 's/.*HTTP_STATUS:\([0-9]*\).*/\1/')
echo ""
echo "[3] HTTP status: $HTTP_STATUS"

if [ "$HTTP_STATUS" = "200" ]; then
  echo "[3] SUCCESS: Proxy → Anthropic interception → upstream → response works!"
elif [ "$HTTP_STATUS" = "401" ]; then
  echo "[3] Got 401 (auth error) — proxy forwarded correctly, but API key issue."
  echo "    The dispatch chain works. Need correct ANTHROPIC_API_KEY."
elif [ "$HTTP_STATUS" = "502" ]; then
  echo "[3] Got 502 — proxy callback returned error. Check server logs above."
else
  echo "[3] Got HTTP $HTTP_STATUS — check response body above."
fi

echo ""
echo "=== Done ==="
