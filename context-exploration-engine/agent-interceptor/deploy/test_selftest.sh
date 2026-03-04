#!/bin/bash
# Test: check if in-process DEALER can reach the ROUTER
set +e

WORKTREE="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="$WORKTREE/build"
CONF_DIR="$(dirname "$0")/../demo"

source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_SERVER_CONF="$CONF_DIR/wrp_conf.yaml"

pkill -9 -f dt_demo_server 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/chimaera_9513.ipc 2>/dev/null || true
sleep 2

SERVER_LOG="/tmp/chi_selftest_$$.log"
echo "[1] Starting server..."
"$BUILD_DIR/bin/dt_demo_server" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!

# Wait for server to start AND self-test to complete (self-test sends at ~3s, waits 5s)
echo "[2] Waiting 20s for server + self-test..."
sleep 20

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "WARN: Server died during self-test"
fi

echo ""
echo "[3] ALL SELF-TEST log lines:"
grep "SELF-TEST" "$SERVER_LOG"

echo ""
echo "[4] ALL ROUTER MONITOR events:"
grep "ROUTER MONITOR" "$SERVER_LOG"

echo ""
echo "[5] RecvMetadata POLLIN events:"
grep "zmq_poll returned POLLIN" "$SERVER_LOG" | head -5

echo ""
echo "[6] Last 10 server log lines:"
tail -10 "$SERVER_LOG"

kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/chimaera_9513.ipc "$SERVER_LOG" 2>/dev/null || true
echo "Done."
