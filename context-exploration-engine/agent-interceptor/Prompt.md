# DTProvenance — Ares Build & Test Playbook

## Environment

You are running on **Ares**, an HPC cluster at IIT Gnosis Research Center.

- **OS:** Rocky Linux 8, x86_64
- **Job scheduler:** SLURM (`sbatch`, `srun`, `salloc`)
- **Package manager:** Spack (environment `mchips` has all dependencies)
- **Compute nodes** have no internet access — FetchContent deps must be cached or pre-fetched on a login node
- **Login nodes** have internet but limited CPU/memory — do builds and tests inside SLURM jobs or `salloc` interactive sessions
- **Working directory:** `~/MChiPs/` (this repo)
- **Submodule:** `clio-core/` at branch `DTProvenance`

### Getting started on Ares

```bash
# 1. Clone (if not already present)
cd ~/
git clone --recursive https://github.com/JaimeCernuda/MChiPs.git
cd MChiPs
git submodule update --init --recursive

# 2. Switch clio-core to the DTProvenance branch
cd clio-core
git checkout DTProvenance
cd ..

# 3. Get an interactive session for building/testing
salloc -N 1 --cpus-per-task=16 --mem=32G --time=02:00:00

# 4. Activate spack
spack env activate mchips
```

All commands below assume you are inside an `salloc` session with `mchips` active.

---

## Mission

Build and test the DTProvenance agent interception pipeline. Work through
phases A → B → C → D → E in order. Each phase has a **gate** — all tests in
that phase must pass before you move on. If a test fails, fix the code and
re-run. Do not skip phases.

**Commit after each phase** with `feat:` or `fix:` prefix.

---

## Architecture Overview

DTProvenance intercepts LLM agent HTTP traffic, parses provider-specific
request/response formats, and stores structured interaction records in CTE.

```
Agent → HTTP Proxy (pool 800) → Provider Interception (801-803)
                                       ↓
                              Conversation Tracker (810) → CTE Storage
```

**Components:**
| Component | Target | Chimaera? |
|---|---|---|
| `dt_protocol` | `libdt_protocol.so` | No |
| `dt_proxy_http` | `libdt_proxy_http.so` | No |
| `dt_tracker_threading` | `libdt_tracker_threading.so` | No |
| `dt_proxy` ChiMod | `libdt_proxy_*.so` | Yes |
| `dt_intercept_anthropic` ChiMod | `libdt_intercept_anthropic_*.so` | Yes |
| `dt_intercept_openai` ChiMod | `libdt_intercept_openai_*.so` | Yes |
| `dt_intercept_ollama` ChiMod | `libdt_intercept_ollama_*.so` | Yes |
| `dt_tracker` ChiMod | `libdt_tracker_*.so` | Yes |
| `dt_demo_server` | executable | Yes |
| `ctx_writer` | executable | Yes |

**Test executables (17 total):**
- Protocol: `test_provider_detection`, `test_session_extraction`, `test_anthropic_parser`, `test_openai_parser`, `test_stream_reassembly`, `test_interaction_record`
- Interception: `test_anthropic_parsing`, `test_anthropic_forwarding`, `test_openai_parsing`, `test_openai_forwarding`, `test_ollama_parsing`, `test_ollama_forwarding`
- Proxy: `test_http_proxy`, `test_session_guard`
- Tracker: `test_conversation_threading`

---

## Phase A: Environment + Build

### A0. Pre-fetch dependencies (login node, one time only)

FetchContent needs internet. Compute nodes may not have it. Run the CMake
configure step once on a login node to populate the build/_deps cache, then
do the actual build inside `salloc`.

```bash
# On login node (has internet)
spack env activate mchips
cd clio-core && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja \
  -DWRP_CORE_ENABLE_CTE=ON -DWRP_CORE_ENABLE_CEE=ON \
  -DDT_PROVENANCE_ENABLE_TESTS=ON
# This downloads nlohmann_json, cpp-httplib, Catch2 into build/_deps/
# Cancel the build (Ctrl-C) — you'll do the real build in salloc
cd ../..
```

