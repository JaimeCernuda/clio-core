#!/bin/bash
# Check ROUTER MONITOR events
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

SERVER_LOG="/tmp/chi_monitor_$$.log"
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
echo "[2] Monitor events BEFORE client:"
grep "ROUTER MONITOR" "$SERVER_LOG"

echo ""
echo "[3] Starting client (10s timeout)..."
timeout 15 python3 -c "
import os, sys, time
os.environ.setdefault('CHI_CLIENT_RETRY_TIMEOUT', '0')
os.environ['CHI_WAIT_SERVER'] = '10'
sys.path.insert(0, '$BUILD_DIR/bin')
import chimaera_runtime_ext as chi
ok = chi.chimaera_init(0)
print(f'chimaera_init result: {ok}', flush=True)
if ok: chi.chimaera_finalize()
" 2>/dev/null
echo "[3] Client done"

echo ""
echo "[4] ALL ROUTER MONITOR events:"
grep "ROUTER MONITOR" "$SERVER_LOG"

echo ""
echo "[5] Check ZMQ I/O thread states:"
for tid in $(ls /proc/$SERVER_PID/task/ 2>/dev/null); do
    name=$(cat /proc/$SERVER_PID/task/$tid/comm 2>/dev/null || echo "?")
    if echo "$name" | grep -q "ZMQ"; then
        wchan=$(cat /proc/$SERVER_PID/task/$tid/wchan 2>/dev/null || echo "?")
        echo "    tid=$tid name=$name wchan=$wchan"
    fi
done

kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/chimaera_9513.ipc "$SERVER_LOG" 2>/dev/null || true
echo "Done."
