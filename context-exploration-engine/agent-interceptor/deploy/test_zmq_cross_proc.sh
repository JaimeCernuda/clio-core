#!/bin/bash
# Test ZMQ ROUTER/DEALER across two separate processes
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VENV_PYTHON="$SCRIPT_DIR/.venv/bin/python"

echo "=== Cross-Process ZMQ Test ==="
echo "Host: $(hostname)"

# Start ROUTER server as a separate process
echo "[1] Starting ROUTER server process..."
"$VENV_PYTHON" -c "
import zmq, time, sys

PORT = 19517
ctx = zmq.Context()
sock = ctx.socket(zmq.ROUTER)
sock.setsockopt(zmq.ROUTER_MANDATORY, 1)
sock.bind(f'tcp://0.0.0.0:{PORT}')
print(f'[ROUTER] Bound on 0.0.0.0:{PORT}', flush=True)

poller = zmq.Poller()
poller.register(sock, zmq.POLLIN)

# Poll for up to 15 seconds
for i in range(150):
    socks = dict(poller.poll(100))
    if sock in socks:
        frames = sock.recv_multipart()
        print(f'[ROUTER] Got {len(frames)} frames: identity={frames[0]!r}', flush=True)
        # Send response back
        sock.send_multipart([frames[0], b'', b'PONG-from-router'])
        print('[ROUTER] Sent PONG', flush=True)
        time.sleep(1)  # Give client time to recv
        break
else:
    print('[ROUTER] TIMEOUT: No messages in 15s', flush=True)

sock.close()
ctx.term()
print('[ROUTER] Done', flush=True)
" &
ROUTER_PID=$!
sleep 1

# Start DEALER client as a separate process
echo "[2] Starting DEALER client process..."
timeout 10 "$VENV_PYTHON" -c "
import zmq, time

PORT = 19517
ctx = zmq.Context()
dealer = ctx.socket(zmq.DEALER)
dealer.setsockopt_string(zmq.IDENTITY, 'cross-proc-client')
dealer.connect(f'tcp://127.0.0.1:{PORT}')
time.sleep(0.5)

print('[DEALER] Sending PING...', flush=True)
dealer.send_multipart([b'', b'PING-from-dealer'])
print('[DEALER] Sent PING', flush=True)

poller = zmq.Poller()
poller.register(dealer, zmq.POLLIN)
socks = dict(poller.poll(5000))
if dealer in socks:
    resp = dealer.recv_multipart()
    print(f'[DEALER] Got response: {resp!r}', flush=True)
    print('[CROSS-PROC] PASS: ZMQ works across processes', flush=True)
else:
    print('[DEALER] No response in 5s', flush=True)
    print('[CROSS-PROC] FAIL: ZMQ broken across processes', flush=True)

dealer.close()
ctx.term()
" 2>&1

wait $ROUTER_PID 2>/dev/null
echo "Done."