### A1. Activate spack environment

```bash
spack env activate mchips
```

### A2. Configure CMake

```bash
cd clio-core
mkdir -p build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -GNinja \
  -DWRP_CORE_ENABLE_CTE=ON \
  -DWRP_CORE_ENABLE_CEE=ON \
  -DDT_PROVENANCE_ENABLE_TESTS=ON
```

**Expected output:** CMake should print:
```
-- Context Exploration Engine: Building Agent Interceptor (DTProvenance)
```

If CMake fails, check:
- `spack env status` shows `mchips` active
- `nlohmann_json`, `cpp-httplib`, `Catch2` will be fetched via FetchContent (needs internet on first build)

### A3. Build everything

```bash
# Build all DTProvenance targets
ninja -j$(nproc) \
  dt_protocol \
  dt_proxy_http \
  dt_tracker_threading \
  dt_demo_server \
  ctx_writer

# Build all test executables
ninja -j$(nproc) \
  test_provider_detection \
  test_session_extraction \
  test_anthropic_parser \
  test_openai_parser \
  test_stream_reassembly \
  test_interaction_record \
  test_anthropic_parsing \
  test_anthropic_forwarding \
  test_openai_parsing \
  test_openai_forwarding \
  test_ollama_parsing \
  test_ollama_forwarding \
  test_http_proxy \
  test_session_guard \
  test_conversation_threading
```

**Build failure triage:**
- If ChiMod targets fail (`add_chimod_client`/`add_chimod_runtime` not found): ensure `WRP_CORE_ENABLE_CTE=ON` and `WRP_CORE_ENABLE_CEE=ON`
- If httplib errors: check FetchContent downloaded `cpp-httplib v0.18.3`
- If nlohmann_json errors: check FetchContent downloaded `nlohmann_json v3.11.3`

### A4. Verify build artifacts

```bash
# Standalone libraries (no Chimaera dependency)
ls -la lib/libdt_protocol.so
ls -la lib/libdt_proxy_http.so
ls -la lib/libdt_tracker_threading.so

# ChiMod libraries
ls -la lib/libdt_proxy_*.so
ls -la lib/libdt_intercept_anthropic_*.so
ls -la lib/libdt_intercept_openai_*.so
ls -la lib/libdt_intercept_ollama_*.so
ls -la lib/libdt_tracker_*.so

# Executables
ls -la bin/dt_demo_server
ls -la bin/ctx_writer
```

**Gate: All 3 standalone libs, 5 ChiMod lib pairs, and 2 executables must exist.**

---

## Phase B: Protocol Library Tests (standalone, no Chimaera needed)

These tests exercise the core protocol parsing logic. They use only `dt_protocol`
and Catch2 — no Chimaera runtime, no network, no shared memory.

```bash
cd clio-core/build

# Run all 6 protocol unit tests
ctest --output-on-failure -R "test_provider_detection"
ctest --output-on-failure -R "test_session_extraction"
ctest --output-on-failure -R "test_anthropic_parser"
ctest --output-on-failure -R "test_openai_parser"
ctest --output-on-failure -R "test_stream_reassembly"
ctest --output-on-failure -R "test_interaction_record"
```

Or run them all at once:

```bash
ctest --output-on-failure \
  -R "test_provider_detection|test_session_extraction|test_anthropic_parser|test_openai_parser|test_stream_reassembly|test_interaction_record"
```

**What each test covers:**

| Test | What it verifies |
|---|---|
| `test_provider_detection` | URL path → provider enum mapping (Anthropic, OpenAI, Ollama, Unknown) |
| `test_session_extraction` | HTTP header → session ID extraction per provider |
| `test_anthropic_parser` | Anthropic Messages API request/response JSON parsing, token extraction |
| `test_openai_parser` | OpenAI Chat Completions API request/response JSON parsing |
| `test_stream_reassembly` | SSE chunk reassembly into complete response JSON |
| `test_interaction_record` | InteractionRecord construction, serialization, cost estimation |

