#!/bin/bash
# Test: trace TCP bytes between DEALER and ROUTER
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

SERVER_LOG="/tmp/chi_tcp_trace_$$.log"
echo "[1] Starting server..."
"$BUILD_DIR/bin/dt_demo_server" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 12

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "FAIL: Server died"
    exit 1
fi
echo "[1] Server alive PID=$SERVER_PID"

# Show listener
echo ""
echo "[2] Listener socket:"
ss -tnlp 2>/dev/null | grep 9516

# Start strace on server to catch network I/O
STRACE_LOG="/tmp/chi_strace_$$.log"
echo ""
echo "[3] Starting strace on server..."
strace -f -e trace=network,read,write,epoll_wait -p $SERVER_PID -o "$STRACE_LOG" &
STRACE_PID=$!
sleep 1

# Connect client
echo ""
echo "[4] Starting client..."
timeout 15 python3 << 'PYEOF'
import os, sys, time, ctypes

BUILD_DIR = os.environ.get("BUILD_DIR", "")

# Use spack libzmq directly to test
LIBZMQ = "/mnt/common/jcernudagarcia/spack/var/spack/environments/mchips/.spack-env/view/lib/libzmq.so.5"
zmq = ctypes.cdll.LoadLibrary(LIBZMQ)
zmq.zmq_ctx_new.restype = ctypes.c_void_p
zmq.zmq_socket.restype = ctypes.c_void_p
zmq.zmq_strerror.restype = ctypes.c_char_p

ctx = ctypes.c_void_p(zmq.zmq_ctx_new())
zmq.zmq_ctx_set(ctx, 1, 2)  # ZMQ_IO_THREADS = 2

dealer = ctypes.c_void_p(zmq.zmq_socket(ctx, 5))  # ZMQ_DEALER

identity = b"test-tcp-trace"
zmq.zmq_setsockopt(dealer, 5, identity, len(identity))  # ZMQ_IDENTITY

imm = ctypes.c_int(0)
zmq.zmq_setsockopt(dealer, 39, ctypes.byref(imm), 4)  # ZMQ_IMMEDIATE

rc = zmq.zmq_connect(dealer, b"tcp://127.0.0.1:9516")
print(f"  connect rc={rc}", flush=True)

print("  Waiting 5s for handshake...", flush=True)
time.sleep(5)

# Check events
evts = ctypes.c_int(0)
evts_len = ctypes.c_size_t(4)
zmq.zmq_getsockopt(dealer, 15, ctypes.byref(evts), ctypes.byref(evts_len))
print(f"  DEALER events before send: {evts.value} (POLLIN={evts.value & 1}, POLLOUT={evts.value & 2})", flush=True)

# Send
print("  Sending delimiter + payload...", flush=True)
rc1 = zmq.zmq_send(dealer, b"", 0, 2)  # ZMQ_SNDMORE
payload = b"HELLO-ROUTER-TEST"
rc2 = zmq.zmq_send(dealer, payload, len(payload), 0)
print(f"  Delimiter rc={rc1}, Payload rc={rc2}", flush=True)

# Wait for delivery
print("  Waiting 5s for server to process...", flush=True)
time.sleep(5)

# Check events after
zmq.zmq_getsockopt(dealer, 15, ctypes.byref(evts), ctypes.byref(evts_len))
print(f"  DEALER events after wait: {evts.value} (POLLIN={evts.value & 1}, POLLOUT={evts.value & 2})", flush=True)

linger = ctypes.c_int(0)
zmq.zmq_setsockopt(dealer, 17, ctypes.byref(linger), 4)
zmq.zmq_close(dealer)
zmq.zmq_ctx_destroy(ctx)
print("  Done", flush=True)
PYEOF

echo ""
echo "[5] Stopping strace..."
kill $STRACE_PID 2>/dev/null || true
wait $STRACE_PID 2>/dev/null

echo ""
echo "[6] TCP connections with port 9516 (from strace):"
grep -c "9516" "$STRACE_LOG" 2>/dev/null || echo "0"

echo ""
echo "[7] Network syscalls related to client connection:"
grep -E "accept|connect|sendmsg|recvmsg" "$STRACE_LOG" 2>/dev/null | grep -v "ENOENT\|EINTR" | head -20

echo ""
echo "[8] Write syscalls on ZMQ-related fds:"
grep -E "^[0-9]+ write\(|^[0-9]+ sendto\(" "$STRACE_LOG" 2>/dev/null | head -20

echo ""
echo "[9] Read syscalls returning data:"
grep -E "^[0-9]+ read\(|^[0-9]+ recvfrom\(" "$STRACE_LOG" 2>/dev/null | grep -v "EAGAIN" | head -20

echo ""
echo "[10] ROUTER MONITOR events:"
grep "ROUTER MONITOR" "$SERVER_LOG"

kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_$(whoami)/chimaera_9513.ipc "$SERVER_LOG" "$STRACE_LOG" 2>/dev/null || true
echo "Done."
