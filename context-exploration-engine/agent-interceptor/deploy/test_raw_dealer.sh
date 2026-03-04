#!/bin/bash
# Test: Send a raw ZMQ DEALER message to a running Chimaera server's ROUTER (port 9516)
# This bypasses Chimaera's client code entirely.
set +e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKTREE="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$WORKTREE/build"
CONF_DIR="$SCRIPT_DIR/../demo"

echo "=== Raw DEALER → Chimaera ROUTER Test ==="
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
SERVER_LOG="/tmp/chi_raw_dealer_$$.log"
echo "[1] Starting server..."
"$BUILD_DIR/bin/dt_demo_server" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 12

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "FAIL: Server died"
    cat "$SERVER_LOG"
    exit 1
fi
echo "    Server alive (PID=$SERVER_PID)"

# Now send a raw DEALER message using spack libzmq via ctypes
echo ""
echo "[2] Sending raw DEALER message to port 9516..."
timeout 15 python3 << 'PYEOF'
import ctypes, time, os

# Use the SAME spack libzmq that Chimaera uses
LIBZMQ = "/mnt/common/jcernudagarcia/spack/var/spack/environments/mchips/.spack-env/view/lib/libzmq.so.5"
zmq = ctypes.cdll.LoadLibrary(LIBZMQ)
zmq.zmq_ctx_new.restype = ctypes.c_void_p
zmq.zmq_socket.restype = ctypes.c_void_p
zmq.zmq_strerror.restype = ctypes.c_char_p

PORT = 9516

# Create DEALER with exact same settings as Chimaera's client
ctx = ctypes.c_void_p(zmq.zmq_ctx_new())
zmq.zmq_ctx_set(ctx, 1, 2)  # ZMQ_IO_THREADS = 2

dealer = ctypes.c_void_p(zmq.zmq_socket(ctx, 5))  # ZMQ_DEALER

hostname = os.uname().nodename
pid = str(os.getpid())
identity = f"{hostname}:{pid}".encode()
zmq.zmq_setsockopt(dealer, 5, identity, len(identity))  # ZMQ_IDENTITY

imm = ctypes.c_int(0)
zmq.zmq_setsockopt(dealer, 39, ctypes.byref(imm), 4)  # ZMQ_IMMEDIATE

to = ctypes.c_int(5000)
zmq.zmq_setsockopt(dealer, 28, ctypes.byref(to), 4)  # ZMQ_SNDTIMEO

sndbuf = ctypes.c_int(4*1024*1024)
zmq.zmq_setsockopt(dealer, 11, ctypes.byref(sndbuf), 4)  # ZMQ_SNDBUF
rcvbuf = ctypes.c_int(4*1024*1024)
zmq.zmq_setsockopt(dealer, 12, ctypes.byref(rcvbuf), 4)  # ZMQ_RCVBUF

hb1 = ctypes.c_int(1000)
zmq.zmq_setsockopt(dealer, 75, ctypes.byref(hb1), 4)  # HB_IVL
hb2 = ctypes.c_int(3000)
zmq.zmq_setsockopt(dealer, 76, ctypes.byref(hb2), 4)  # HB_TIMEOUT
zmq.zmq_setsockopt(dealer, 77, ctypes.byref(hb2), 4)  # HB_TTL

url = f"tcp://127.0.0.1:{PORT}".encode()
rc = zmq.zmq_connect(dealer, url)
print(f"  DEALER connect: rc={rc}")

print("  Waiting 3s for ZMTP handshake...")
time.sleep(3)

# Check socket events
evts = ctypes.c_int(0)
evts_len = ctypes.c_size_t(4)
zmq.zmq_getsockopt(dealer, 15, ctypes.byref(evts), ctypes.byref(evts_len))  # ZMQ_EVENTS
pollin = 1 if (evts.value & 1) else 0
pollout = 1 if (evts.value & 2) else 0
print(f"  DEALER events: {evts.value} (POLLIN={pollin}, POLLOUT={pollout})")

# Send empty delimiter + simple payload
print("  Sending delimiter + payload...")
rc1 = zmq.zmq_send(dealer, b"", 0, 2)  # ZMQ_SNDMORE
payload = b"RAW-DEALER-TEST-MESSAGE"
rc2 = zmq.zmq_send(dealer, payload, len(payload), 0)
print(f"  Delimiter rc={rc1}, Payload rc={rc2}")

if rc2 < 0:
    err = zmq.zmq_errno()
    print(f"  SEND FAILED: errno={err}: {zmq.zmq_strerror(err).decode()}")
else:
    print(f"  Send appears successful. Waiting 5s for server to process...")
    time.sleep(5)

# Check events again
zmq.zmq_getsockopt(dealer, 15, ctypes.byref(evts), ctypes.byref(evts_len))
pollin = 1 if (evts.value & 1) else 0
pollout = 1 if (evts.value & 2) else 0
print(f"  DEALER events after wait: {evts.value} (POLLIN={pollin}, POLLOUT={pollout})")

# Close
linger = ctypes.c_int(0)
zmq.zmq_setsockopt(dealer, 17, ctypes.byref(linger), 4)  # ZMQ_LINGER
zmq.zmq_close(dealer)
zmq.zmq_ctx_destroy(ctx)
print("  DEALER closed")
PYEOF

echo ""
echo "[3] Server log after raw DEALER test:"
echo "    RecvMetadata diag lines:"
grep "RecvMetadata ROUTER diag" "$SERVER_LOG" | tail -5
echo ""
echo "    ClientRecv lines:"
grep "ClientRecv" "$SERVER_LOG" | tail -5
echo ""
echo "    Any received task lines:"
grep -i "received task\|Got identity" "$SERVER_LOG" | head -5

kill -9 $SERVER_PID 2>/dev/null || true
rm -f /tmp/chimaera_jcernudagarcia/chimaera_9513.ipc "$SERVER_LOG" 2>/dev/null || true
echo ""
echo "Done."
