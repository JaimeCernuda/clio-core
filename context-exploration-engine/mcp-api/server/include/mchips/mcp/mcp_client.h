/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_MCP_CLIENT_H_
#define MCHIPS_MCP_MCP_CLIENT_H_

// TODO(Phase 3): Include Chimaera client base
// #include <chimaera/chimaera.h>
// #include <mchips/mcp/mcp_tasks.h>

namespace mchips::mcp {

/// ChiMod client for the MCP server.
///
/// Provides async methods for submitting MCP tasks to the Chimaera runtime.
/// Follows the ContainerClient pattern from wrp_cte::core::Client.
///
/// Usage:
///   auto future = WRP_MCP_CLIENT->AsyncHandleRequest(pool_query, raw_json);
///   future.Wait();
///   auto response = future.Get();
class Client {
 public:
  Client() = default;

  // TODO(Phase 3): Implement async methods
  // chi::Future<HandleHttpRequestTask> AsyncHandleRequest(
  //     const chi::PoolQuery& pool_query, const std::string& raw_json);
};

}  // namespace mchips::mcp

#endif  // MCHIPS_MCP_MCP_CLIENT_H_
