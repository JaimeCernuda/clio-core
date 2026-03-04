#!/bin/bash
# Detailed diagnostic for proxy dispatch chain
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
CONF_DIR="$SCRIPT_DIR/../demo"
PROXY_PORT=9090
SERVER_LOG="/tmp/dt_server_diag.log"

echo "=== DTProvenance Dispatch Diagnostic ==="
echo "Host: $(hostname)"
echo "Server log: $SERVER_LOG"
echo ""

# Environment
source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_SERVER_CONF="$CONF_DIR/wrp_conf.yaml"

# Clean up
killall -9 dt_demo_server 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || true
sleep 2

# Check memory limits
echo "[mem] ulimit -v (virtual mem): $(ulimit -v)"
echo "[mem] ulimit -m (resident mem): $(ulimit -m)"
echo "[mem] Free memory: $(free -h | grep Mem | awk '{print $4}')"
echo ""

# Start server with separate log
echo "[1] Starting server (log → $SERVER_LOG)..."
"$BUILD_DIR/bin/dt_demo_server" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 8

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Server failed to start"
  echo "--- Last 30 lines of server log ---"
  tail -30 "$SERVER_LOG"
  exit 1
fi
echo "[1] Server running (PID=$SERVER_PID)"

cleanup() {
  echo "[cleanup] Stopping server..."
  kill $SERVER_PID 2>/dev/null || true
  wait $SERVER_PID 2>/dev/null || true
}
trap cleanup EXIT

# Check proxy
echo "[2] Checking proxy port..."
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "http://localhost:$PROXY_PORT/" --connect-timeout 5 2>/dev/null)
echo "    Root endpoint: HTTP $HTTP_CODE"

# Test: Direct POST with max-time to avoid hanging
echo ""
echo "[3] Sending POST to /_session/diag-test/v1/messages..."
echo "    (max-time=60s, will show progress)"

# Send request in background so we can monitor server
curl -v \
  -X POST "http://localhost:$PROXY_PORT/_session/diag-test/v1/messages" \
  -H "Content-Type: application/json" \
  -H "anthropic-version: 2023-06-01" \
  -H "x-api-key: dummy-key-for-testing" \
  --connect-timeout 10 \
  --max-time 60 \
  -d '{"model":"claude-sonnet-4-6","max_tokens":16,"messages":[{"role":"user","content":"Say hi"}]}' \
  > /tmp/dt_curl_response.txt 2> /tmp/dt_curl_verbose.txt &
CURL_PID=$!

# Monitor server while curl is running
for i in $(seq 1 12); do
  sleep 5
  if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo ""
    echo "!!! SERVER CRASHED after ${i}x5 seconds !!!"
    echo ""
    echo "--- Server log (last 50 lines) ---"
    tail -50 "$SERVER_LOG"
    echo ""
    echo "--- dmesg OOM check ---"
    dmesg 2>/dev/null | grep -i "oom\|killed\|out of memory" | tail -10 || echo "(no access to dmesg)"
    echo ""
    echo "--- Curl verbose output ---"
    cat /tmp/dt_curl_verbose.txt
    echo ""
    echo "--- Curl response ---"
    cat /tmp/dt_curl_response.txt
    kill $CURL_PID 2>/dev/null || true
    exit 1
  fi

  # Check if curl finished
  if ! kill -0 $CURL_PID 2>/dev/null; then
    CURL_EXIT=$?
    wait $CURL_PID 2>/dev/null
    CURL_EXIT=$?
    echo ""
    echo "[3] Curl finished (exit=$CURL_EXIT) after ${i}x5 seconds"
    break
  fi
  echo "    ... waiting (${i}x5s, server alive)"
done

# If curl is still running after 60s
if kill -0 $CURL_PID 2>/dev/null; then
  echo ""
  echo "[3] Curl still running after 60s — killing"
  kill $CURL_PID 2>/dev/null
fi

echo ""
echo "--- Curl verbose output ---"
cat /tmp/dt_curl_verbose.txt 2>/dev/null
echo ""
echo "--- Curl response body ---"
cat /tmp/dt_curl_response.txt 2>/dev/null
echo ""
echo "--- Server log (last 50 lines) ---"
tail -50 "$SERVER_LOG"
echo ""
echo "=== Diagnostic Complete ==="
