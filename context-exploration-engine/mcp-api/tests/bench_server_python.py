#!/usr/bin/env python3
"""Python FastMCP benchmark server.

Implements echo(message) and add(a,b) tools for scalability comparison
against the IOWarp MChiPs gateway.

Usage:
  python tests/bench_server_python.py [--port 9099] [--host 0.0.0.0]
"""

import argparse

from fastmcp import FastMCP

mcp = FastMCP("bench-python", version="1.0.0")


@mcp.tool()
def echo(message: str) -> str:
    """Echo back the given message."""
    return message


@mcp.tool()
def add(a: float, b: float) -> float:
    """Add two numbers and return the result."""
    return a + b


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Python MCP benchmark server")
    parser.add_argument("--port", type=int, default=9099)
    parser.add_argument("--host", type=str, default="0.0.0.0")
    args = parser.parse_args()

    print(f"Starting Python FastMCP benchmark server on {args.host}:{args.port}")
    mcp.run(transport="streamable-http", host=args.host, port=args.port)
