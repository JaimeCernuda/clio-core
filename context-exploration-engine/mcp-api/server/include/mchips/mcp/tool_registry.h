/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_TOOL_REGISTRY_H_
#define MCHIPS_MCP_TOOL_REGISTRY_H_

#include <mchips/protocol/json_rpc.h>
#include <mchips/protocol/mcp_types.h>

#include <functional>
#include <string>
#include <vector>

namespace mchips::mcp {

/// Handler function signature for MCP tools.
/// Takes JSON arguments, returns JSON result.
using ToolHandler =
    std::function<protocol::CallToolResult(const protocol::json& args)>;

/// Registry for MCP tools — registration and name-based dispatch.
///
/// Tools are registered at server startup with a ToolDefinition (metadata)
/// and a ToolHandler (implementation). The registry handles:
///   - tools/list: returns all registered ToolDefinitions
///   - tools/call: looks up handler by name, invokes with args
///
/// Thread safety: registration happens at startup (single-threaded),
/// dispatch is read-only and safe for concurrent access.
class ToolRegistry {
 public:
  ToolRegistry() = default;

  // TODO(Phase 3): Implement registration and dispatch
  // void Register(const protocol::ToolDefinition& definition,
  //               ToolHandler handler);
  // protocol::CallToolResult Invoke(const std::string& name,
  //                                 const protocol::json& args);
  // std::vector<protocol::ToolDefinition> ListTools() const;
};

}  // namespace mchips::mcp

#endif  // MCHIPS_MCP_TOOL_REGISTRY_H_
