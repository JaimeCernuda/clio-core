#!/bin/bash
# Diagnostic: test raw ZMQ DEALER→ROUTER on port 9516 (same as Chimaera)
# This isolates whether the issue is in ZMQ or Chimaera task handling.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
CONF_DIR="$SCRIPT_DIR/../demo"

echo "=== Raw ZMQ Connectivity Test ==="
echo "Host: $(hostname)"

# Environment
source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_SERVER_CONF="$CONF_DIR/wrp_conf.yaml"
export PYTHONPATH="$BUILD_DIR/bin:${PYTHONPATH:-}"

VENV_PYTHON="$SCRIPT_DIR/.venv/bin/python"

# Install pyzmq if needed
if ! "$VENV_PYTHON" -c "import zmq" 2>/dev/null; then
  echo "[setup] Installing pyzmq..."
  uv pip install --python "$VENV_PYTHON" pyzmq
fi

# Test 1: Check raw ZMQ DEALER→ROUTER (standalone, no Chimaera)
echo ""
echo "=== Test 1: Standalone ZMQ ROUTER/DEALER ==="
timeout 15 "$VENV_PYTHON" -c "
import zmq, time, threading

PORT = 19516  # Use different port to avoid conflict with Chimaera

# Start ROUTER in background
def run_router():
    ctx = zmq.Context()
    sock = ctx.socket(zmq.ROUTER)
    sock.setsockopt(zmq.ROUTER_MANDATORY, 1)
    sock.bind(f'tcp://0.0.0.0:{PORT}')
    print(f'[ROUTER] Bound on 0.0.0.0:{PORT}')

    poller = zmq.Poller()
    poller.register(sock, zmq.POLLIN)

    for _ in range(50):  # Poll up to 5 seconds
        socks = dict(poller.poll(100))
        if sock in socks:
            frames = sock.recv_multipart()
            print(f'[ROUTER] Received {len(frames)} frames: identity={frames[0]!r}, data={frames[1:]!r}')
            # Send response back
            sock.send_multipart([frames[0], b'', b'PONG'])
            print('[ROUTER] Sent PONG')
            break
    else:
        print('[ROUTER] No messages received in 5s')
    sock.close()
    ctx.term()

t = threading.Thread(target=run_router, daemon=True)
t.start()
time.sleep(0.5)

# DEALER connects and sends
ctx2 = zmq.Context()
dealer = ctx2.socket(zmq.DEALER)
dealer.setsockopt_string(zmq.IDENTITY, 'test-client')
dealer.connect(f'tcp://127.0.0.1:{PORT}')
time.sleep(0.1)

dealer.send_multipart([b'', b'PING'])
print('[DEALER] Sent PING')

poller = zmq.Poller()
poller.register(dealer, zmq.POLLIN)
socks = dict(poller.poll(3000))
if dealer in socks:
    resp = dealer.recv_multipart()
    print(f'[DEALER] Received response: {resp!r}')
    print('[Test 1] PASS: Standalone ZMQ works')
else:
    print('[DEALER] No response in 3s')
    print('[Test 1] FAIL: Standalone ZMQ broken')

dealer.close()
ctx2.term()
t.join(timeout=3)
" 2>&1
echo ""

# Test 2: Check if we can reach the Chimaera server's ROUTER on port 9516
echo "=== Test 2: Connect to Chimaera ROUTER (port 9516) ==="
# First check if server is running
pkill -f dt_demo_server 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || true
sleep 2

echo "[2a] Starting Chimaera server..."
"$BUILD_DIR/bin/dt_demo_server" &
SERVER_PID=$!
sleep 10

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "FAIL: Server failed to start"
  exit 1
fi
echo "[2a] Server running (PID=$SERVER_PID)"

# Check ports
echo "[2b] Checking ports..."
ss -tlnp 2>/dev/null | grep -E '951[3-9]' || echo "  No 951x ports found"

echo ""
echo "[2c] Attempting raw ZMQ DEALER connect to 127.0.0.1:9516..."
timeout 15 "$VENV_PYTHON" -c "
import zmq, time

