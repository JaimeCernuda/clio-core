# DTProvenance Deployment Guide

Deploy the DTProvenance agent interception pipeline on the Ares cluster. This system intercepts Claude/OpenAI/Ollama API traffic, records provenance data in CTE, and provides a live dashboard.

## Architecture

```
Claude Agent  ──HTTP──▶  Flask Bridge (:9090)  ──IPC──▶  Chimaera Runtime (:9513)
                              │                              ├─ Proxy ChiMod (pool 800)
                              │                              │    └─ ForwardHttp task → I/O Workers (1-5)
                              │                              ├─ Anthropic Interceptor (pool 801)
                              │                              ├─ OpenAI Interceptor (pool 802)
                              │                              ├─ Ollama Interceptor (pool 803)
                              │                              ├─ Tracker (pool 810)
                              │                              ├─ Ctx Untangler (pool 820)
                              │                              └─ CTE Storage (pool 512)
                              │
Browser  ──HTTP──▶  Dashboard (/provenance)
```

HTTP forwarding runs on Chimaera I/O workers (1-5), keeping Worker 0 free for scheduling. Multiple concurrent agents' API calls execute in parallel.

## Prerequisites

- Ares cluster access with Spack `mchips` environment at `/mnt/common/jcernudagarcia/spack/`
- Claude Code OAuth token (auto-discovered from `~/.claude/`)
- `uv` available for Python environment management

## Key Files

| File | Purpose |
|------|---------|
| `demo/wrp_conf.yaml` | Chimaera server compose config (all ChiMod pools) |
| `demo/dt_demo_server.cc` | Server binary entry point |
| `deploy/test_e2e.sh` | Full automated E2E test suite |
| `deploy/run_agents.py` | Agent launcher using `claude-agent-sdk` |
| `deploy/.venv/` | Python venv (created automatically by test scripts) |
| `proxy/src/proxy_runtime.cc` | Proxy runtime (ForwardHttp + Monitor handlers) |
| `tracker/src/tracker_runtime.cc` | Tracker runtime (CTE storage) |

## 1. Build

**Build on a login node.** Compute nodes may lack `libaio.so`.

```bash
# Activate build environment
source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

# Clone and checkout
git clone git@github.com:iowarp/clio-core.git
cd clio-core
git checkout DTProvenance
git submodule update --init --recursive

# Configure and build
mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DWRP_CORE_ENABLE_CTE=ON \
  -DWRP_CORE_ENABLE_CEE=ON
ninja dt_demo_server
ninja dt_provenance_dt_proxy_runtime \
      dt_provenance_dt_intercept_anthropic_runtime \
      dt_provenance_dt_intercept_openai_runtime \
      dt_provenance_dt_intercept_ollama_runtime \
      dt_provenance_dt_tracker_runtime \
      dt_provenance_dt_ctx_untangler_runtime
```

If building on a **compute node** (not recommended), disable ccache:
```bash
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DWRP_CORE_ENABLE_CTE=ON \
  -DWRP_CORE_ENABLE_CEE=ON \
  -DCCACHE_PROGRAM=CCACHE_PROGRAM-NOTFOUND
```

## 2. Deploy on a Compute Node

### Allocate a node

```bash
salloc -N 1
```

### Set environment

```bash
source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

# Adjust REPO_ROOT to wherever you cloned clio-core
REPO_ROOT=$HOME/clio-core
BUILD_DIR=$REPO_ROOT/build
CONF_DIR=$REPO_ROOT/context-exploration-engine/agent-interceptor/demo
VIS_DIR=$REPO_ROOT/context-visualizer
DEPLOY_DIR=$REPO_ROOT/context-exploration-engine/agent-interceptor/deploy

export LD_LIBRARY_PATH="$BUILD_DIR/bin:${LD_LIBRARY_PATH:-}"
export CHI_SERVER_CONF="$CONF_DIR/wrp_conf.yaml"
export PYTHONPATH="$BUILD_DIR/bin:${PYTHONPATH:-}"
```

### Start Chimaera server

```bash
$BUILD_DIR/bin/dt_demo_server &
sleep 8   # Allow all ChiMods to initialize
```

