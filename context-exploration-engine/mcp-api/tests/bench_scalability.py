#!/usr/bin/env python3
"""
MChiPs Scalability Evaluation Suite

Compares IOWarp MChiPs MCP gateway against Python FastMCP and C++ standalone
MCP servers across two dimensions:

  1. Data volume: variable payload sizes (1KB → 10MB)
  2. Parallel access: concurrent client connections (1 → 100)

Usage:
  source .venv/bin/activate
  python tests/bench_scalability.py --iowarp http://ares-comp-29:8080/mcp \
                                     --python http://localhost:9099/mcp \
                                     --cpp    http://localhost:9098/mcp

Each server must implement echo(message) and add(a,b) tools.
"""

import argparse
import asyncio
import json
import statistics
import sys
import time

from mcp.client.streamable_http import streamablehttp_client
from mcp import ClientSession


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

async def create_session(url):
    """Create and initialize an MCP session."""
    read, write, _ = await streamablehttp_client(url).__aenter__()
    session = ClientSession(read, write)
    await session.__aenter__()
    await session.initialize()
    return session, read, write


async def timed_call(session, tool_name, args):
    """Call a tool and return (latency_ms, result)."""
    t0 = time.perf_counter()
    result = await session.call_tool(tool_name, args)
    t1 = time.perf_counter()
    return (t1 - t0) * 1000, result


