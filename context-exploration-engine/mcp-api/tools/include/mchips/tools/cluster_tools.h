/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_TOOLS_CLUSTER_TOOLS_H_
#define MCHIPS_TOOLS_CLUSTER_TOOLS_H_

#include <mchips/protocol/mcp_types.h>

#include <vector>

namespace mchips::tools {

/// Get ToolDefinitions for cluster monitoring tools.
///
/// Cluster tools expose IOWarp runtime diagnostics:
///   cluster_status  — node health, pool stats, worker utilization
///   list_pools      — enumerate active ChiMod pools
///   pool_stats      — per-pool task throughput and latency
std::vector<protocol::ToolDefinition> GetClusterToolDefinitions();

// TODO(Phase 4): Handler functions
// protocol::CallToolResult HandleClusterStatus(const protocol::json& args);
// protocol::CallToolResult HandleListPools(const protocol::json& args);
// protocol::CallToolResult HandlePoolStats(const protocol::json& args);

}  // namespace mchips::tools

#endif  // MCHIPS_TOOLS_CLUSTER_TOOLS_H_
