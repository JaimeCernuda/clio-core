"""
Test that Claude can discover MChiPs tools through the gateway.

Verifies that the MCP tool listing works correctly and Claude
can see and describe the available tools.
"""

import pytest
from conftest import requires_claude


@requires_claude
def test_claude_lists_tools(claude_query):
    """Claude should be able to list available MChiPs tools."""
    messages = claude_query(
        "List all available tools from the mchips server. "
        "Just list their names, nothing else."
    )
    text = " ".join(str(m) for m in messages)

    # Should find at least the demo tools
    assert "demo__echo" in text or "echo" in text
    assert "demo__add" in text or "add" in text


@requires_claude
def test_claude_describes_add_tool(claude_query):
    """Claude should describe what the demo__add tool does."""
    messages = claude_query(
        "Describe what the demo__add tool does. Be brief."
    )
    text = " ".join(str(m) for m in messages).lower()

    # Should mention adding/summing numbers
    assert "add" in text or "sum" in text or "number" in text