ctx = zmq.Context()
dealer = ctx.socket(zmq.DEALER)
dealer.setsockopt_string(zmq.IDENTITY, 'diag-client-$(hostname)-$$')
dealer.setsockopt(zmq.LINGER, 1000)
dealer.setsockopt(zmq.RCVTIMEO, 5000)
dealer.setsockopt(zmq.SNDTIMEO, 5000)

print('[DEALER] Connecting to tcp://127.0.0.1:9516...')
dealer.connect('tcp://127.0.0.1:9516')
time.sleep(0.5)  # Give ZMQ time to establish connection

# Send a simple message (empty delimiter + payload)
print('[DEALER] Sending test message...')
try:
    dealer.send_multipart([b'', b'hello-from-diag'])
    print('[DEALER] Message sent successfully')
except Exception as e:
    print(f'[DEALER] Send failed: {e}')

# Try to receive response
print('[DEALER] Waiting for response (5s timeout)...')
try:
    resp = dealer.recv_multipart()
    print(f'[DEALER] Got response: {resp!r}')
except zmq.Again:
    print('[DEALER] No response (timeout)')
except Exception as e:
    print(f'[DEALER] Recv error: {e}')

dealer.close()
ctx.term()
print('[2c] Done')
" 2>&1

echo ""
echo "[2d] Testing raw TCP connect to port 9516..."
timeout 5 "$VENV_PYTHON" -c "
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(3)
try:
    s.connect(('127.0.0.1', 9516))
    print('[TCP] Connected to 127.0.0.1:9516 OK')
    s.close()
except Exception as e:
    print(f'[TCP] Connection failed: {e}')
" 2>&1

echo ""
echo "[2e] Testing IPC socket connect..."
timeout 5 "$VENV_PYTHON" -c "
import socket, os
ipc_path = '/tmp/chimaera_jcernudagarcia/chimaera_9513.ipc'
if os.path.exists(ipc_path):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(3)
    try:
        s.connect(ipc_path)
        print(f'[IPC] Connected to {ipc_path} OK')
        # Try sending some bytes
        s.sendall(b'\\x00\\x00\\x00\\x04TEST')
        print('[IPC] Sent 8 bytes')
        s.close()
    except Exception as e:
        print(f'[IPC] Connection/send failed: {e}')
else:
    print(f'[IPC] Socket not found: {ipc_path}')
" 2>&1

echo ""
echo "[2f] Testing chimaera_init(0) with verbose logging..."
timeout 45 "$VENV_PYTHON" -c "
import os, sys, time
os.environ.setdefault('CHI_CLIENT_RETRY_TIMEOUT', '0')
os.environ.setdefault('CHI_CLIENT_TRY_NEW_SERVERS', '16')
os.environ['CHI_WAIT_SERVER'] = '15'  # Shorter timeout for faster feedback
os.environ['HERMES_LOG_LEVEL'] = '0'  # Most verbose logging (kDebug)
sys.path.insert(0, '$BUILD_DIR/bin')

print('[init] CHI_SERVER_CONF =', os.environ.get('CHI_SERVER_CONF', 'NOT SET'))
print('[init] CHI_IPC_MODE =', os.environ.get('CHI_IPC_MODE', 'NOT SET (default TCP)'))
print('[init] CHI_WAIT_SERVER =', os.environ.get('CHI_WAIT_SERVER'))
print('[init] HERMES_LOG_LEVEL =', os.environ.get('HERMES_LOG_LEVEL'))

print('[init] Importing chimaera_runtime_ext...')
import chimaera_runtime_ext as chi

print('[init] Calling chimaera_init(0) [kClient]...')
start = time.time()
ok = chi.chimaera_init(0)
elapsed = time.time() - start
print(f'[init] chimaera_init returned: {ok} (took {elapsed:.1f}s)')

if ok:
    print('[init] SUCCESS!')
    chi.chimaera_finalize()
else:
    print('[init] FAIL')
" 2>&1

echo ""
echo "[cleanup] Killing server..."
kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || true
echo "Done."
