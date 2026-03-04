#!/bin/bash
# Test forward request via Monitor protocol (without real API key — expect 401)
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

SERVER_LOG="/tmp/server_forward_$$.log"

echo "[1] Starting server..."
"$BUILD_DIR/bin/dt_demo_server" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 12

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Server died"; tail -20 "$SERVER_LOG"; exit 1
fi
echo "[1] Server alive"

echo ""
echo "[2] Testing forward request via Monitor..."
timeout 60 python3 -c "
import os, sys, time, json
os.environ.setdefault('CHI_CLIENT_RETRY_TIMEOUT', '0')
os.environ['CHI_WAIT_SERVER'] = '10'
sys.path.insert(0, os.environ['BUILD_DIR'] + '/bin')
import msgpack, chimaera_runtime_ext as chi

ok = chi.chimaera_init(0)
if not ok: print('FAIL: init'); sys.exit(1)
print('Init OK', flush=True)

# Build a forward request JSON
forward_query = json.dumps({
    'action': 'forward',
    'session_id': 'test-session-001',
    'provider': 'anthropic',
    'path': '/v1/messages',
    'headers': {
        'content-type': 'application/json',
        'x-api-key': 'test-key-not-real',
        'anthropic-version': '2023-06-01'
    },
    'body': json.dumps({
        'model': 'claude-sonnet-4-20250514',
        'max_tokens': 10,
        'messages': [{'role': 'user', 'content': 'Say hi'}]
    })
})

t0 = time.time()
task = chi.async_monitor('local', f'pool_stats://800.0:local:{forward_query}')
results = task.wait(30)
t1 = time.time()
print(f'Took {t1-t0:.2f}s', flush=True)

if results:
    for cid, blob in results.items():
        if isinstance(blob, (bytes, bytearray)):
            resp = msgpack.unpackb(blob, raw=False)
            print(f'  status: {resp.get(\"status\", \"unknown\")}', flush=True)
            body = resp.get('body', '')
            if isinstance(body, bytes):
                body = body.decode('utf-8', errors='replace')
            # Truncate body for display
            print(f'  body: {body[:200]}', flush=True)
    print('PASSED (got response)', flush=True)
else:
    print('FAILED: empty results', flush=True)

chi.chimaera_finalize()
" 2>&1
echo "Client exit: $?"

echo ""
echo "=== MonitorPoolStats flow ==="
grep -E 'Admin::Monitor|MonitorPoolStats|Monitor forward' "$SERVER_LOG" | head -10

# Cleanup
kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/* 2>/dev/null || true
echo "Done."
