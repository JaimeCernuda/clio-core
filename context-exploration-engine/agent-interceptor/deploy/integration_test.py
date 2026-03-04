# /// script
# requires-python = ">=3.10"
# dependencies = ["claude-agent-sdk"]
# ///
"""End-to-end integration test for DTProvenance streaming proxy.

Starts the DTProvenance server, runs multiple agents through the proxy
via claude_agent_sdk, verifies responses and tracker state.

Usage (on a compute node):
    unset CLAUDECODE
    deploy/.venv/bin/python deploy/integration_test.py \
        --build-dir /path/to/clio-core/build \
        --proxy-port 9090
"""

import argparse
import atexit
import os
import signal
import subprocess
import sys
import time
import urllib.request
import urllib.error

import anyio
from claude_agent_sdk import (
    query,
    ClaudeAgentOptions,
    AssistantMessage,
    ResultMessage,
    TextBlock,
)

os.environ.pop("CLAUDECODE", None)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
AGENT_INTERCEPTOR_DIR = os.path.dirname(SCRIPT_DIR)

passed = 0
failed = 0


def report(name: str, ok: bool, detail: str = ""):
    global passed, failed
    tag = "PASS" if ok else "FAIL"
    print(f"  [{tag}] {name}" + (f" — {detail}" if detail else ""), flush=True)
    if ok:
        passed += 1
    else:
        failed += 1


# ---------------------------------------------------------------------------
# Server lifecycle
# ---------------------------------------------------------------------------

server_proc = None


def start_server(build_dir: str, proxy_port: int) -> subprocess.Popen:
    global server_proc
    config = os.path.join(AGENT_INTERCEPTOR_DIR, "demo", "wrp_conf.yaml")
    binary = os.path.join(build_dir, "bin", "dt_demo_server")

    if not os.path.isfile(binary):
        print(f"FATAL: binary not found: {binary}")
        sys.exit(1)
    if not os.path.isfile(config):
        print(f"FATAL: config not found: {config}")
        sys.exit(1)

    env = os.environ.copy()
    env["CHI_SERVER_CONF"] = config

    log_path = os.path.join(AGENT_INTERCEPTOR_DIR, "deploy", "server.log")
    log_file = open(log_path, "w")
    server_proc = subprocess.Popen(
        [binary],
        env=env,
        stdout=log_file,
        stderr=log_file,
    )
    atexit.register(kill_server)

    # Wait for proxy to become ready
    url = f"http://localhost:{proxy_port}/v1/messages"
    for attempt in range(30):
        try:
            req = urllib.request.Request(url, method="GET")
            with urllib.request.urlopen(req, timeout=2) as resp:
                if resp.status == 200:
                    print(f"  Server ready (attempt {attempt + 1})", flush=True)
                    return server_proc
        except Exception:
            pass
        # Check server hasn't died
        if server_proc.poll() is not None:
            print(f"FATAL: server exited with code {server_proc.returncode}")
            sys.exit(1)
        time.sleep(1)

    print("FATAL: server did not become ready in 30s")
    kill_server()
    sys.exit(1)


def kill_server():
    global server_proc
    if server_proc and server_proc.poll() is None:
        server_proc.send_signal(signal.SIGTERM)
        try:
            server_proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            server_proc.kill()
            server_proc.wait()
        server_proc = None


# ---------------------------------------------------------------------------
# Test helpers
# ---------------------------------------------------------------------------

def make_opts(port: int, session: str, **kwargs) -> ClaudeAgentOptions:
    return ClaudeAgentOptions(
        max_turns=kwargs.get("max_turns", 1),
        permission_mode="bypassPermissions",
        env={"ANTHROPIC_BASE_URL": f"http://localhost:{port}/_session/{session}"},
        **{k: v for k, v in kwargs.items() if k != "max_turns"},
    )


async def run_query(prompt: str, opts: ClaudeAgentOptions, timeout_sec: int = 60):
    """Run a single query, return (texts, result_msg, error_str)."""
    texts = []
    result = None
    try:
        with anyio.fail_after(timeout_sec):
            async for msg in query(prompt=prompt, options=opts):
                if isinstance(msg, AssistantMessage):
                    for block in msg.content:
                        if isinstance(block, TextBlock):
                            texts.append(block.text)
                elif isinstance(msg, ResultMessage):
                    result = msg
    except TimeoutError:
        return texts, result, f"timeout after {timeout_sec}s"
    except Exception as e:
        return texts, result, str(e)
    return texts, result, None


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

async def test_a_basic(port: int):
    """Test A: Single agent basic query."""
    print("\n=== Test A: Single agent basic query (session: inttest-basic) ===", flush=True)
    opts = make_opts(port, "inttest-basic")
    texts, result, err = await run_query(
        "What is 2+2? Reply only with the number.", opts
    )

    if err:
        report("Query completed without error", False, f"error={err}")
        return result

    got_text = len(texts) > 0
    report("Got AssistantMessage with text", got_text,
           f"texts={texts}" if got_text else "no text received")

    got_result = result is not None and not result.is_error
    report("Got successful ResultMessage", got_result,
           f"session_id={result.session_id}" if result else "no result")

    return result


async def test_b_multi_agent(port: int):
    """Test B: Multiple agents with distinct sessions."""
    print("\n=== Test B: Multiple agents, distinct sessions ===", flush=True)
    agents = [
        ("inttest-multi-0", "What is 2+2? Reply only with the number."),
        ("inttest-multi-1", "Name one planet. Reply in one word."),
        ("inttest-multi-2", "What color is the sky? Reply in one word."),
    ]

    for session, prompt in agents:
        opts = make_opts(port, session)
        texts, result, err = await run_query(prompt, opts)

        if err:
            report(f"Agent {session} completed", False, f"error={err}")
            continue

        ok = result is not None and not result.is_error and len(texts) > 0
        report(f"Agent {session} completed", ok,
               f"answer={texts[0][:50] if texts else 'none'}")


