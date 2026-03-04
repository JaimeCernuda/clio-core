#!/bin/bash
# Test: chimaera_init with IPC mode (Unix domain sockets, bypasses ZMQ)
set +e

WORKTREE="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="$WORKTREE/build"
CONF_DIR="$(dirname "$0")/../demo"

source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_SERVER_CONF="$CONF_DIR/wrp_conf.yaml"
export PYTHONPATH="$BUILD_DIR/bin:${PYTHONPATH:-}"

pkill -9 -f dt_demo_server 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/chimaera_9513.ipc 2>/dev/null || true
sleep 2

SERVER_LOG="/tmp/chi_ipc_mode_$$.log"
echo "[1] Starting server..."
"$BUILD_DIR/bin/dt_demo_server" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 12

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "FAIL: Server died"
    cat "$SERVER_LOG"
    exit 1
fi
echo "[1] Server alive PID=$SERVER_PID"

echo ""
echo "[2] IPC socket:"
ls -la /tmp/chimaera_$(whoami)/chimaera_9513.ipc 2>/dev/null || echo "NOT FOUND"

echo ""
echo "[3] Testing chimaera_init with CHI_IPC_MODE=ipc..."
timeout 20 python3 -c "
import os, sys, time
os.environ.setdefault('CHI_CLIENT_RETRY_TIMEOUT', '0')
os.environ['CHI_WAIT_SERVER'] = '10'
os.environ['CHI_IPC_MODE'] = 'ipc'
sys.path.insert(0, '$BUILD_DIR/bin')
import chimaera_runtime_ext as chi
print('[client] Calling chimaera_init(0)...', flush=True)
ok = chi.chimaera_init(0)
print(f'[client] Result: {ok}', flush=True)
if ok:
    print('[client] SUCCESS! Finalizing...', flush=True)
    chi.chimaera_finalize()
else:
    print('[client] FAILED', flush=True)
" 2>&1

echo ""
echo "[4] Server log (last 10 lines):"
tail -10 "$SERVER_LOG"

kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/chimaera_9513.ipc "$SERVER_LOG" 2>/dev/null || true
echo "Done."