The server loads all ChiMods from `wrp_conf.yaml` (Proxy, Anthropic/OpenAI/Ollama interceptors, Tracker, Ctx Untangler, CTE).

### Start Flask bridge

```bash
# Create venv if needed
if [ ! -d "$DEPLOY_DIR/.venv" ]; then
  uv venv "$DEPLOY_DIR/.venv"
fi
VENV_PYTHON="$DEPLOY_DIR/.venv/bin/python"
uv pip install --python "$VENV_PYTHON" flask msgpack claude-agent-sdk

# Start Flask
cd $VIS_DIR
FLASK_APP=context_visualizer.app $VENV_PYTHON -m flask run \
  --host 0.0.0.0 --port 9090 &
```

Flask translates HTTP requests to Chimaera IPC Monitor calls and serves the dashboard.

## 3. Run Claude Agents Through the Interceptor

```bash
# IMPORTANT: Unset CLAUDECODE to avoid nested session detection
unset CLAUDECODE

# Option A: Use run_agents.py
$VENV_PYTHON $DEPLOY_DIR/run_agents.py \
  --proxy-host localhost \
  --proxy-port 9090 \
  --sessions "my-session" \
  --prompts "Write a hello world program in Python."

# Option B: Direct claude CLI
export ANTHROPIC_BASE_URL=http://localhost:9090/_session/my-session
claude --model claude-sonnet-4-6 -p "What is 2+2?"
```

All traffic routes through the interceptor: Flask receives the request, dispatches to the Proxy ChiMod via Chimaera IPC, the Proxy identifies the provider and forwards to the appropriate interceptor (Anthropic/OpenAI/Ollama), which calls the upstream API and records the interaction in the Tracker and CTE.

## 4. Dashboard

The web dashboard is served by Flask at `/provenance`.

### SSH port forwarding (access from local machine)

```bash
# Two-hop tunnel: local → ares login → compute node
ssh -L 9090:localhost:9090 ares -t ssh -L 9090:localhost:9090 ares-comp-XX
```

Then open `http://localhost:9090/provenance` in your browser.

The dashboard supports:
- **List sessions** — see all active/completed agent sessions
- **Query session** — view all interactions for a specific session
- **Query graph** — view context diff graph for a session

## 5. E2E Test

### Automated (recommended)

The E2E test handles server startup, Flask, agent runs, and cleanup automatically:

```bash
salloc -N 1
bash $REPO_ROOT/context-exploration-engine/agent-interceptor/deploy/test_e2e.sh
```

This tests:
- **5a**: Single agent through proxy
- **5b**: Multiple concurrent agents (3 sessions)
- **5c**: Session resume (same session ID, follow-up prompt)
- **5d**: New session (`/clear` equivalent)
- **5e**: Persistence across server restart
- **5f**: Dashboard integration (list_sessions query)

### Multi-node scale test

```bash
salloc -N 2
bash $REPO_ROOT/context-exploration-engine/agent-interceptor/deploy/test_multinode.sh --agents-per-node 4
```

## 6. Stopping

```bash
# Use SIGKILL for the Chimaera server (SIGTERM may not stop it)
kill -9 <SERVER_PID>
kill -9 <FLASK_PID>

# Clean up IPC socket
rm -f /tmp/chimaera_$(whoami)/chimaera_9513.ipc
```

## Troubleshooting

- **Server won't start**: Check that `CHI_SERVER_CONF` points to a valid `wrp_conf.yaml` and no stale IPC socket exists at `/tmp/chimaera_$(whoami)/chimaera_9513.ipc`.
- **Agent errors**: Ensure `CLAUDECODE` is unset. The `claude-agent-sdk` package must be installed in the venv (not `claude-code-sdk`).
- **Dashboard shows no sessions**: The proxy must have `WRP_CTE_CLIENT` initialized with the CTE pool. Check server startup logs for `cte_main` pool creation.
- **`chimaera_finalize()` segfault**: Known pre-existing issue. Use `os._exit(0)` in Python scripts to skip C++ destructor teardown.
