#!/bin/bash
# Full server log capture + search for ClientRecv
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
CONF_DIR="$SCRIPT_DIR/../demo"

echo "=== Full Log chimaera_init Test ==="
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

SERVER_LOG="/tmp/chimaera_full_$$.log"
echo "[1] Starting server (full log: $SERVER_LOG)..."
"$BUILD_DIR/bin/dt_demo_server" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 12  # Extra time for compose to finish

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Server failed to start"
  cat "$SERVER_LOG"
  exit 1
fi
echo "[1] Server running (PID=$SERVER_PID)"

# How many lines in log so far?
echo "[2] Server log line count after 12s startup: $(wc -l < "$SERVER_LOG")"

# Search for ClientRecv
echo "[3] ClientRecv log lines:"
grep -c "ClientRecv" "$SERVER_LOG" || echo "  (none found)"
grep "ClientRecv" "$SERVER_LOG" | head -5

# Search for compose completion
echo "[4] Compose completion:"
grep -c "Successfully created pool" "$SERVER_LOG" || echo "  (none found)"
grep "Compose.*Successfully" "$SERVER_LOG" | tail -3

echo ""
echo "[5] Running chimaera_init(0) with 15s timeout..."
timeout 25 "$VENV_PYTHON" -c "
import os, sys, time
os.environ.setdefault('CHI_CLIENT_RETRY_TIMEOUT', '0')
os.environ.setdefault('CHI_CLIENT_TRY_NEW_SERVERS', '16')
os.environ['CHI_WAIT_SERVER'] = '15'
sys.path.insert(0, '$BUILD_DIR/bin')
import chimaera_runtime_ext as chi
ok = chi.chimaera_init(0)
print(f'Result: {ok}')
if ok: chi.chimaera_finalize()
" 2>&1

echo ""
echo "[6] Server log after client attempt:"
echo "  Total lines: $(wc -l < "$SERVER_LOG")"
echo "  ClientRecv lines: $(grep -c 'ClientRecv' "$SERVER_LOG" || echo 0)"
grep "ClientRecv" "$SERVER_LOG" | head -10
echo ""
echo "  Any error/warning from admin:"
grep -E '(ERROR|WARNING).*admin|admin.*(ERROR|WARNING)' "$SERVER_LOG" | head -5

echo ""
echo "[7] Full log last 10 lines:"
tail -10 "$SERVER_LOG"

echo ""
kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc "$SERVER_LOG" 2>/dev/null || true
echo "Done."
