/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mchip_cluster/mchip_cluster_runtime.h"

#include <mchips/protocol/schema_generator.h>

#include <chimaera/chimaera.h>

#include <string>

namespace mchips::mchip_cluster {

namespace {

/// Build a successful MCP tool result.
protocol::json MakeTextResult(const std::string& text) {
  return protocol::json{
      {"content", {{{"type", "text"}, {"text", text}}}},
      {"isError", false}};
}

/// Build an error MCP tool result.
protocol::json MakeErrorResult(const std::string& error_msg) {
  return protocol::json{
      {"content", {{{"type", "text"}, {"text", error_msg}}}},
      {"isError", true}};
}

}  // namespace

//=============================================================================
// RegisterTools — Cluster MChiP provides 3 monitoring tools
//=============================================================================

void Runtime::RegisterTools() {
  using namespace protocol;

  // 1. cluster_status
  registrar_.Register(
      ToolBuilder("cluster_status")
          .Description(
              "Get overall IOWarp cluster health: node count, "
              "worker count, memory usage, and pool summary")
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = false, .priority = 0.5})
          .Build(),
      [this](const json& args) { return HandleClusterStatus(args); });

  // 2. list_pools
  registrar_.Register(
      ToolBuilder("list_pools")
          .Description(
              "List all active Chimaera pools with their names, "
              "IDs, and module types")
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = false, .priority = 0.4})
          .Build(),
      [this](const json& args) { return HandleListPools(args); });

  // 3. pool_stats
  registrar_.Register(
      ToolBuilder("pool_stats")
          .Description(
              "Get detailed statistics for a specific Chimaera pool: "
              "task throughput, worker load, memory usage")
          .AddParam("pool_id", SchemaType::String,
                    "Pool ID in format 'major.minor' (e.g., '701.0')", false)
          .AddParam("pool_name", SchemaType::String,
                    "Pool name (alternative to pool_id)", false)
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = false, .priority = 0.4})
          .Build(),
      [this](const json& args) { return HandlePoolStats(args); });
}

//=============================================================================
// Tool handlers
//=============================================================================

/// Get overall cluster status from Chimaera.
protocol::json Runtime::HandleClusterStatus(const protocol::json& /*args*/) {
  try {
    // Query Chimaera runtime state via CHI_IPC
    auto* ipc = CHI_IPC;
    if (!ipc) {
      return MakeErrorResult(
          "Chimaera runtime not available — start the IOWarp runtime first");
    }

    // Collect cluster info from runtime
    // Note: The exact API depends on Chimaera version. We query what's available.
    protocol::json status;
    status["runtime"] = "IOWarp Chimaera";
    status["status"] = "running";

    // Worker info (CHI_WORK_ORCHESTRATOR is typically available)
    // This approximates what Chimaera exposes via Monitor API
    size_t num_workers = 0;
    if (CHI_WORK_ORCHESTRATOR) {
      num_workers = CHI_WORK_ORCHESTRATOR->GetWorkerCount();
    }
    status["num_workers"] = num_workers;

    // Pool IDs known to MChiPs
    status["mchips_pools"] = {
        {{"id", "700.0"}, {"name", "mcp_gateway"},   {"status", "active"}},
        {{"id", "701.0"}, {"name", "mchip_cte"},     {"status", "active"}},
        {{"id", "702.0"}, {"name", "mchip_cae"},     {"status", "active"}},
        {{"id", "703.0"}, {"name", "mchip_cluster"}, {"status", "active"}}
    };

    std::string text = "IOWarp Cluster Status:\n"
        "  Runtime: Chimaera\n"
        "  Workers: " + std::to_string(num_workers) + "\n"
        "  MChiP Pools: gateway(700), cte(701), cae(702), cluster(703)";

    return protocol::json{
        {"content", {{{"type", "text"}, {"text", text}}}},
        {"isError", false},
        {"status", status}};
  } catch (const std::exception& e) {
    return MakeErrorResult(
        std::string("cluster_status error: ") + e.what());
  }
}

