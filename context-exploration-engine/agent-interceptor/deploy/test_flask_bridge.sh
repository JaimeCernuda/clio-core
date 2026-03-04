#!/bin/bash
# Test Flask bridge: HTTP → Chimaera Monitor → upstream API
set +e
source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

WORKTREE="/mnt/common/jcernudagarcia/clio-core/.claude/worktrees/read_path"
BUILD_DIR="$WORKTREE/build"
export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_REPO_PATH="$BUILD_DIR/bin"
export CHI_SERVER_CONF="$WORKTREE/context-exploration-engine/agent-interceptor/demo/wrp_conf.yaml"
export PYTHONPATH="$BUILD_DIR/bin:${PYTHONPATH:-}"
export BUILD_DIR

FLASK_PORT=9090
FLASK_APP="$WORKTREE/context-visualizer/context_visualizer/app.py"
VENV_PYTHON="$WORKTREE/context-exploration-engine/agent-interceptor/deploy/.venv/bin/python3"

# Cleanup from previous runs
pkill -9 -f dt_demo_server 2>/dev/null || true
pkill -9 -f "flask run" 2>/dev/null || true
pkill -9 -f "context_visualizer" 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/* 2>/dev/null || true
sleep 1

SERVER_LOG="/tmp/server_flask_bridge_$$.log"
FLASK_LOG="/tmp/flask_bridge_$$.log"

echo "[1] Starting Chimaera server..."
"$BUILD_DIR/bin/dt_demo_server" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 12

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Server died"; tail -20 "$SERVER_LOG"; exit 1
fi
echo "[1] Chimaera server alive (PID=$SERVER_PID)"

echo ""
echo "[2] Starting Flask bridge on port $FLASK_PORT..."
cd "$WORKTREE/context-visualizer"
FLASK_APP=context_visualizer.app FLASK_ENV=development \
  "$VENV_PYTHON" -m flask run --host 0.0.0.0 --port $FLASK_PORT > "$FLASK_LOG" 2>&1 &
FLASK_PID=$!
cd "$WORKTREE"
sleep 5

if ! kill -0 $FLASK_PID 2>/dev/null; then
  echo "FAIL: Flask died"; tail -20 "$FLASK_LOG"; exit 1
fi
echo "[2] Flask bridge alive (PID=$FLASK_PID)"

echo ""
echo "[3] Testing forward request via Flask HTTP bridge..."
# Send a request through Flask → Chimaera → upstream (expect 401 with fake key)
RESPONSE=$(curl -s -w "\n%{http_code}" \
  -X POST "http://localhost:$FLASK_PORT/_session/test-flask-001/v1/messages" \
  -H "Content-Type: application/json" \
  -H "x-api-key: test-key-not-real" \
  -H "anthropic-version: 2023-06-01" \
  -d '{"model":"claude-sonnet-4-20250514","max_tokens":10,"messages":[{"role":"user","content":"Say hi"}]}' \
  --max-time 30)

HTTP_CODE=$(echo "$RESPONSE" | tail -1)
BODY=$(echo "$RESPONSE" | sed '$d')

echo "  HTTP status: $HTTP_CODE"
echo "  Body: ${BODY:0:200}"

if [ "$HTTP_CODE" = "401" ]; then
  echo "  PASSED (401 = expected with fake API key)"
elif [ "$HTTP_CODE" = "200" ]; then
  echo "  PASSED (200 = got real response)"
else
  echo "  FAILED (unexpected status $HTTP_CODE)"
fi

echo ""
echo "[4] Testing dispatch_stats via Flask dashboard..."
DS_RESPONSE=$(curl -s "http://localhost:$FLASK_PORT/api/provenance/dispatch_stats" --max-time 10)
echo "  dispatch_stats: $DS_RESPONSE"

echo ""
echo "[5] Testing session rejection (no /_session/ prefix)..."
REJECT_RESPONSE=$(curl -s -X POST "http://localhost:$FLASK_PORT/v1/messages" \
  -H "Content-Type: application/json" \
  -H "x-api-key: test" \
  -H "anthropic-version: 2023-06-01" \
  -d '{"model":"claude-sonnet-4-20250514","max_tokens":10,"messages":[{"role":"user","content":"test"}]}' \
  --max-time 10)
echo "  Rejection: ${REJECT_RESPONSE:0:200}"

# Show server logs for the forward request
echo ""
echo "=== Monitor forward log ==="
grep 'Monitor forward' "$SERVER_LOG" | head -5

echo ""
echo "=== Flask log ==="
tail -20 "$FLASK_LOG"

# Cleanup
kill -9 $FLASK_PID 2>/dev/null || true
kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/* 2>/dev/null || true
echo ""
echo "Done."
