/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_SDK_TOOL_REGISTRAR_H_
#define MCHIPS_SDK_TOOL_REGISTRAR_H_

#include <mchips/protocol/mcp_types.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mchips::sdk {

/// Callback signature for tool execution.
///
/// Takes a JSON object of arguments and returns a JSON object conforming
/// to CallToolResult (content array + optional isError).
using ToolHandler = std::function<protocol::json(const protocol::json& args)>;

/// Entry in the tool registrar: definition metadata + handler function.
struct ToolEntry {
  protocol::ToolDefinition definition;
  ToolHandler handler;
};

/// Manages tool registration and dispatch for a single MChiP ChiMod.
///
/// Each MChiP creates a ToolRegistrar in its Create() handler,
/// registers tools with their definitions and handlers, then uses
/// it to service ListMcpTools and CallMcpTool requests from the gateway.
///
/// Example:
/// ```cpp
/// registrar_.Register(
///     ToolBuilder("put_blob")
///         .Description("Store a blob in CTE")
///         .AddParam("tag_name", SchemaType::String, "Tag namespace", true)
///         .Build(),
///     [this](const json& args) { return HandlePutBlob(args); }
/// );
/// ```
class ToolRegistrar {
 public:
  ToolRegistrar() = default;

  /// Register a tool with its definition and handler.
  ///
  /// @param definition MCP ToolDefinition (name, schema, annotations)
  /// @param handler    Function to execute when the tool is called
  void Register(protocol::ToolDefinition definition, ToolHandler handler) {
    auto name = definition.name;
    entries_[name] = ToolEntry{std::move(definition), std::move(handler)};
  }

  /// Invoke a tool by name.
  ///
  /// @param name Tool name (local, without MChiP prefix)
  /// @param args JSON object of tool arguments
  /// @return JSON CallToolResult, or error result if tool not found
  protocol::json Invoke(const std::string& name,
                        const protocol::json& args) const {
    auto it = entries_.find(name);
    if (it == entries_.end()) {
      return protocol::json{
          {"content",
           {{{"type", "text"},
             {"text", "Tool not found: " + name}}}},
          {"isError", true}};
    }
    return it->second.handler(args);
  }

  /// Return all registered tool definitions as a JSON array.
  ///
  /// Used to service ListMcpTools requests from the gateway.
  protocol::json ListTools() const {
    auto tools = protocol::json::array();
    for (const auto& [name, entry] : entries_) {
      tools.push_back(entry.definition.ToJson());
    }
    return tools;
  }

  /// Check whether a tool is registered.
  bool HasTool(const std::string& name) const {
    return entries_.count(name) > 0;
  }

  /// Return the number of registered tools.
  size_t Size() const { return entries_.size(); }

 private:
  std::unordered_map<std::string, ToolEntry> entries_;
};

}  // namespace mchips::sdk

#endif  // MCHIPS_SDK_TOOL_REGISTRAR_H_
