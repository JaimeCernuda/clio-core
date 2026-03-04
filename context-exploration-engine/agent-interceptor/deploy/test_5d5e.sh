#!/bin/bash
# Run only tests 5d (new session) and 5e (persistence) — 5a-5c already passed
# Uses timeouts and SIGKILL for robust cleanup

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
CONF_DIR="$SCRIPT_DIR/../demo"
DEPLOY_DIR="$SCRIPT_DIR"
PROXY_PORT=9090
SERVER_PID=""

echo "=== DTProvenance E2E Tests 5d+5e ==="
echo "Host: $(hostname)"
echo "Date: $(date)"

# ── Environment ──
source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips
export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_SERVER_CONF="$CONF_DIR/wrp_conf.yaml"
unset CLAUDECODE 2>/dev/null || true

VENV_PYTHON="$DEPLOY_DIR/.venv/bin/python"

cleanup() {
  if [ -n "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
    echo "[cleanup] Killing server (SIGKILL)..."
    kill -9 $SERVER_PID 2>/dev/null || true
  fi
  rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || true
}
trap cleanup EXIT

# Clean up any leftover server
pkill -9 -f dt_demo_server 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || true
sleep 2

# ── Start server ──
echo "[server] Starting DTProvenance server..."
"$BUILD_DIR/bin/dt_demo_server" > /tmp/dt_server_5d5e.log 2>&1 &
SERVER_PID=$!
sleep 8

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Server failed to start"
  tail -20 /tmp/dt_server_5d5e.log
  exit 1
fi
echo "[server] Running (PID=$SERVER_PID)"

# Wait for proxy to be ready
for i in 1 2 3 4 5; do
  if curl -s -o /dev/null -m 5 "http://localhost:$PROXY_PORT/" 2>/dev/null; then
    echo "[server] Proxy responding on port $PROXY_PORT"
    break
  fi
  sleep 2
done

# ── Test 5d: New Session ──
echo ""
echo "=== Test 5d: New Session ==="
if timeout 120 "$VENV_PYTHON" "$DEPLOY_DIR/run_agents.py" \
  --proxy-host localhost \
  --proxy-port $PROXY_PORT \
  --sessions "fresh-session" \
  --prompts "Say hello in French. Reply with just the phrase." 2>&1; then
  echo "[5d] PASSED"
else
  echo "[5d] FAILED (exit code $?)"
fi

# ── Test 5e: Persistence ──
echo ""
echo "=== Test 5e: Persistence ==="
echo "[5e] Killing server..."
kill -9 $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || true
sleep 3

echo "[5e] Restarting server..."
"$BUILD_DIR/bin/dt_demo_server" > /tmp/dt_server_5e.log 2>&1 &
SERVER_PID=$!
sleep 8

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "[5e] FAILED: Server failed to restart"
  tail -20 /tmp/dt_server_5e.log
  exit 1
fi
echo "[5e] Server restarted (PID=$SERVER_PID)"

# Wait for proxy
for i in 1 2 3 4 5; do
  if curl -s -o /dev/null -m 5 "http://localhost:$PROXY_PORT/" 2>/dev/null; then
    echo "[5e] Proxy responding"
    break
  fi
  sleep 2
done

# Send interaction after restart
if timeout 120 "$VENV_PYTHON" "$DEPLOY_DIR/run_agents.py" \
  --proxy-host localhost \
  --proxy-port $PROXY_PORT \
  --sessions "test-single" \
  --prompts "What is 10+5? Reply with just the number." 2>&1; then
  echo "[5e] PASSED"
else
  echo "[5e] FAILED (exit code $?)"
fi

echo ""
echo "=== Tests 5d+5e Complete ==="
echo "Results:"
echo "  5a: PASSED (previous run)"
echo "  5b: PASSED (previous run)"
echo "  5c: PASSED (previous run)"
echo "  5d: See above"
echo "  5e: See above"
