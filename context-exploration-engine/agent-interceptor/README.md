# DTProvenance Agent Interceptor

A transparent HTTP proxy that intercepts LLM API traffic from AI agents, captures interaction records (tokens, cost, latency, conversation threading), and stores them via the Chimaera runtime. Agents connect through the proxy using `ANTHROPIC_BASE_URL` — no agent code changes required.

## Architecture

```
                          +-----------------------+
  Agent (claude CLI)      |   DTProvenance Server |
  ANTHROPIC_BASE_URL  --> |                       |
  http://host:9090/       |  +------+  +--------+ |    +-------------------+
  _session/{id}/v1/msg    |  |Proxy |->|Anthropic|----->| api.anthropic.com |
                          |  |:9090 |  |Intercept| |    +-------------------+
                          |  |      |->|OpenAI   |----->| api.openai.com    |
                          |  |      |  |Intercept| |    +-------------------+
                          |  |      |->|Ollama   |----->| localhost:11434   |
                          |  +------+  +--------+ |    +-------------------+
                          |       |                |
                          |  +----v----+           |
                          |  | Tracker |           |
                          |  | (store) |           |
                          |  +---------+           |
                          +-----------------------+
```

### Request Flow

1. Agent sends `POST /_session/{session_id}/v1/messages` with `"stream": true`
2. **Proxy ChiMod** (port 9090) extracts `session_id`, detects provider from path/headers, creates a `StreamBuffer`
3. **Interception ChiMod** (Anthropic/OpenAI/Ollama) forwards the request to the upstream API, streaming chunks back through the `StreamBuffer` to the agent in real-time
4. After the response completes, the interception module parses the full response (SSE reassembly for Anthropic/OpenAI, NDJSON for Ollama), builds an `InteractionRecord`, and dispatches it to the **Tracker ChiMod**
5. Tracker stores the interaction keyed by `session_id` with a monotonic sequence ID

### Session Routing

Sessions are encoded in the URL path: `/_session/{session_id}/original/path`. The proxy strips this prefix before forwarding, so the upstream API sees the original path. Agents that don't include a session prefix get a fake rejection response (the proxy needs a session to track).

### Provider Detection

The proxy auto-detects the LLM provider from the request:
- `/v1/messages` -> Anthropic
- `/api/*` -> Ollama
- `/v1/*` + `anthropic-version` header -> Anthropic
- `/v1/*` (fallback) -> OpenAI

### Modules

| Module | Pool ID | Description |
|--------|---------|-------------|
| `dt_provenance_dt_proxy` | 800 | HTTP proxy server on port 9090 |
| `dt_provenance_dt_intercept_anthropic` | 801 | Anthropic API forwarding + parsing |
| `dt_provenance_dt_intercept_openai` | 802 | OpenAI API forwarding + parsing |
| `dt_provenance_dt_intercept_ollama` | 803 | Ollama API forwarding + parsing |
| `dt_provenance_dt_tracker` | 810 | Conversation tracker (in-memory store) |

### Directory Layout

```
agent-interceptor/
  protocol/       # Standalone library: types, parsers, cost estimator, stream buffer
  proxy/          # HTTP Proxy ChiMod
  interception/
    anthropic/    # Anthropic interception ChiMod
    openai/       # OpenAI interception ChiMod
    ollama/       # Ollama interception ChiMod
  tracker/        # Conversation Tracker ChiMod
  demo/           # Server launcher + compose config (wrp_conf.yaml)
  deploy/         # Python agent scripts + integration tests
  tools/          # ctx_writer verification tool
```

## Build

### Prerequisites

- Spack environment `mchips` (provides Chimaera, hermes_shm, etc.)
- CMake 3.20+, Ninja
- OpenSSL development headers
- C++20 compiler

### Build Commands

```bash
# Activate spack environment
source /path/to/spack/share/spack/setup-env.sh
spack env activate mchips

# Configure (from clio-core root)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j$(nproc)
```

Build artifacts go to `build/bin/`:
- `dt_demo_server` — the server binary
- `ctx_writer` — tracker verification tool
- `test_*` — unit test binaries
- `build/lib/` and `build/bin/` — ChiMod shared libraries (`libdt_provenance_*.so`)

### Unit Tests

```bash
# Run all 16 DTProvenance unit tests
for t in build/bin/test_{http_proxy,anthropic_forwarding,openai_forwarding,ollama_forwarding,session_extraction,provider_detection,stream_reassembly,interaction_record,session_guard,session_manager,conversation_threading,anthropic_parser,openai_parser,anthropic_parsing,openai_parsing,ollama_parsing}; do
  echo "=== $(basename $t) ===" && $t
done
```

## Deploy

### 1. Start the Server

The server requires a compute node (or any node with internet access to reach the LLM APIs).

```bash
# Set the compose config
export CHI_SERVER_CONF=/path/to/agent-interceptor/demo/wrp_conf.yaml

# Start the server
./build/bin/dt_demo_server
```

The server starts all 5 ChiMods and listens on:
- Port **9513** — Chimaera RPC (internal)
- Port **9090** — HTTP proxy (agent-facing)

### 2. Connect Claude Code CLI

Point any Claude Code session at the proxy by setting `ANTHROPIC_BASE_URL`:

```bash
# Replace "mynode" with the hostname where the server is running
# Replace "my-session" with any session identifier you want
ANTHROPIC_BASE_URL="http://mynode:9090/_session/my-session" claude

# Or for a one-shot prompt:
ANTHROPIC_BASE_URL="http://mynode:9090/_session/my-session" claude -p "What is 2+2?"
```