**Failure triage:**
- JSON parse failures → check `nlohmann_json` version is v3.11.3
- Missing symbols → `libdt_protocol.so` not on `LD_LIBRARY_PATH`

**Gate: All 6 protocol tests must pass before proceeding.**

---

## Phase C: Interception + Proxy Tests (httplib mock servers, no Chimaera)

These tests spin up local httplib mock servers to test request forwarding and
response parsing without hitting real APIs. They also test HTTP proxy routing
and session guard logic.

### C1. Interception parsing tests (protocol lib + Catch2 only)

```bash
ctest --output-on-failure \
  -R "test_anthropic_parsing|test_openai_parsing|test_ollama_parsing"
```

### C2. Forwarding tests (mock httplib servers)

```bash
ctest --output-on-failure \
  -R "test_anthropic_forwarding|test_openai_forwarding|test_ollama_forwarding"
```

### C3. Proxy tests

```bash
ctest --output-on-failure -R "test_http_proxy|test_session_guard"
```

### C4. Tracker threading test

```bash
ctest --output-on-failure -R "test_conversation_threading"
```

Or run all Phase C tests at once:

```bash
ctest --output-on-failure \
  -R "test_anthropic_parsing|test_openai_parsing|test_ollama_parsing|test_anthropic_forwarding|test_openai_forwarding|test_ollama_forwarding|test_http_proxy|test_session_guard|test_conversation_threading"
```

**What each test covers:**

| Test | What it verifies |
|---|---|
| `test_anthropic_parsing` | Anthropic-specific header/body parsing in interception context |
| `test_openai_parsing` | OpenAI-specific header/body parsing in interception context |
| `test_ollama_parsing` | Ollama-specific header/body parsing in interception context |
| `test_anthropic_forwarding` | Mock server → forward → capture response (Anthropic format) |
| `test_openai_forwarding` | Mock server → forward → capture response (OpenAI format) |
| `test_ollama_forwarding` | Mock server → forward → capture response (Ollama format) |
| `test_http_proxy` | HTTP proxy route registration, request dispatch, error handling |
| `test_session_guard` | Session ID extraction, validation, concurrent session isolation |
| `test_conversation_threading` | Conversation tree construction, sequence numbering, thread merging |

**Failure triage:**
- Port conflicts → forwarding tests bind ephemeral ports; if they fail, check for port exhaustion
- Timeout failures → httplib default timeout is 5s; increase if on slow node

**Gate: All 9 tests must pass before proceeding.**

---

## Phase D: Full Integration (requires Chimaera runtime)

This phase starts the full DTProvenance pipeline as a Chimaera application.

### D1. Start DTProvenance server

```bash
cd clio-core/build

# Start the demo server with compose config
./bin/dt_demo_server \
  --config ../context-exploration-engine/agent-interceptor/demo/wrp_conf.yaml &
PROXY_PID=$!

# Wait for Chimaera + HTTP server to initialize
sleep 10

# Verify server is running
if ! kill -0 $PROXY_PID 2>/dev/null; then
  echo "FAIL: DTProvenance server failed to start"
  exit 1
fi
echo "DTProvenance server started (PID=$PROXY_PID) on port 9090"
```

**Expected startup:**
- Chimaera runtime initializes (shared memory, task queues)
- 5 ChiMods register: dt_proxy (800), dt_intercept_anthropic (801), dt_intercept_openai (802), dt_intercept_ollama (803), dt_tracker (810)
- HTTP proxy listens on port 9090

### D2. Smoke test with curl

```bash
# Test proxy health (should return 200 or route to upstream)
curl -v http://localhost:9090/health 2>&1 | head -20

# Test Anthropic-style request (will fail auth but proves routing works)
curl -X POST http://localhost:9090/v1/messages \
  -H "Content-Type: application/json" \
  -H "x-api-key: test-key" \
  -d '{"model":"claude-sonnet-4-6","messages":[{"role":"user","content":"test"}],"max_tokens":10}' \
  2>&1 | head -20
```

