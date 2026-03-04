#!/bin/bash
# Check if TCP connection is established between DEALER and ROUTER
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
CONF_DIR="$SCRIPT_DIR/../demo"

echo "=== TCP Connection Check ==="
echo "Host: $(hostname)"

source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_SERVER_CONF="$CONF_DIR/wrp_conf.yaml"
export PYTHONPATH="$BUILD_DIR/bin:${PYTHONPATH:-}"

VENV_PYTHON="$SCRIPT_DIR/.venv/bin/python"

pkill -f dt_demo_server 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || true
sleep 2

echo "[1] Starting server..."
"$BUILD_DIR/bin/dt_demo_server" > /dev/null 2>&1 &
SERVER_PID=$!
sleep 10

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Server failed to start"
  exit 1
fi
echo "[1] Server PID=$SERVER_PID"

echo "[2] Connections before client:"
ss -tnp 2>/dev/null | grep 9516 || echo "  (none)"

echo ""
echo "[3] Starting client (background, 10s timeout)..."
timeout 10 "$VENV_PYTHON" -c "
import os, sys, time, signal
os.environ.setdefault('CHI_CLIENT_RETRY_TIMEOUT', '0')
os.environ.setdefault('CHI_CLIENT_TRY_NEW_SERVERS', '16')
os.environ['CHI_WAIT_SERVER'] = '8'
sys.path.insert(0, '$BUILD_DIR/bin')
import chimaera_runtime_ext as chi

# Signal handler: print message when SIGALRM fires
def on_alarm(sig, frame):
    pass

signal.signal(signal.SIGALRM, on_alarm)

print('[client] Calling chimaera_init(0)...', flush=True)
# Start alarm to wake up (not used, just being safe)
ok = chi.chimaera_init(0)
print(f'[client] Result: {ok}', flush=True)
if ok: chi.chimaera_finalize()
" 2>/dev/null &
CLIENT_PID=$!

# Wait 3 seconds for the client to connect
sleep 3

echo "[4] Connections DURING client init:"
ss -tnp 2>/dev/null | grep 9516

echo ""
echo "[5] All connections to/from port 9516:"
ss -tnp sport = :9516 or dport = :9516 2>/dev/null

echo ""
echo "[6] Server fds:"
ls -la /proc/$SERVER_PID/fd 2>/dev/null | grep socket | head -10

echo ""
echo "[7] Netstat (if available):"
netstat -tnp 2>/dev/null | grep 9516 | head -10

# Wait for client to finish
wait $CLIENT_PID 2>/dev/null
echo ""
echo "[8] Client exit status: $?"

kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || true
echo "Done."
