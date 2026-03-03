#!/bin/bash
#SBATCH -N 1
#SBATCH --exclusive
#SBATCH --cpus-per-task=16
#SBATCH --mem=32G
#SBATCH --time=01:00:00
#SBATCH --job-name=dt-test
#SBATCH --output=logs/test_%j.out
#SBATCH --error=logs/test_%j.err

set -euo pipefail

echo "=== DTProvenance End-to-End Test on $(hostname) ==="
echo "Date: $(date)"

PROXY_PORT=9090
HOSTNAME=$(hostname)
BUILD_DIR=clio-core/build

# 1. Start DTProvenance proxy (distinct from MChiPs on 8080)
echo "Starting DTProvenance server..."
${BUILD_DIR}/dt_demo_server --config clio-core/context-exploration-engine/agent-interceptor/demo/wrp_conf.yaml &
PROXY_PID=$!
sleep 10  # Wait for Chimaera + HTTP server to initialize

# Verify server is running
if ! kill -0 $PROXY_PID 2>/dev/null; then
  echo "FAIL: DTProvenance server failed to start"
  exit 1
fi

echo "DTProvenance server started (PID=$PROXY_PID)"

# 2. Unset CLAUDECODE to avoid nested session detection
unset CLAUDECODE 2>/dev/null || true

# 3. Run 3 Claude agents in parallel, each with unique session
echo "Running Claude agents through proxy..."
uv run python deploy/run_agents.py \
  --proxy-host "$HOSTNAME" \
  --proxy-port "$PROXY_PORT" \
  --sessions "agent-0" "agent-1" "agent-2" \
  --prompts "What is 2+2?" "Name the planets in our solar system" "Write a haiku about code"

# 4. Verify via C++ ctx_writer
echo "Verifying stored data with ctx_writer..."
${BUILD_DIR}/ctx_writer \
  --expected-sessions 3 \
  --expected-tag-prefix "Agentic_session_" \
  --min-interactions-per-session 1
CTX_RC=$?

# 5. Also verify via Python
echo "Verifying with Python script..."
uv run python deploy/verify_storage.py \
  --expected-sessions 3 \
  --tag-prefix "Agentic_session_"

# 6. Cleanup
kill $PROXY_PID 2>/dev/null || true
wait $PROXY_PID 2>/dev/null || true

echo "=== Test exit code: $CTX_RC ==="
exit $CTX_RC