def percentiles(latencies):
    """Compute p50, p95, p99, mean from sorted latencies."""
    s = sorted(latencies)
    n = len(s)
    return {
        "p50": s[n // 2],
        "p95": s[int(n * 0.95)],
        "p99": s[int(n * 0.99)],
        "mean": statistics.mean(s),
        "min": s[0],
        "max": s[-1],
    }


def detect_tool_prefix(tool_names):
    """Detect if tools have MChiPs prefix (demo__echo) or bare (echo)."""
    if "demo__echo" in tool_names:
        return "demo__"
    return ""


# ---------------------------------------------------------------------------
# Benchmark 1: Data Volume (payload size scaling)
# ---------------------------------------------------------------------------

async def bench_data_volume(url, label, sizes_kb, calls_per_size=100):
    """Measure latency as a function of payload size."""
    print(f"\n{'='*60}")
    print(f"DATA VOLUME BENCHMARK: {label}")
    print(f"  Server: {url}")
    print(f"  Calls per size: {calls_per_size}")
    print(f"{'='*60}")

    results = []
    try:
        async with streamablehttp_client(url) as (read, write, _):
            async with ClientSession(read, write) as session:
                await session.initialize()

                tool_list = await session.list_tools()
                prefix = detect_tool_prefix([t.name for t in tool_list.tools])
                echo_tool = f"{prefix}echo"

                for size_kb in sizes_kb:
                    payload = "x" * (size_kb * 1024)

                    # Warmup
                    for _ in range(3):
                        await session.call_tool(echo_tool, {"message": payload[:100]})

                    latencies = []
                    for _ in range(calls_per_size):
                        lat, _ = await timed_call(
                            session, echo_tool, {"message": payload})
                        latencies.append(lat)

                    stats = percentiles(latencies)
                    results.append({"size_kb": size_kb, **stats})

                    print(f"  {size_kb:>6} KB | "
                          f"p50: {stats['p50']:>8.2f} ms | "
                          f"p95: {stats['p95']:>8.2f} ms | "
                          f"mean: {stats['mean']:>8.2f} ms")

    except Exception as e:
        print(f"  ERROR: {e}")
        return []

    return results


# ---------------------------------------------------------------------------
# Benchmark 2: Parallel Access (concurrent connections)
# ---------------------------------------------------------------------------

async def single_client_burst(url, tool_name, args, num_calls):
    """Run num_calls sequential tool calls on a single connection."""
    latencies = []
    try:
        async with streamablehttp_client(url) as (read, write, _):
            async with ClientSession(read, write) as session:
                await session.initialize()
                for _ in range(num_calls):
                    lat, _ = await timed_call(session, tool_name, args)
                    latencies.append(lat)
    except Exception as e:
        latencies.append(-1)  # Error marker
    return latencies


async def bench_parallel_access(url, label, concurrencies,
                                 calls_per_client=50):
    """Measure throughput and latency under concurrent load."""
    print(f"\n{'='*60}")
    print(f"PARALLEL ACCESS BENCHMARK: {label}")
    print(f"  Server: {url}")
    print(f"  Calls per client: {calls_per_client}")
    print(f"{'='*60}")

    results = []

    # Detect tool prefix
    try:
        async with streamablehttp_client(url) as (read, write, _):
            async with ClientSession(read, write) as session:
                await session.initialize()
                tool_list = await session.list_tools()
                prefix = detect_tool_prefix([t.name for t in tool_list.tools])
    except Exception as e:
        print(f"  ERROR connecting: {e}")
        return []

    add_tool = f"{prefix}add"
    args = {"a": 17, "b": 25}

    for num_clients in concurrencies:
        t_start = time.perf_counter()
        tasks = [
            single_client_burst(url, add_tool, args, calls_per_client)
            for _ in range(num_clients)
        ]
        all_latencies_nested = await asyncio.gather(*tasks)
        t_end = time.perf_counter()

        all_latencies = [
            lat for lats in all_latencies_nested for lat in lats if lat > 0
        ]
        total_time = (t_end - t_start) * 1000
        total_calls = num_clients * calls_per_client
        successful = len(all_latencies)
        errors = total_calls - successful

        if all_latencies:
            stats = percentiles(all_latencies)
            throughput = successful / (total_time / 1000)
        else:
            stats = {"p50": 0, "p95": 0, "p99": 0, "mean": 0,
                     "min": 0, "max": 0}
            throughput = 0

        results.append({
            "clients": num_clients,
            "total_calls": total_calls,
            "successful": successful,
            "errors": errors,
            "throughput_rps": throughput,
            "wall_time_ms": total_time,
            **stats,
        })

        print(f"  {num_clients:>4} clients | "
              f"calls: {successful:>5}/{total_calls:>5} | "
              f"throughput: {throughput:>7.0f} req/s | "
              f"p50: {stats['p50']:>7.2f} ms | "
              f"p95: {stats['p95']:>7.2f} ms | "
              f"wall: {total_time:>8.0f} ms")

    return results


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

async def run_benchmarks(servers, data_sizes_kb, concurrencies,
                          data_calls, parallel_calls):
    """Run all benchmarks across all servers."""
    all_results = {}

    for label, url in servers.items():
        if not url:
            continue

        print(f"\n{'#'*60}")
        print(f"# SERVER: {label} ({url})")
        print(f"{'#'*60}")

        # Data volume benchmark
        data_results = await bench_data_volume(
            url, label, data_sizes_kb, data_calls)

        # Parallel access benchmark
        parallel_results = await bench_parallel_access(
            url, label, concurrencies, parallel_calls)

        all_results[label] = {
            "url": url,
            "data_volume": data_results,
            "parallel_access": parallel_results,
        }

    return all_results


def print_comparison(all_results, metric="p50"):
    """Print a side-by-side comparison table."""
    servers = list(all_results.keys())
    if not servers:
        return

    # Data volume comparison
    print(f"\n{'='*80}")
    print("COMPARISON: Data Volume (p50 latency in ms)")
    print(f"{'='*80}")

    header = f"{'Size':>8}"
    for s in servers:
        header += f" | {s:>15}"
    print(header)
    print("-" * len(header))

    first = all_results[servers[0]].get("data_volume", [])
    for i, entry in enumerate(first):
        row = f"{entry['size_kb']:>6}KB"
        for s in servers:
            data = all_results[s].get("data_volume", [])
            if i < len(data):
                row += f" | {data[i][metric]:>14.2f}"
            else:
                row += f" | {'N/A':>14}"
        print(row)

    # Parallel access comparison
    print(f"\n{'='*80}")
    print("COMPARISON: Parallel Access (throughput in req/s)")
    print(f"{'='*80}")

    header = f"{'Clients':>8}"
    for s in servers:
        header += f" | {s:>15}"
    print(header)
    print("-" * len(header))

    first = all_results[servers[0]].get("parallel_access", [])
    for i, entry in enumerate(first):
        row = f"{entry['clients']:>8}"
        for s in servers:
            data = all_results[s].get("parallel_access", [])
            if i < len(data):
                row += f" | {data[i]['throughput_rps']:>14.0f}"
            else:
                row += f" | {'N/A':>14}"
        print(row)


def main():
    parser = argparse.ArgumentParser(
        description="MChiPs Scalability Evaluation Suite")
    parser.add_argument("--iowarp", type=str, default=None,
                        help="IOWarp MChiPs gateway URL")
    parser.add_argument("--python", type=str, default=None,
                        help="Python FastMCP server URL")
    parser.add_argument("--cpp", type=str, default=None,
                        help="C++ standalone MCP server URL")
    parser.add_argument("--data-sizes", type=str, default="1,10,100,1024,5120,10240",
                        help="Comma-separated payload sizes in KB")
    parser.add_argument("--concurrencies", type=str, default="1,5,10,25,50,100",
                        help="Comma-separated client counts")
    parser.add_argument("--data-calls", type=int, default=50,
                        help="Calls per data size test")
    parser.add_argument("--parallel-calls", type=int, default=50,
                        help="Calls per client in parallel test")
    parser.add_argument("--output", type=str, default=None,
                        help="Save results as JSON to this file")
    args = parser.parse_args()

    servers = {}
    if args.iowarp:
        servers["IOWarp"] = args.iowarp
    if args.python:
        servers["Python"] = args.python
    if args.cpp:
        servers["C++"] = args.cpp

    if not servers:
        print("No servers specified. Use --iowarp, --python, and/or --cpp.")
        sys.exit(1)

    data_sizes = [int(x) for x in args.data_sizes.split(",")]
    concurrencies = [int(x) for x in args.concurrencies.split(",")]

    all_results = asyncio.run(run_benchmarks(
        servers, data_sizes, concurrencies,
        args.data_calls, args.parallel_calls))

    print_comparison(all_results)

    if args.output:
        with open(args.output, "w") as f:
            json.dump(all_results, f, indent=2)
        print(f"\nResults saved to {args.output}")


if __name__ == "__main__":
    main()
