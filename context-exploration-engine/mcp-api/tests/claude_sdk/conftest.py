"""
Pytest fixtures for Claude Agent SDK integration tests.

Uses the Claude Agent SDK (claude-agent-sdk) which authenticates via
OAuth token at ~/.claude (no API key needed — same auth as Claude Code CLI).

Set MCHIPS_GATEWAY_URL to point to a running MChiPs gateway.
"""

import os
import asyncio
from pathlib import Path

import pytest

# Mark for tests requiring the Claude Agent SDK
requires_claude = pytest.mark.skipif(
    not Path.home().joinpath(".claude").exists(),
    reason="~/.claude OAuth token not found",
)


@pytest.fixture
def gateway_url():
    """Gateway URL from environment or default."""
    return os.environ.get("MCHIPS_GATEWAY_URL", "http://localhost:8080/mcp")


@pytest.fixture
def claude_options(gateway_url):
    """Claude Agent SDK options with MChiPs MCP server configured."""
    try:
        from claude_agent_sdk import ClaudeAgentOptions
    except ImportError:
        pytest.skip("claude-agent-sdk not installed")

    if not Path.home().joinpath(".claude").exists():
        pytest.skip("~/.claude OAuth token not found")

    return ClaudeAgentOptions(
        mcp_servers={"mchips": {"url": gateway_url}},
    )


async def collect_messages(prompt, options):
    """Run a Claude Agent SDK query and collect all messages."""
    from claude_agent_sdk import query

    messages = []
    async for message in query(prompt=prompt, options=options):
        messages.append(message)
    return messages


@pytest.fixture
def claude_query(claude_options):
    """Returns a sync helper that queries Claude with MChiPs tools available."""

    def _query(prompt):
        return asyncio.run(collect_messages(prompt, claude_options))

    return _query
