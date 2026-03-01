"""
Test Claude's agentic loop with MChiPs tools.

Verifies that Claude can autonomously discover and use tools
to accomplish tasks. The Claude Agent SDK handles the full loop:
  query() → Claude sees tools → tool_use(demo__add) → tool_result → answer
"""

import pytest
from conftest import requires_claude


@requires_claude
def test_claude_calls_demo_add(claude_query):
    """Claude should discover demo__add and use it to compute 17+25=42."""
    messages = claude_query("Add 17 and 25 using the add tool.")

    # The Claude Agent SDK handles the full agentic loop
    text = " ".join(str(m) for m in messages)
    assert "42" in text


@requires_claude
def test_claude_calls_demo_echo(claude_query):
    """Claude should use demo__echo to echo a message back."""
    messages = claude_query(
        'Use the echo tool to echo the message "hello from claude".'
    )

    text = " ".join(str(m) for m in messages)
    assert "hello from claude" in text.lower()


@requires_claude
def test_claude_handles_multiple_operations(claude_query):
    """Claude should handle a multi-step task using tools."""
    messages = claude_query(
        "First add 10 and 20 using the add tool, "
        "then echo the result using the echo tool."
    )

    text = " ".join(str(m) for m in messages)
    assert "30" in text
