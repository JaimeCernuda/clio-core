# /// script
# requires-python = ">=3.10"
# dependencies = ["claude-agent-sdk"]
# ///
"""Minimal test: two sequential queries through the proxy."""

import os
import signal
import subprocess
import sys
import time
import urllib.request

import anyio
from claude_agent_sdk import query, ClaudeAgentOptions, AssistantMessage, ResultMessage, TextBlock

os.environ.pop("CLAUDECODE", None)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.realpath(os.path.join(os.path.dirname(SCRIPT_DIR), "..", "..", "build"))
CONFIG = os.path.join(os.path.dirname(SCRIPT_DIR), "demo", "wrp_conf.yaml")
BINARY = os.path.join(BUILD_DIR, "bin", "dt_demo_server")

PORT = 9090
SERVER_LOG = os.path.join(SCRIPT_DIR, "server.log")

# Kill any leftover servers
subprocess.run(["pkill", "-f", "dt_demo_server"], capture_output=True)
time.sleep(1)

# Start server (capture stderr for debugging)
env = os.environ.copy()
env["CHI_SERVER_CONF"] = CONFIG
print(f"Starting server: {BINARY}", flush=True)
print(f"Config: {CONFIG}", flush=True)
print(f"Build dir: {BUILD_DIR}", flush=True)
with open(SERVER_LOG, "w") as log:
    server = subprocess.Popen([BINARY], env=env, stdout=log, stderr=log)

# Wait for ready
for i in range(30):
    try:
        req = urllib.request.Request(f"http://localhost:{PORT}/v1/messages", method="GET")
        with urllib.request.urlopen(req, timeout=2):
            print(f"Server ready (attempt {i+1})", flush=True)
            break
    except Exception:
        if server.poll() is not None:
            print(f"Server died with code {server.returncode}", flush=True)
            sys.exit(1)
        time.sleep(1)


async def do_query(session: str, prompt: str):
    print(f"\n--- Query: session={session} prompt={prompt} ---", flush=True)
    opts = ClaudeAgentOptions(
        max_turns=1,
        permission_mode="bypassPermissions",
        env={"ANTHROPIC_BASE_URL": f"http://localhost:{PORT}/_session/{session}"},
    )
    try:
        async for msg in query(prompt=prompt, options=opts):
            if isinstance(msg, AssistantMessage):
                for block in msg.content:
                    if isinstance(block, TextBlock):
                        print(f"  Answer: {block.text}", flush=True)
            elif isinstance(msg, ResultMessage):
                print(f"  Result: is_error={msg.is_error} session={msg.session_id}", flush=True)
    except Exception as e:
        print(f"  ERROR: {e}", flush=True)


async def main():
    await do_query("q1", "What is 2+2? Reply only with the number.")
    print("\n--- First query done, starting second ---", flush=True)
    await do_query("q2", "What is 3+3? Reply only with the number.")
    print("\nBoth queries completed!", flush=True)


anyio.run(main)

# Cleanup
server.send_signal(signal.SIGTERM)
server.wait(timeout=10)
