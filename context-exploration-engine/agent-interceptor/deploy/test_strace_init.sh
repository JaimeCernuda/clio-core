#!/bin/bash
# Use strace to see what the server and client do during chimaera_init
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
CONF_DIR="$SCRIPT_DIR/../demo"

echo "=== Strace chimaera_init Diagnostic ==="
echo "Host: $(hostname)"

# Environment
source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_SERVER_CONF="$CONF_DIR/wrp_conf.yaml"
export PYTHONPATH="$BUILD_DIR/bin:${PYTHONPATH:-}"

VENV_PYTHON="$SCRIPT_DIR/.venv/bin/python"

# Clean up
pkill -f dt_demo_server 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || true
sleep 2

# Start server
echo "[1] Starting Chimaera server..."
"$BUILD_DIR/bin/dt_demo_server" > /dev/null 2>&1 &
SERVER_PID=$!
sleep 10

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Server failed to start"
  exit 1
fi
echo "[1] Server running (PID=$SERVER_PID)"

# Verify port
ss -tlnp 2>/dev/null | grep 9516

# Get the server's network fd (9516 listener)
echo ""
echo "[2] Server's file descriptors (ZMQ-related):"
ls -la /proc/$SERVER_PID/fd/ 2>/dev/null | head -30

# Attach strace to server for 15 seconds, watching recvmsg on the ZMQ fd
echo ""
echo "[3] Starting strace on server (background, 20s)..."
SERVER_STRACE="/tmp/strace_server_$$.log"
strace -p $SERVER_PID -e trace=recvmsg,sendmsg,accept,accept4,epoll_wait,epoll_pwait,epoll_pwait2 -o "$SERVER_STRACE" -T -f &
STRACE_PID=$!
sleep 2

# Run client with strace too
echo "[4] Starting chimaera_init(0) client with strace..."
CLIENT_STRACE="/tmp/strace_client_$$.log"
timeout 20 strace -e trace=sendmsg,recvmsg,connect,write,writev -T -f -o "$CLIENT_STRACE" \
  "$VENV_PYTHON" -c "
import os, sys, time
os.environ.setdefault('CHI_CLIENT_RETRY_TIMEOUT', '0')
os.environ.setdefault('CHI_CLIENT_TRY_NEW_SERVERS', '16')
os.environ['CHI_WAIT_SERVER'] = '10'
sys.path.insert(0, '$BUILD_DIR/bin')
import chimaera_runtime_ext as chi
ok = chi.chimaera_init(0)
print(f'Result: {ok}')
if ok: chi.chimaera_finalize()
" 2>&1

echo ""
echo "[5] Stopping strace..."
kill $STRACE_PID 2>/dev/null || true
wait $STRACE_PID 2>/dev/null || true

echo ""
echo "[6] Server strace - recvmsg calls on port 9516 (last 30):"
grep -E 'recvmsg|sendmsg|accept' "$SERVER_STRACE" 2>/dev/null | tail -30

echo ""
echo "[7] Client strace - sendmsg/connect calls (last 30):"
grep -E 'sendmsg|connect.*9516|writev' "$CLIENT_STRACE" 2>/dev/null | tail -30

echo ""
echo "[8] Client strace - connects:"
grep 'connect(' "$CLIENT_STRACE" 2>/dev/null | head -20

echo ""
echo "[cleanup]"
kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc "$SERVER_STRACE" "$CLIENT_STRACE" 2>/dev/null || true
echo "Done."
