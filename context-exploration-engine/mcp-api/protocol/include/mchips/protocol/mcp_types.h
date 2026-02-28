/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_PROTOCOL_MCP_TYPES_H_
#define MCHIPS_PROTOCOL_MCP_TYPES_H_

#include <mchips/protocol/json_rpc.h>

#include <optional>
#include <string>
#include <vector>

namespace mchips::protocol {

/// MCP protocol version this implementation supports
inline constexpr const char* kMcpProtocolVersion = "2025-11-25";

/// Tool annotations per MCP spec
struct ToolAnnotations {
  std::optional<std::string> title;
  bool readOnlyHint = false;
  bool destructiveHint = true;
  bool idempotentHint = false;
  bool openWorldHint = true;
  double priority = 0.5;  // 0.0 (low) to 1.0 (high)

  json ToJson() const;
  static ToolAnnotations FromJson(const json& j);
};

/// MCP tool definition
struct ToolDefinition {
  std::string name;
  std::string description;
  json inputSchema;                          // JSON Schema (draft 2020-12)
  std::optional<json> outputSchema;          // Optional output schema
  std::optional<ToolAnnotations> annotations;

  json ToJson() const;
  static ToolDefinition FromJson(const json& j);
};

/// Content item in a tool result
struct ContentItem {
  std::string type;  // "text", "image", "resource"
  std::optional<std::string> text;
  std::optional<std::string> mimeType;
  std::optional<std::string> data;  // base64 for binary

  json ToJson() const;
  static ContentItem FromJson(const json& j);
};

/// Result of calling a tool
struct CallToolResult {
  std::vector<ContentItem> content;
  bool isError = false;

  json ToJson() const;
  static CallToolResult FromJson(const json& j);
};

/// Server capabilities advertised during initialize
struct ServerCapabilities {
  struct ToolsCapability {
    bool listChanged = true;  // supports notifications/tools/list_changed
  };

  std::optional<ToolsCapability> tools;
  // Future: resources, prompts, logging

  json ToJson() const;
  static ServerCapabilities FromJson(const json& j);
};

/// Client capabilities received during initialize
struct ClientCapabilities {
  // Future: roots, sampling, elicitation
  json ToJson() const;
  static ClientCapabilities FromJson(const json& j);
};

/// Server info returned in initialize response
struct ServerInfo {
  std::string name = "mchips";
  std::string version = "0.1.0";

  json ToJson() const;
  static ServerInfo FromJson(const json& j);
};

/// Initialize result
struct InitializeResult {
  std::string protocolVersion = kMcpProtocolVersion;
  ServerCapabilities capabilities;
  ServerInfo serverInfo;

  json ToJson() const;
  static InitializeResult FromJson(const json& j);
};

}  // namespace mchips::protocol

#endif  // MCHIPS_PROTOCOL_MCP_TYPES_H_