All API traffic from that Claude Code session will be routed through the proxy, with every interaction captured and stored under the session `my-session`.

Multiple CLI sessions can run simultaneously with different session IDs — each gets independent tracking.

### 3. Connect via claude-agent-sdk (Python)

Install the SDK:

```bash
# Create a venv (if needed)
python -m venv deploy/.venv
uv pip install --python deploy/.venv/bin/python claude-agent-sdk
```

Run agents programmatically:

```python
import anyio
from claude_agent_sdk import query, ClaudeAgentOptions, AssistantMessage, TextBlock

async def main():
    opts = ClaudeAgentOptions(
        max_turns=1,
        permission_mode="bypassPermissions",
        env={"ANTHROPIC_BASE_URL": "http://mynode:9090/_session/my-agent"},
    )
    async for msg in query(prompt="What is 2+2?", options=opts):
        if isinstance(msg, AssistantMessage):
            for block in msg.content:
                if isinstance(block, TextBlock):
                    print(block.text)

anyio.run(main)
```

Key `ClaudeAgentOptions` parameters for session management:

| Parameter | Effect |
|-----------|--------|
| `env={"ANTHROPIC_BASE_URL": ".../_session/{id}"}` | Routes through proxy with session ID |
| `resume="<session_id>"` | Resumes a previous Claude conversation (carries context) |
| `continue_conversation=True` | Continues the most recent conversation |
| (default — none of the above) | Fresh conversation, no prior context |

### 4. Run the Deploy Script

```bash
deploy/.venv/bin/python deploy/run_agents.py \
    --proxy-host mynode \
    --proxy-port 9090 \
    --sessions agent-0 agent-1 agent-2 \
    --prompts "What is 2+2?" "Name one planet." "Write a haiku."
```

### 5. SLURM Usage

On a SLURM cluster, allocate a node and run the server there:

```bash
# Allocate a compute node
salloc -N 1

# On the node, start the server
export CHI_SERVER_CONF=/path/to/agent-interceptor/demo/wrp_conf.yaml
./build/bin/dt_demo_server &

# Now connect agents from anywhere that can reach the node
ANTHROPIC_BASE_URL="http://$(hostname):9090/_session/test" claude -p "Hello"
```

Or use sbatch for batch testing:

```bash
sbatch deploy/integration_test.sbatch
```

## Integration Tests

The integration test suite (`deploy/integration_test.py`) verifies the full pipeline end-to-end:

```bash
deploy/.venv/bin/python deploy/integration_test.py \
    --build-dir /path/to/clio-core/build \
    --proxy-port 9090
```

### Test Cases

| Test | What it verifies |
|------|------------------|
| **A: Single agent** | Basic query through proxy returns correct answer |
| **B: Multiple agents** | 3 agents with distinct sessions, all get independent responses |
| **C: Multi-turn resume** | Turn 1 sets a fact, Turn 2 resumes and recalls it |
| **D: Context isolation** | Fresh query (no resume) does NOT carry over context from previous query |
| **E: Tracker state** | `ctx_writer` verifies stored interactions via Chimaera IPC |

### What Gets Captured

Each interaction stored by the tracker includes:
- Session ID and monotonic sequence ID
- Provider (Anthropic/OpenAI/Ollama) and model name
- Request path, headers, body
- Response status, text, tool calls, stop reason
- Token counts (input, output, cache read, cache creation)
- Cost estimate (USD)
- Latency and time-to-first-token
- Conversation threading (turn number, parent sequence ID)

## Server Configuration

The compose config (`demo/wrp_conf.yaml`) controls all modules:

```yaml
networking:
  port: 9513            # Chimaera RPC port

compose:
  - mod_name: dt_provenance_dt_proxy
    pool_name: dt_proxy_pool
    pool_id: "800.0"
    config: |
      port: 9090          # HTTP proxy port
      num_threads: 8       # HTTP handler threads

  - mod_name: dt_provenance_dt_intercept_anthropic
    pool_name: dt_intercept_anthropic_pool
    pool_id: "801.0"
    config: |
      upstream_base_url: "https://api.anthropic.com"

  - mod_name: dt_provenance_dt_intercept_openai
    pool_name: dt_intercept_openai_pool
    pool_id: "802.0"
    config: |
      upstream_base_url: "https://api.openai.com"

  - mod_name: dt_provenance_dt_intercept_ollama
    pool_name: dt_intercept_ollama_pool
    pool_id: "803.0"
    config: |
      upstream_base_url: "http://localhost:11434"

  - mod_name: dt_provenance_dt_tracker
    pool_name: dt_tracker_pool
    pool_id: "810.0"
```

## Troubleshooting

**Port already in use**: Kill leftover servers with `pkill -f dt_demo_server` before starting a new one.

**Agent hangs**: Ensure `ANTHROPIC_BASE_URL` includes `/_session/{id}` — without it, the proxy returns a rejection response.

**CLAUDECODE env var**: When launching `claude-agent-sdk` from within a Claude Code session, unset `CLAUDECODE` first: `unset CLAUDECODE`.

**Build on compute nodes**: If `libaio.so` is missing, build on the login node instead. Use `-DCCACHE_PROGRAM=CCACHE_PROGRAM-NOTFOUND` if ccache causes issues.
