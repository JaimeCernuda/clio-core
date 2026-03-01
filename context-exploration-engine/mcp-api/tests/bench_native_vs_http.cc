/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Benchmark: Native IPC client vs HTTP client latency.
 *
 * Measures p50/p95/p99 for 1000 sequential demo__add calls through:
 *   1. NativeClient (Chimaera IPC → Gateway → Demo MChiP)
 *   2. McpClient (HTTP POST → Gateway → Demo MChiP)
 *
 * Usage:
 *   ./bench_native_vs_http [gateway_url]
 *   Default gateway_url: http://localhost:8080/mcp
 *
 * Requires a running Chimaera server with gateway + demo pools,
 * and the HTTP server started on the specified URL.
 */

#include <mchips/client/native_client.h>
#include <mchips/client/mcp_client.h>
#include <mchips/protocol/mcp_types.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using json = nlohmann::ordered_json;
using Clock = std::chrono::high_resolution_clock;

struct BenchResult {
  double p50_us;
  double p95_us;
  double p99_us;
  double mean_us;
};

BenchResult ComputeStats(std::vector<double>& latencies) {
  std::sort(latencies.begin(), latencies.end());
  size_t n = latencies.size();

  double sum = 0;
  for (double l : latencies) sum += l;

  return {
      latencies[n / 2],
      latencies[static_cast<size_t>(n * 0.95)],
      latencies[static_cast<size_t>(n * 0.99)],
      sum / static_cast<double>(n)
  };
}

void PrintResult(const std::string& label, const BenchResult& r, int calls) {
  std::cout << "\n=== " << label << " (" << calls << " calls) ===" << std::endl;
  std::cout << "  p50:  " << r.p50_us / 1000.0 << " ms" << std::endl;
  std::cout << "  p95:  " << r.p95_us / 1000.0 << " ms" << std::endl;
  std::cout << "  p99:  " << r.p99_us / 1000.0 << " ms" << std::endl;
  std::cout << "  mean: " << r.mean_us / 1000.0 << " ms" << std::endl;
}

int main(int argc, char* argv[]) {
  std::string gateway_url = "http://localhost:8080/mcp";
  int num_calls = 1000;
  if (argc > 1) gateway_url = argv[1];
  if (argc > 2) num_calls = std::atoi(argv[2]);

  json args = {{"a", 17}, {"b", 25}};

  // --- Native client benchmark ---
  std::cout << "Benchmarking native client..." << std::endl;
  mchips::client::NativeClient::Init();
  {
    mchips::client::NativeClient native;
    native.Connect();
    native.Initialize();

    // Warmup
    for (int i = 0; i < 10; ++i) {
      native.CallTool("demo__add", args);
    }

    std::vector<double> native_latencies;
    native_latencies.reserve(num_calls);

    for (int i = 0; i < num_calls; ++i) {
      auto start = Clock::now();
      native.CallTool("demo__add", args);
      auto end = Clock::now();
      double us = std::chrono::duration<double, std::micro>(end - start).count();
      native_latencies.push_back(us);
    }

    auto native_result = ComputeStats(native_latencies);
    PrintResult("Native Client (Chimaera IPC)", native_result, num_calls);
  }
  mchips::client::NativeClient::Finalize();

  // --- HTTP client benchmark ---
  std::cout << "\nBenchmarking HTTP client..." << std::endl;
  {
    mchips::client::McpClient http_client({gateway_url});
    http_client.Initialize();

    // Warmup
    for (int i = 0; i < 10; ++i) {
      http_client.CallTool("demo__add", args);
    }

    std::vector<double> http_latencies;
    http_latencies.reserve(num_calls);

    for (int i = 0; i < num_calls; ++i) {
      auto start = Clock::now();
      http_client.CallTool("demo__add", args);
      auto end = Clock::now();
      double us = std::chrono::duration<double, std::micro>(end - start).count();
      http_latencies.push_back(us);
    }

    auto http_result = ComputeStats(http_latencies);
    PrintResult("HTTP Client (httplib)", http_result, num_calls);
  }

  std::cout << "\nDone." << std::endl;
  return 0;
}