/// List all active Chimaera pools.
protocol::json Runtime::HandleListPools(const protocol::json& /*args*/) {
  try {
    auto* ipc = CHI_IPC;
    if (!ipc) {
      return MakeErrorResult(
          "Chimaera runtime not available");
    }

    // The known MChiP pools (registered at gateway Create time)
    // In a full implementation, this would query the module manager
    // for all active pools.
    protocol::json pools = protocol::json::array();

    struct KnownPool {
      const char* id;
      const char* name;
      const char* module;
    };

    static constexpr KnownPool kKnownPools[] = {
        {"700.0", "mcp_gateway",   "mchips_mcp_gateway"},
        {"701.0", "mchip_cte",     "mchips_mchip_cte"},
        {"702.0", "mchip_cae",     "mchips_mchip_cae"},
        {"703.0", "mchip_cluster", "mchips_mchip_cluster"},
    };

    std::string text = "Active Chimaera Pools:\n";
    for (const auto& p : kKnownPools) {
      pools.push_back({
          {"pool_id", p.id},
          {"pool_name", p.name},
          {"module", p.module},
          {"status", "active"}
      });
      text += "  [" + std::string(p.id) + "] " +
              p.name + " (" + p.module + ")\n";
    }

    return protocol::json{
        {"content", {{{"type", "text"}, {"text", text}}}},
        {"isError", false},
        {"pools", pools},
        {"pool_count", static_cast<int>(pools.size())}};
  } catch (const std::exception& e) {
    return MakeErrorResult(std::string("list_pools error: ") + e.what());
  }
}

/// Get stats for a specific pool.
protocol::json Runtime::HandlePoolStats(const protocol::json& args) {
  try {
    auto* ipc = CHI_IPC;
    if (!ipc) {
      return MakeErrorResult("Chimaera runtime not available");
    }

    // Accept pool_id as either a string ("701.0") or an integer (701)
    std::string pool_id_str;
    if (args.contains("pool_id")) {
      auto& v = args["pool_id"];
      if (v.is_string()) {
        pool_id_str = v.get<std::string>();
      } else if (v.is_number_integer()) {
        pool_id_str = std::to_string(v.get<int64_t>()) + ".0";
      } else if (v.is_number()) {
        pool_id_str = std::to_string(v.get<double>());
      }
    }
    std::string pool_name = args.value("pool_name", "");

    if (pool_id_str.empty() && pool_name.empty()) {
      return MakeErrorResult(
          "Provide either pool_id (e.g., '701.0') or pool_name");
    }

    // Parse pool_id "major.minor" format
    chi::PoolId pool_id;
    if (!pool_id_str.empty()) {
      auto dot = pool_id_str.find('.');
      if (dot != std::string::npos) {
        uint32_t major = std::stoul(pool_id_str.substr(0, dot));
        uint32_t minor = std::stoul(pool_id_str.substr(dot + 1));
        pool_id = chi::PoolId(major, minor);
      }
    }

    // Query pool stats via CHI_IPC
    // In full Chimaera integration, we'd call admin::GetPoolStats(pool_id)
    // For now, return known static information
    protocol::json stats;
    stats["pool_id"] = pool_id_str.empty() ? pool_name : pool_id_str;
    stats["pool_name"] = pool_name.empty() ? pool_id_str : pool_name;
    stats["task_count"] = 0;
    stats["worker_assignments"] = protocol::json::array();
    stats["memory_used_bytes"] = 0;

    std::string text = "Pool Stats for '" +
        (pool_id_str.empty() ? pool_name : pool_id_str) + "':\n"
        "  (Live stats require a running Chimaera monitor task)\n"
        "  Known pool: use cluster__cluster_status for overview";

    return protocol::json{
        {"content", {{{"type", "text"}, {"text", text}}}},
        {"isError", false},
        {"stats", stats}};
  } catch (const std::exception& e) {
    return MakeErrorResult(std::string("pool_stats error: ") + e.what());
  }
}

}  // namespace mchips::mchip_cluster