### D3. Verify CTE storage with ctx_writer

```bash
./bin/ctx_writer \
  --expected-sessions 0 \
  --expected-tag-prefix "Agentic_session_"
```

This should report 0 sessions (no real traffic yet) and exit 0.

### D4. Cleanup

```bash
kill $PROXY_PID 2>/dev/null || true
wait $PROXY_PID 2>/dev/null || true
```

**Gate: Server starts, proxy accepts connections, ctx_writer runs without error.**

---

## Phase E: Real End-to-End with Claude Agents

This phase routes real Claude agent traffic through DTProvenance. Requires:
- **Claude Code OAuth token** already present in `~/.claude/` (auto-discovered by `claude_agent_sdk` — no `ANTHROPIC_API_KEY` needed)
- `claude_agent_sdk` Python package installed (`uv add claude-agent-sdk`)
- A SLURM allocation on Ares (the test_deployment.sbatch handles this)
- **Network access** to api.anthropic.com from the compute node (check with `curl -s https://api.anthropic.com`)

### E1. Submit via SLURM (preferred)

```bash
# From MChiPs project root (~/MChiPs)
mkdir -p logs
sbatch clio-core/context-exploration-engine/agent-interceptor/deploy/test_deployment.sbatch

# Monitor
squeue -u $USER
tail -f logs/test_*.out
```

### E2. Or run manually (inside an existing salloc session)

```bash
# 1. Start server
cd clio-core/build
./bin/dt_demo_server \
  --config ../context-exploration-engine/agent-interceptor/demo/wrp_conf.yaml &
PROXY_PID=$!
sleep 10

# 2. Unset CLAUDECODE to avoid nested session detection
unset CLAUDECODE 2>/dev/null || true

# 3. Run 3 agents
uv run python deploy/run_agents.py \
  --proxy-host "$(hostname)" \
  --proxy-port 9090 \
  --sessions "agent-0" "agent-1" "agent-2" \
  --prompts "What is 2+2?" "Name the planets in our solar system" "Write a haiku about code"

# 4. Verify storage
./bin/ctx_writer \
  --expected-sessions 3 \
  --expected-tag-prefix "Agentic_session_" \
  --min-interactions-per-session 1

# 5. Also verify with Python
uv run python deploy/verify_storage.py \
  --expected-sessions 3 \
  --tag-prefix "Agentic_session_"

# 6. Cleanup
kill $PROXY_PID 2>/dev/null || true
```

### E3. What success looks like

```
Sessions found: 3

Session: agent-0 (1 interactions)
  [0] model=claude-sonnet-4-6 OK

Session: agent-1 (1 interactions)
  [0] model=claude-sonnet-4-6 OK

Session: agent-2 (1 interactions)
  [0] model=claude-sonnet-4-6 OK

=== Result: PASS ===
```

**Gate: 3 sessions stored in CTE, each with at least 1 interaction record containing valid provider, model, and token counts.**

---

## Quick Reference: Run All Tests

```bash
# From clio-core/build — runs all 15+ DTProvenance tests
ctest --output-on-failure \
  -R "test_provider_detection|test_session_extraction|test_anthropic_parser|test_openai_parser|test_stream_reassembly|test_interaction_record|test_anthropic_parsing|test_openai_parsing|test_ollama_parsing|test_anthropic_forwarding|test_openai_forwarding|test_ollama_forwarding|test_http_proxy|test_session_guard|test_conversation_threading"
```

## Quick Reference: Build + Test One-Liner

```bash
spack env activate mchips && \
cd clio-core && mkdir -p build && cd build && \
cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja \
  -DWRP_CORE_ENABLE_CTE=ON -DWRP_CORE_ENABLE_CEE=ON \
  -DDT_PROVENANCE_ENABLE_TESTS=ON && \
ninja -j$(nproc) && \
ctest --output-on-failure -R "test_provider|test_session|test_anthropic|test_openai|test_stream|test_interaction|test_ollama|test_http_proxy|test_session_guard|test_conversation"
```
