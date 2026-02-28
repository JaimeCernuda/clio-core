/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_CLIENT_MCP_CLIENT_H_
#define MCHIPS_CLIENT_MCP_CLIENT_H_

#include <mchips/client/mcp_session.h>
#include <mchips/client/mcp_transport.h>
#include <mchips/protocol/mcp_types.h>

#include <string>
#include <vector>

namespace mchips::client {

/// Configuration for connecting to an MCP server.
struct McpClientConfig {
  std::string url;  // e.g., "http://localhost:8080/mcp"
};

/// Standalone MCP client — no Chimaera dependency.
///
/// Connects to any MCP-compliant server over Streamable HTTP.
/// Usable by any C++ agent application.
///
/// Usage:
///   McpClient client({"http://localhost:8080/mcp"});
///   client.Initialize();
///   auto tools = client.ListTools();
///   auto result = client.CallTool("put_blob", args);
///   client.Close();
class McpClient {
 public:
  explicit McpClient(const McpClientConfig& config);
  ~McpClient();

  /// Perform the MCP initialize handshake.
  ///
  /// Sends an initialize request, receives ServerCapabilities, then
  /// sends the notifications/initialized notification. Stores the
  /// session ID for subsequent requests.
  ///
  /// @throws std::runtime_error if the handshake fails.
  protocol::InitializeResult Initialize();

  /// List all tools available on the connected MCP server.
  ///
  /// @throws std::runtime_error if not initialized or request fails.
  std::vector<protocol::ToolDefinition> ListTools();

  /// Call a named tool with the given arguments.
  ///
  /// @param name  Tool name (e.g., "put_blob" or "cte__put_blob")
  /// @param args  Tool arguments as a JSON object
  /// @throws std::runtime_error if not initialized or request fails.
  protocol::CallToolResult CallTool(const std::string& name,
                                     const protocol::json& args);

  /// Close the MCP session (sends notifications/cancelled + HTTP DELETE).
  void Close();

  /// True if Initialize() has been called successfully.
  bool IsInitialized() const { return session_.IsActive(); }

 private:
  McpClientConfig config_;
  McpTransport transport_;
  McpSession session_;

  int next_id_ = 1;
  protocol::json NextId() { return next_id_++; }
};

}  // namespace mchips::client

#endif  // MCHIPS_CLIENT_MCP_CLIENT_H_
