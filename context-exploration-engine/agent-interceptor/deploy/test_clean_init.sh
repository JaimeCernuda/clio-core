#!/bin/bash
# Clean test: chimaera_init(0) only — no raw ZMQ junk
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
CONF_DIR="$SCRIPT_DIR/../demo"

echo "=== Clean chimaera_init(0) Test ==="
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

# Start server — redirect stderr to file so we can inspect it later
SERVER_LOG="/tmp/chimaera_server_$$.log"
echo "[1] Starting Chimaera server (log: $SERVER_LOG)..."
"$BUILD_DIR/bin/dt_demo_server" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 10

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Server failed to start"
  cat "$SERVER_LOG"
  exit 1
fi
echo "[1] Server running (PID=$SERVER_PID)"

# Verify port
echo "[2] Port check:"
ss -tlnp 2>/dev/null | grep -E '951[3-9]'

echo ""
echo "[3] Testing chimaera_init(0) with 20s timeout..."
timeout 30 "$VENV_PYTHON" -c "
import os, sys, time
os.environ.setdefault('CHI_CLIENT_RETRY_TIMEOUT', '0')
os.environ.setdefault('CHI_CLIENT_TRY_NEW_SERVERS', '16')
os.environ['CHI_WAIT_SERVER'] = '20'
sys.path.insert(0, '$BUILD_DIR/bin')

print('[init] Calling chimaera_init(0)...')
import chimaera_runtime_ext as chi
start = time.time()
ok = chi.chimaera_init(0)
elapsed = time.time() - start
print(f'[init] Result: {ok} (took {elapsed:.1f}s)')
if ok:
    print('[init] SUCCESS!')
    chi.chimaera_finalize()
else:
    print('[init] FAIL')
" 2>&1

echo ""
echo "[4] Server still alive?"
if kill -0 $SERVER_PID 2>/dev/null; then
  echo "  YES — server still running"
else
  echo "  NO — server DIED!"
  wait $SERVER_PID 2>/dev/null
  echo "  Server exit status: $?"
fi

echo ""
echo "[5] Server log (last 30 lines):"
tail -30 "$SERVER_LOG"

echo ""
echo "[cleanup]"
kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc "$SERVER_LOG" 2>/dev/null || true
echo "Done."
