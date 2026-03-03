"""Run Claude agents through the DTProvenance proxy.

Each agent gets a unique session ID and prompt.
Agents run sequentially (parallel would fight over env vars).

Usage:
    uv run python deploy/run_agents.py \
        --proxy-host hostname \
        --proxy-port 9090 \
        --sessions agent-0 agent-1 agent-2 \
        --prompts "What is 2+2?" "Name the planets" "Write a haiku"
"""

import argparse
import asyncio
import os
import sys

import claude_agent_sdk


async def run_single_agent(
    proxy_url: str, session_id: str, prompt: str
) -> None:
    """Run one Claude agent through the DTProvenance proxy."""
    # Set the base URL so the agent routes through our proxy
    env_url = f"{proxy_url}/_session/{session_id}"
    os.environ["ANTHROPIC_BASE_URL"] = env_url

    print(f"  [{session_id}] Starting: {prompt[:50]}...")
    print(f"  [{session_id}] ANTHROPIC_BASE_URL={env_url}")

    try:
        async for msg in claude_agent_sdk.query(
            prompt=prompt,
            options=claude_agent_sdk.ClaudeAgentOptions(
                model="claude-sonnet-4-6",
                permission_mode="bypassPermissions",
                max_turns=3,
            ),
        ):
            msg_type = type(msg).__name__
            if msg_type == "ResultMessage":
                print(f"  [{session_id}] Complete")
            elif msg_type == "AssistantMessage":
                # Truncate long messages
                text = str(msg)[:100]
                print(f"  [{session_id}] Assistant: {text}...")
    except Exception as e:
        print(f"  [{session_id}] Error: {e}", file=sys.stderr)
        raise


async def main() -> None:
    """Parse arguments and run agents sequentially."""
    parser = argparse.ArgumentParser(description="Run Claude agents through DTProvenance proxy")
    parser.add_argument("--proxy-host", required=True, help="Proxy hostname")
    parser.add_argument("--proxy-port", type=int, required=True, help="Proxy port")
    parser.add_argument("--sessions", nargs="+", required=True, help="Session IDs")
    parser.add_argument("--prompts", nargs="+", required=True, help="Prompts for each agent")
    args = parser.parse_args()

    if len(args.sessions) != len(args.prompts):
        print("Error: Number of sessions must match number of prompts", file=sys.stderr)
        sys.exit(1)

    proxy_url = f"http://{args.proxy_host}:{args.proxy_port}"
    print(f"Proxy URL: {proxy_url}")
    print(f"Running {len(args.sessions)} agents...")

    # Run agents sequentially (parallel would fight over ANTHROPIC_BASE_URL env var)
    for session_id, prompt in zip(args.sessions, args.prompts):
        await run_single_agent(proxy_url, session_id, prompt)

    print("All agents completed.")


if __name__ == "__main__":
    asyncio.run(main())
