#!/bin/bash
# Diagnostic test for co_await CTE calls from inline dispatch
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

pkill -9 -f dt_demo_server 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/* 2>/dev/null || true
sleep 1

SERVER_LOG="/tmp/server_diag_$$.log"

echo "[1] Starting server..."
"$BUILD_DIR/bin/dt_demo_server" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 12

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Server died"; tail -50 "$SERVER_LOG"; exit 1
fi
echo "[1] Server alive (PID=$SERVER_PID)"

echo ""
echo "[2] Testing list_sessions with 15s timeout..."
timeout 30 python3 -c "
import os, sys, time, json
os.environ.setdefault('CHI_CLIENT_RETRY_TIMEOUT', '0')
os.environ['CHI_WAIT_SERVER'] = '10'
sys.path.insert(0, os.environ['BUILD_DIR'] + '/bin')
import msgpack, chimaera_runtime_ext as chi

ok = chi.chimaera_init(0)
if not ok: print('FAIL: init'); sys.exit(1)
print('Init OK', flush=True)

# Test: list_sessions
print('Sending list_sessions query...', flush=True)
t0 = time.time()
task = chi.async_monitor('local', 'pool_stats://800.0:local:list_sessions')
results = task.wait(15)
t1 = time.time()
print(f'Took {t1-t0:.2f}s', flush=True)
print(f'Results type: {type(results)}', flush=True)
print(f'Results truthy: {bool(results)}', flush=True)
if results:
    print(f'Results keys: {list(results.keys())}', flush=True)
    for cid, blob in results.items():
        print(f'  cid={cid} type={type(blob)} len={len(blob) if hasattr(blob, \"__len__\") else \"N/A\"}', flush=True)
        if isinstance(blob, (bytes, bytearray)):
            data = msgpack.unpackb(blob, raw=False)
            print(f'  data={json.dumps(data)}', flush=True)
    print('PASSED', flush=True)
else:
    print(f'FAILED - results={results}', flush=True)

print('Done.', flush=True)
" 2>&1

echo ""
echo "[3] Server log (last 50 lines):"
tail -50 "$SERVER_LOG"

echo ""
echo "[4] Looking for Monitor/CTE/await/yield messages in server log..."
grep -iE 'monitor|tracker|untangler|cte|await|yield|suspend|future|event_queue|tag_query|list_session' "$SERVER_LOG" | tail -30

# Cleanup
kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/* 2>/dev/null || true
echo "Done."
