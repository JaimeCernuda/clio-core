#!/bin/bash
# Test cross-process Monitor dispatch (all query types)
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

SERVER_LOG="/tmp/server_cross_proc_$$.log"

echo "[1] Starting server..."
"$BUILD_DIR/bin/dt_demo_server" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 12

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Server died"; tail -20 "$SERVER_LOG"; exit 1
fi
echo "[1] Server alive"

echo ""
echo "[2] Testing multiple Monitor queries..."
timeout 30 python3 -c "
import os, sys, time, json
os.environ.setdefault('CHI_CLIENT_RETRY_TIMEOUT', '0')
os.environ['CHI_WAIT_SERVER'] = '10'
sys.path.insert(0, os.environ['BUILD_DIR'] + '/bin')
import msgpack, chimaera_runtime_ext as chi

ok = chi.chimaera_init(0)
if not ok: print('FAIL: init'); sys.exit(1)
print('Init OK', flush=True)

passed = 0
failed = 0

# Test 1: dispatch_stats
print('\n--- dispatch_stats ---', flush=True)
t0 = time.time()
task = chi.async_monitor('local', 'pool_stats://800.0:local:dispatch_stats')
results = task.wait(10)
t1 = time.time()
print(f'  Took {t1-t0:.2f}s', flush=True)
if results:
    for cid, blob in results.items():
        if isinstance(blob, (bytes, bytearray)):
            print(f'  {json.dumps(msgpack.unpackb(blob, raw=False))}', flush=True)
    print('  dispatch_stats: PASSED', flush=True)
    passed += 1
else:
    print('  dispatch_stats: FAILED (empty)', flush=True)
    failed += 1

# Test 2: list_sessions (via proxy -> tracker inline dispatch)
print('\n--- list_sessions ---', flush=True)
t0 = time.time()
task = chi.async_monitor('local', 'pool_stats://800.0:local:list_sessions')
results = task.wait(10)
t1 = time.time()
print(f'  Took {t1-t0:.2f}s', flush=True)
if results:
    for cid, blob in results.items():
        if isinstance(blob, (bytes, bytearray)):
            data = msgpack.unpackb(blob, raw=False)
            print(f'  sessions: {json.dumps(data)}', flush=True)
    print('  list_sessions: PASSED', flush=True)
    passed += 1
else:
    print('  list_sessions: FAILED (empty)', flush=True)
    failed += 1

# Test 3: worker_stats (admin direct — baseline)
print('\n--- worker_stats ---', flush=True)
t0 = time.time()
task = chi.async_monitor('local', 'worker_stats')
results = task.wait(10)
t1 = time.time()
print(f'  Took {t1-t0:.2f}s', flush=True)
if results:
    print('  worker_stats: PASSED', flush=True)
    passed += 1
else:
    print('  worker_stats: FAILED (empty)', flush=True)
    failed += 1

print(f'\n=== Results: {passed} passed, {failed} failed ===', flush=True)
chi.chimaera_finalize()
" 2>&1
echo "Client exit: $?"

echo ""
echo "=== MonitorPoolStats flow ==="
grep -E 'Admin::Monitor|MonitorPoolStats' "$SERVER_LOG" | head -20

# Cleanup
kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/* 2>/dev/null || true
echo "Done."
