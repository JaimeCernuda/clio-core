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

  // TODO(Phase 2): Implement client methods
  // protocol::InitializeResult Initialize();
  // std::vector<protocol::ToolDefinition> ListTools();
  // protocol::CallToolResult CallTool(const std::string& name,
  //                                    const protocol::json& args);
  // void Close();

 private:
  McpClientConfig config_;
  // McpTransport transport_;
  // McpSession session_;
};

}  // namespace mchips::client

#endif  // MCHIPS_CLIENT_MCP_CLIENT_H_
