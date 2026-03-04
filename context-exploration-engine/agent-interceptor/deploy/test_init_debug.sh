#!/bin/bash
# Debug chimaera_init(0) — run on compute node
set +e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKTREE="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$WORKTREE/build"
CONF_DIR="$SCRIPT_DIR/../demo"

echo "=== chimaera_init Debug Test ==="
echo "Host: $(hostname)"
echo "Worktree: $WORKTREE"
echo "Build: $BUILD_DIR"

source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_SERVER_CONF="$CONF_DIR/wrp_conf.yaml"
export PYTHONPATH="$BUILD_DIR/bin:${PYTHONPATH:-}"

# Clean
pkill -9 -f dt_demo_server 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/chimaera_9513.ipc 2>/dev/null || true
sleep 2

# Start server
SERVER_LOG="/tmp/chi_init_debug_$$.log"
echo "[1] Starting server (log: $SERVER_LOG)..."
"$BUILD_DIR/bin/dt_demo_server" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!
echo "    PID=$SERVER_PID"
sleep 12

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "FAIL: Server died. Log:"
    cat "$SERVER_LOG"
    exit 1
fi
echo "[1] Server alive. Log lines: $(wc -l < "$SERVER_LOG")"

# Show server threads
echo ""
echo "[2] Server threads:"
NTHREADS=$(ls /proc/$SERVER_PID/task/ 2>/dev/null | wc -l)
echo "    Count: $NTHREADS"
for tid in $(ls /proc/$SERVER_PID/task/ 2>/dev/null | head -25); do
    name=$(cat /proc/$SERVER_PID/task/$tid/comm 2>/dev/null || echo "?")
    # Check CPU affinity
    affinity=$(taskset -p $tid 2>/dev/null | grep -o '0x[0-9a-f]*' || echo "?")
    echo "    tid=$tid  name=$name  affinity=$affinity"
done

# Check listeners
echo ""
echo "[3] TCP listener on port 9516:"
ss -tnlp 2>/dev/null | grep 9516 || echo "    NOT LISTENING"

echo ""
echo "[4] IPC socket:"
ls -la /tmp/chimaera_$(whoami)/chimaera_9513.ipc 2>/dev/null || echo "    NOT FOUND"

# Now test chimaera_init
echo ""
echo "[5] Testing chimaera_init(0) with 20s timeout..."
CLIENT_LOG="/tmp/chi_init_client_$$.log"
timeout 25 python3 -c "
import os, sys, time
os.environ.setdefault('CHI_CLIENT_RETRY_TIMEOUT', '0')
os.environ.setdefault('CHI_CLIENT_TRY_NEW_SERVERS', '16')
os.environ['CHI_WAIT_SERVER'] = '20'
sys.path.insert(0, '$BUILD_DIR/bin')

print('[client] Importing chimaera_runtime_ext...', flush=True)
import chimaera_runtime_ext as chi
print('[client] Import OK. Calling chimaera_init(0)...', flush=True)

t0 = time.time()
ok = chi.chimaera_init(0)
t1 = time.time()
print(f'[client] Result: {ok} (took {t1-t0:.1f}s)', flush=True)

if ok:
    print('[client] SUCCESS! Testing async_monitor...', flush=True)
    try:
        task = chi.async_monitor('local', 'status')
        results = task.wait(5)
        print(f'[client] Monitor result: {results}', flush=True)
    except Exception as e:
        print(f'[client] Monitor error: {e}', flush=True)
    chi.chimaera_finalize()
else:
    print('[client] FAILED — chimaera_init returned False', flush=True)
" > "$CLIENT_LOG" 2>&1
CLIENT_RC=$?
echo "    Client exit code: $CLIENT_RC"
echo "    Client output:"
cat "$CLIENT_LOG"

# Check server state after client attempt
echo ""
echo "[6] Server log after client:"
echo "    Total lines: $(wc -l < "$SERVER_LOG")"
echo "    ClientRecv lines: $(grep -c 'ClientRecv' "$SERVER_LOG" || echo 0)"
echo "    ZeroMq lines: $(grep -c 'ZeroMq' "$SERVER_LOG" || echo 0)"
echo "    SendZmq lines: $(grep -c 'SendZmq' "$SERVER_LOG" || echo 0)"

echo ""
echo "[7] TCP connections during test:"
ss -tnp 2>/dev/null | grep 9516

echo ""
echo "[8] Last 15 server log lines:"
tail -15 "$SERVER_LOG"

# Cleanup
kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/chimaera_9513.ipc "$SERVER_LOG" "$CLIENT_LOG" 2>/dev/null || true
echo ""
echo "Done."
