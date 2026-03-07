#!/bin/bash
set -uo pipefail

REPO_ROOT=${HOME}/clio-core
BUILD_DIR=$REPO_ROOT/build
CONF_DIR=$REPO_ROOT/context-exploration-engine/agent-interceptor/demo
DEPLOY_DIR=$REPO_ROOT/context-exploration-engine/agent-interceptor/deploy
VIS_DIR=$REPO_ROOT/context-visualizer
PROXY_PORT=9090
VENV_PYTHON="$DEPLOY_DIR/.venv/bin/python"

# Environment
source ${HOME}/spack/share/spack/setup-env.sh
spack env activate mchips

export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_SERVER_CONF="$CONF_DIR/wrp_conf.yaml"
export PYTHONPATH="$BUILD_DIR/bin:${PYTHONPATH:-}"
unset CLAUDECODE 2>/dev/null || true

# Clean up any leftovers
pkill -9 -f dt_demo_server 2>/dev/null || true
pkill -9 -f "flask run.*$PROXY_PORT" 2>/dev/null || true
rm -f "/tmp/chimaera_$(whoami)/chimaera_9513.ipc" 2>/dev/null || true
sleep 2

# Start Chimaera server
echo "[server] Starting Chimaera server..."
"$BUILD_DIR/bin/dt_demo_server" &
SERVER_PID=$!
sleep 8

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Chimaera server failed to start"; exit 1
fi
echo "[server] Running (PID=$SERVER_PID)"

# Start Flask
echo "[flask] Starting Flask on port $PROXY_PORT..."
cd "$VIS_DIR"
FLASK_APP=context_visualizer.app "$VENV_PYTHON" -m flask run \
  --host 0.0.0.0 --port $PROXY_PORT &
FLASK_PID=$!
sleep 3

if ! kill -0 $FLASK_PID 2>/dev/null; then
  echo "FAIL: Flask failed to start"; kill -9 $SERVER_PID; exit 1
fi
echo "[flask] Running (PID=$FLASK_PID)"

# Run agent tests
echo ""
echo "=== Test 5a: Single Agent ==="
timeout 120 "$VENV_PYTHON" "$DEPLOY_DIR/run_agents.py" \
  --proxy-host localhost --proxy-port $PROXY_PORT \
  --sessions "test-single" \
  --prompts "What is 2+2? Reply with just the number."
echo "[5a] DONE"

echo ""
echo "=== Test 5b: Multi-Agent (3 concurrent) ==="
timeout 120 "$VENV_PYTHON" "$DEPLOY_DIR/run_agents.py" \
  --proxy-host localhost --proxy-port $PROXY_PORT \
  --sessions "multi-a" "multi-b" "multi-c" \
  --prompts "Name one color." "Name one animal." "Name one fruit."
echo "[5b] DONE"

echo ""
echo "=== Test 5c: Session Resume ==="
timeout 120 "$VENV_PYTHON" "$DEPLOY_DIR/run_agents.py" \
  --proxy-host localhost --proxy-port $PROXY_PORT \
  --sessions "test-single" \
  --prompts "Now multiply that answer by 3. Reply with just the number."
echo "[5c] DONE"

echo ""
echo "=== Test 5d: New Session ==="
timeout 120 "$VENV_PYTHON" "$DEPLOY_DIR/run_agents.py" \
  --proxy-host localhost --proxy-port $PROXY_PORT \
  --sessions "fresh-session" \
  --prompts "Say hello in French."
echo "[5d] DONE"

echo ""
echo "=== All tests done. Dashboard is live at http://localhost:$PROXY_PORT/provenance ==="
echo "Server PID=$SERVER_PID  Flask PID=$FLASK_PID"
echo "Press Ctrl+C or run: kill -9 $SERVER_PID $FLASK_PID"

# Wait indefinitely — keep server and Flask alive
wait $SERVER_PID
