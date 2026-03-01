/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_GATEWAY_MCHIP_ROUTER_H_
#define MCHIPS_MCP_GATEWAY_MCHIP_ROUTER_H_

#include <mchips/sdk/mchip_client.h>
#include <mchips/protocol/mcp_types.h>

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mchips::mcp_gateway {

/// Tool naming convention: "mchip_name__tool_name" (double underscore).
///
/// This matches the emerging ecosystem standard used by MetaMCP,
/// Docker MCP Gateway, and MCPJungle.
static constexpr const char* kToolSeparator = "__";

/// Routing entry for a single MChiP ChiMod.
struct MchipRoute {
  std::string name;            ///< MChiP name (e.g., "cte")
  chi::PoolId pool_id;         ///< Chimaera pool ID for this MChiP
  sdk::MchipClient client;     ///< Client for sending tasks to this MChiP
  std::vector<protocol::ToolDefinition> tools;  ///< Cached tool definitions
};

/// Discovers MChiP ChiMods, aggregates their tools, and routes requests.
///
/// The router maintains a mapping from qualified tool names
/// (e.g., "cte__put_blob") to MChiP pool IDs. When a tools/call
/// request arrives, it splits on "__", looks up the target MChiP,
/// and dispatches via MchipClient::AsyncCallMcpTool.
///
/// Discovery: MChiPs are registered at gateway startup via the compose
/// config (chimaera_default.yaml). The gateway queries each MChiP's
/// kListMcpTools to populate the routing table.
class MchipRouter {
 public:
  MchipRouter() = default;

  /// Register a MChiP and cache its tools.
  ///
  /// Called during gateway Create() for each configured MChiP pool.
  /// Sends a ListMcpTools request to populate the routing table.
  ///
  /// @param name    MChiP name prefix (e.g., "cte", "cae", "cluster")
  /// @param pool_id Chimaera pool ID for the MChiP
  void RegisterMchip(const std::string& name, const chi::PoolId& pool_id) {
    MchipRoute route;
    route.name = name;
    route.pool_id = pool_id;
    route.client = sdk::MchipClient(pool_id);
    routes_[name] = std::move(route);
  }

  /// Refresh tool lists from all registered MChiPs.
  ///
  /// Sends ListMcpTools to each MChiP and updates the cached tool table.
  /// Blocks until all MChiPs respond. Called from the gateway Create() coroutine.
  void RefreshTools() {
    for (auto& [name, route] : routes_) {
      try {
        auto future = route.client.AsyncListMcpTools(chi::PoolQuery::Local());
        future.Wait();
        auto json_str = future->result_json_.str();
        auto tools_json = protocol::json::parse(json_str);
        route.tools.clear();
        for (const auto& tool_json : tools_json) {
          route.tools.push_back(protocol::ToolDefinition::FromJson(tool_json));
        }
      } catch (const std::exception&) {
        // Leave tools empty if MChiP doesn't respond
      }
    }
  }

  /// Route a qualified tool name to the right MChiP.
  ///
  /// Splits "cte__put_blob" into mchip="cte", tool="put_blob".
  /// Returns nullptr if the MChiP prefix is not found.
  ///
  /// @param qualified_name Full tool name with MChiP prefix
  /// @param out_tool_name  Output: local tool name (without prefix)
  /// @return Pointer to the MchipRoute, or nullptr
  MchipRoute* Route(const std::string& qualified_name,
                    std::string& out_tool_name) {
    auto sep_pos = qualified_name.find(kToolSeparator);
    if (sep_pos == std::string::npos) {
      return nullptr;
    }

    auto mchip_name = qualified_name.substr(0, sep_pos);
    out_tool_name = qualified_name.substr(sep_pos + 2);  // skip "__"

    auto it = routes_.find(mchip_name);
    if (it == routes_.end()) {
      return nullptr;
    }
    return &it->second;
  }

  /// Return the aggregated tool list with qualified names.
  ///
  /// Each tool name is prefixed: "mchip_name__original_tool_name".
  /// Used to service tools/list MCP requests.
  std::vector<protocol::ToolDefinition> ListAllTools() const {
    std::vector<protocol::ToolDefinition> all_tools;
    for (const auto& [mchip_name, route] : routes_) {
      for (const auto& tool : route.tools) {
        protocol::ToolDefinition qualified = tool;
        qualified.name = mchip_name + kToolSeparator + tool.name;
        all_tools.push_back(std::move(qualified));
      }
    }
    return all_tools;
  }

  /// Check whether any MChiPs are registered.
  bool HasRoutes() const { return !routes_.empty(); }

  /// Get the number of registered MChiPs.
  size_t NumMchips() const { return routes_.size(); }

  /// Access the routes map (for co_await iteration in coroutines).
  std::unordered_map<std::string, MchipRoute>& GetRoutes() { return routes_; }

  /// Const access to the routes map.
  const std::unordered_map<std::string, MchipRoute>& GetRoutes() const {
    return routes_;
  }

  /// Register a MChiP route directly (for unit testing without Chimaera).
  ///
  /// Allows populating the router with pre-built routes, bypassing
  /// the Chimaera client initialization in RegisterMchip().
  void RegisterMchipDirect(MchipRoute route) {
    auto name = route.name;
    routes_[name] = std::move(route);
  }

 private:
  std::unordered_map<std::string, MchipRoute> routes_;
};

}  // namespace mchips::mcp_gateway

#endif  // MCHIPS_MCP_GATEWAY_MCHIP_ROUTER_H_
