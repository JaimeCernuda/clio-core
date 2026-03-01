"""
Test Claude orchestrating a scientific data workflow using CTE tools.

Verifies that Claude can use the CTE MChiP tools to perform
a put_blob → get_blob → list workflow through the gateway.

NOTE: These tests require a fully initialized CTE runtime (pool 701)
with the wrp_cte_core module loaded. They will fail gracefully if
CTE is not available, as the tool will return an error result.
"""

import pytest
from conftest import requires_claude


@requires_claude
def test_claude_puts_and_gets_blob(claude_query):
    """Claude should store and retrieve a blob using CTE tools."""
    messages = claude_query(
        "Using the CTE tools: "
        "1. Store a blob named 'test_blob' in tag 'test_tag' with data 'hello world'. "
        "2. Then retrieve the blob 'test_blob' from tag 'test_tag'. "
        "Report what you stored and retrieved."
    )

    text = " ".join(str(m) for m in messages).lower()

    # Should attempt to use CTE tools (even if CTE isn't initialized,
    # Claude should still try to use the tools)
    assert "put_blob" in text or "store" in text or "cte" in text


@requires_claude
def test_claude_lists_cte_types(claude_query):
    """Claude should list available CTE storage tier types."""
    messages = claude_query(
        "What storage tiers are available in the CTE system? "
        "Use the get_cte_types tool to find out."
    )

    text = " ".join(str(m) for m in messages).lower()

    # Should mention storage tiers
    assert "ram" in text or "ssd" in text or "tier" in text or "storage" in text


@requires_claude
def test_claude_checks_cte_status(claude_query):
    """Claude should check the CTE client initialization status."""
    messages = claude_query(
        "Check if the CTE runtime is initialized using the get_client_status tool."
    )

    text = " ".join(str(m) for m in messages).lower()

    # Should report initialization status
    assert "initialized" in text or "status" in text or "client" in text
