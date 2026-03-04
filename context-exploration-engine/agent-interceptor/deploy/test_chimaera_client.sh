#!/bin/bash
# Minimal test: can Python chimaera_init(0) connect to a running server?
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
CONF_DIR="$SCRIPT_DIR/../demo"

echo "=== Minimal chimaera_init(0) Test ==="
echo "Host: $(hostname)"

# Environment
source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_SERVER_CONF="$CONF_DIR/wrp_conf.yaml"
export PYTHONPATH="$BUILD_DIR/bin:${PYTHONPATH:-}"

# Clean up
pkill -f dt_demo_server 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || true
sleep 2

# Start server
echo "[1] Starting Chimaera server..."
"$BUILD_DIR/bin/dt_demo_server" &
SERVER_PID=$!
sleep 10

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Server failed to start"
  exit 1
fi
echo "[1] Server running (PID=$SERVER_PID)"

# Check that IPC socket and port are available
echo "[2] Checking IPC socket..."
ls -la /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || echo "  IPC socket NOT found"

echo "[3] Checking port 9516 (TCP ROUTER)..."
ss -tlnp 2>/dev/null | grep 9516 || netstat -tlnp 2>/dev/null | grep 9516 || echo "  Port 9516 not found in ss/netstat"

# Test 1: Direct Python chimaera_init (no Flask)
echo ""
echo "[4] Testing chimaera_init(0) directly from Python..."
VENV_PYTHON="$SCRIPT_DIR/.venv/bin/python"

timeout 60 "$VENV_PYTHON" -c "
import os, sys
os.environ.setdefault('CHI_CLIENT_RETRY_TIMEOUT', '0')
os.environ.setdefault('CHI_CLIENT_TRY_NEW_SERVERS', '16')
sys.path.insert(0, '$BUILD_DIR/bin')

print('[python] Importing chimaera_runtime_ext...')
import chimaera_runtime_ext as chi

print('[python] Calling chimaera_init(0) [kClient]...')
ok = chi.chimaera_init(0)
print(f'[python] chimaera_init returned: {ok}')

if ok:
    print('[python] SUCCESS! Client connected.')
    print('[python] Testing async_monitor...')
    task = chi.async_monitor('local', 'pool_stats://800.0:local:dispatch_stats')
    result = task.wait(10)
    print(f'[python] Monitor result: {result}')
    chi.chimaera_finalize()
else:
    print('[python] FAIL: chimaera_init returned False')
" 2>&1

echo ""
echo "[5] Cleanup..."
kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || true
echo "Done."
