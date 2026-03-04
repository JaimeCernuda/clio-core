#!/bin/bash
# Test: pyzmq DEALER → Chimaera ROUTER on port 9516
# Also: spack libzmq ROUTER (standalone) → same port
set +e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKTREE="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$WORKTREE/build"
CONF_DIR="$SCRIPT_DIR/../demo"
VENV_PYTHON="$SCRIPT_DIR/.venv/bin/python"

echo "=== pyzmq + spack DEALER → Chimaera ROUTER ==="
echo "Host: $(hostname)"

source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_SERVER_CONF="$CONF_DIR/wrp_conf.yaml"

# Clean
pkill -9 -f dt_demo_server 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc 2>/dev/null || true
sleep 2

# Start server
SERVER_LOG="/tmp/chi_pyzmq_$$.log"
echo "[1] Starting Chimaera server..."
"$BUILD_DIR/bin/dt_demo_server" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 12

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "FAIL: Server died"
    cat "$SERVER_LOG"
    exit 1
fi
echo "    Server alive (PID=$SERVER_PID)"

# Check I/O thread states
echo ""
echo "[2] ZMQ I/O thread states:"
for tid in $(ls /proc/$SERVER_PID/task/ 2>/dev/null); do
    name=$(cat /proc/$SERVER_PID/task/$tid/comm 2>/dev/null || echo "?")
    if echo "$name" | grep -q "ZMQ"; then
        state=$(cat /proc/$SERVER_PID/task/$tid/status 2>/dev/null | grep "^State:" || echo "?")
        wchan=$(cat /proc/$SERVER_PID/task/$tid/wchan 2>/dev/null || echo "?")
        echo "    tid=$tid name=$name $state wchan=$wchan"
    fi
done

# Test 1: pyzmq DEALER
echo ""
echo "[3] Testing pyzmq DEALER → Chimaera ROUTER (port 9516)..."
timeout 10 "$VENV_PYTHON" -c "
import zmq, time
print(f'  pyzmq={zmq.__version__}, libzmq={zmq.zmq_version()}')
ctx = zmq.Context()
dealer = ctx.socket(zmq.DEALER)
dealer.setsockopt_string(zmq.IDENTITY, 'pyzmq-test-client')
dealer.connect('tcp://127.0.0.1:9516')
time.sleep(2)

# Check socket events
events = dealer.getsockopt(zmq.EVENTS)
print(f'  DEALER events before send: {events} (POLLIN={events & zmq.POLLIN}, POLLOUT={events & zmq.POLLOUT})')

dealer.send_multipart([b'', b'PYZMQ-TEST-PAYLOAD'])
print('  Sent message via pyzmq DEALER')

time.sleep(3)
events = dealer.getsockopt(zmq.EVENTS)
print(f'  DEALER events after send: {events} (POLLIN={events & zmq.POLLIN}, POLLOUT={events & zmq.POLLOUT})')

dealer.close()
ctx.term()
print('  pyzmq DEALER done')
" 2>&1

# Check server log
echo ""
echo "[4] Server log RecvMetadata ROUTER diag lines:"
grep "RecvMetadata ROUTER diag" "$SERVER_LOG" | grep "port=9516" | tail -5

echo ""
echo "[5] Checking connections:"
ss -tnp 2>/dev/null | grep 9516

# Test 2: standalone spack libzmq ROUTER on a different port
echo ""
echo "[6] Standalone spack libzmq ROUTER test on port 19517..."
timeout 10 python3 << 'PYEOF2'
import ctypes, time, threading

LIBZMQ = "/mnt/common/jcernudagarcia/spack/var/spack/environments/mchips/.spack-env/view/lib/libzmq.so.5"
zmq = ctypes.cdll.LoadLibrary(LIBZMQ)
zmq.zmq_ctx_new.restype = ctypes.c_void_p
zmq.zmq_socket.restype = ctypes.c_void_p

# Create standalone ROUTER (like Chimaera but without Chimaera runtime)
r_ctx = ctypes.c_void_p(zmq.zmq_ctx_new())
zmq.zmq_ctx_set(r_ctx, 1, 2)
r_sock = ctypes.c_void_p(zmq.zmq_socket(r_ctx, 6))
m = ctypes.c_int(1)
zmq.zmq_setsockopt(r_sock, 33, ctypes.byref(m), 4)
rc = zmq.zmq_bind(r_sock, b"tcp://0.0.0.0:19517")
print(f"  Standalone ROUTER bind: rc={rc}")

# Create DEALER and connect
d_ctx = ctypes.c_void_p(zmq.zmq_ctx_new())
zmq.zmq_ctx_set(d_ctx, 1, 2)
d_sock = ctypes.c_void_p(zmq.zmq_socket(d_ctx, 5))
identity = b"standalone-test"
zmq.zmq_setsockopt(d_sock, 5, identity, len(identity))
zmq.zmq_connect(d_sock, b"tcp://127.0.0.1:19517")
time.sleep(1)

zmq.zmq_send(d_sock, b"", 0, 2)
zmq.zmq_send(d_sock, b"STANDALONE-MSG", 14, 0)
print("  Sent to standalone ROUTER")

buf = ctypes.create_string_buffer(1024)
for i in range(30):
    rc = zmq.zmq_recv(r_sock, buf, 1024, 1)
    if rc >= 0:
        print(f"  PASS: Standalone ROUTER received ({rc} bytes)")
        break
    time.sleep(0.1)
else:
    print("  FAIL: Standalone ROUTER never received")

l = ctypes.c_int(0)
zmq.zmq_setsockopt(d_sock, 17, ctypes.byref(l), 4)
zmq.zmq_setsockopt(r_sock, 17, ctypes.byref(l), 4)
zmq.zmq_close(d_sock)
zmq.zmq_close(r_sock)
zmq.zmq_ctx_destroy(d_ctx)
zmq.zmq_ctx_destroy(r_ctx)
PYEOF2

kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc "$SERVER_LOG" 2>/dev/null || true
echo ""
echo "Done."