async def test_c_multiturn(port: int):
    """Test C: Multi-turn conversation with resume."""
    print("\n=== Test C: Multi-turn conversation (session: inttest-multiturn) ===", flush=True)

    # Turn 1: ask to remember 42
    opts1 = make_opts(port, "inttest-multiturn", max_turns=1)
    texts1, result1, err1 = await run_query(
        "Remember the number 42. Just confirm you will remember it.", opts1
    )

    if err1:
        report("Turn 1 completed", False, f"error={err1}")
        report("Turn 2 skipped", False, "Turn 1 failed")
        return

    got_turn1 = result1 is not None and not result1.is_error
    report("Turn 1 completed", got_turn1,
           f"session_id={result1.session_id}" if result1 else "no result")

    if not got_turn1:
        report("Turn 2 skipped", False, "Turn 1 failed")
        return

    # Turn 2: resume and ask what the number was
    opts2 = ClaudeAgentOptions(
        resume=result1.session_id,
        max_turns=1,
        permission_mode="bypassPermissions",
        env={"ANTHROPIC_BASE_URL": f"http://localhost:{port}/_session/inttest-multiturn"},
    )
    texts2, result2, err2 = await run_query(
        "What number did I ask you to remember? Reply only with the number.", opts2
    )

    if err2:
        report("Turn 2 completed", False, f"error={err2}")
        return

    got_turn2 = result2 is not None and not result2.is_error
    report("Turn 2 completed", got_turn2)

    has_42 = any("42" in t for t in texts2)
    report("Turn 2 references 42", has_42,
           f"answer={texts2[0][:80] if texts2 else 'none'}")


async def test_d_context_isolation(port: int):
    """Test D: Verify fresh queries do NOT carry over context (clear semantics)."""
    print("\n=== Test D: Context isolation (no resume = clear) ===", flush=True)

    # Step 1: tell an agent a secret word
    opts1 = make_opts(port, "inttest-isolation", max_turns=1)
    texts1, result1, err1 = await run_query(
        "The secret word is 'pineapple'. Just confirm you noted it.", opts1
    )

    if err1:
        report("Step 1 (set secret) completed", False, f"error={err1}")
        report("Step 2 skipped", False, "Step 1 failed")
        return

    got1 = result1 is not None and not result1.is_error
    report("Step 1 (set secret) completed", got1,
           f"session_id={result1.session_id}" if result1 else "no result")

    if not got1:
        report("Step 2 skipped", False, "Step 1 failed")
        return

    # Step 2: fresh query (NO resume, NO continue_conversation) — same session ID
    # The agent should NOT know the secret word.
    opts2 = make_opts(port, "inttest-isolation", max_turns=1)
    texts2, result2, err2 = await run_query(
        "What is the secret word? If you don't know, reply exactly: UNKNOWN", opts2
    )

    if err2:
        report("Step 2 (fresh query) completed", False, f"error={err2}")
        return

    got2 = result2 is not None and not result2.is_error
    report("Step 2 (fresh query) completed", got2)

    # The agent should NOT know 'pineapple' — context was cleared
    answer = " ".join(texts2).lower()
    knows_secret = "pineapple" in answer
    report("Context was cleared (no pineapple)", not knows_secret,
           f"answer={texts2[0][:80] if texts2 else 'none'}")


def test_e_tracker(build_dir: str):
    """Test E: Verify tracker state via ctx_writer."""
    print("\n=== Test E: Verify tracker state (ctx_writer) ===", flush=True)

    ctx_writer = os.path.join(build_dir, "bin", "ctx_writer")
    if not os.path.isfile(ctx_writer):
        report("ctx_writer binary exists", False, f"not found: {ctx_writer}")
        return

    try:
        result = subprocess.run(
            [
                ctx_writer,
                "--expected-sessions", "4",
                "--min-interactions-per-session", "1",
            ],
            capture_output=True,
            text=True,
            timeout=30,
        )

        print(result.stdout)
        if result.stderr:
            print(result.stderr)

        report("ctx_writer exit code 0", result.returncode == 0,
               f"exit={result.returncode}")
    except subprocess.TimeoutExpired:
        report("ctx_writer completed in time", False, "timed out after 30s")
    except Exception as e:
        report("ctx_writer ran successfully", False, f"error: {e}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

async def run_all(build_dir: str, port: int):
    await test_a_basic(port)
    await test_b_multi_agent(port)
    await test_c_multiturn(port)
    await test_d_context_isolation(port)
    test_e_tracker(build_dir)


def main():
    parser = argparse.ArgumentParser(description="DTProvenance integration test")
    parser.add_argument("--build-dir", required=True, help="Path to clio-core build dir")
    parser.add_argument("--proxy-port", type=int, default=9090, help="Proxy port")
    args = parser.parse_args()

    print("=" * 60, flush=True)
    print("DTProvenance Integration Test Suite", flush=True)
    print("=" * 60, flush=True)

    print("\n--- Starting server ---", flush=True)
    start_server(args.build_dir, args.proxy_port)

    anyio.run(run_all, args.build_dir, args.proxy_port)

    print("\n" + "=" * 60)
    print(f"Results: {passed} passed, {failed} failed")
    print("=" * 60)

    kill_server()
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
