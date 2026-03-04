# /// script
# requires-python = ">=3.10"
# dependencies = ["claude-agent-sdk"]
# ///
"""Simple test: ask Claude 2+2 via claude_agent_sdk."""

import os

import anyio
from claude_agent_sdk import query, ClaudeAgentOptions, AssistantMessage, TextBlock

os.environ.pop("CLAUDECODE", None)


async def main():
    async for msg in query(
        prompt="What is 2+2? Reply only with the number.",
        options=ClaudeAgentOptions(max_turns=1),
    ):
        if isinstance(msg, AssistantMessage):
            for block in msg.content:
                if isinstance(block, TextBlock):
                    print(f"Answer: {block.text}")
        else:
            print(f"{type(msg).__name__}: {msg}")


anyio.run(main)
