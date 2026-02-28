/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_CLIENT_MCP_TRANSPORT_H_
#define MCHIPS_CLIENT_MCP_TRANSPORT_H_

#include <mchips/protocol/json_rpc.h>

#include <string>

namespace mchips::client {

/// Streamable HTTP transport for MCP client.
///
/// Implements the MCP Streamable HTTP transport specification:
///   POST /mcp   — send JSON-RPC request, receive response
///   GET  /mcp   — open SSE stream for server notifications
///   DELETE /mcp — close session
///
/// Manages MCP-Session-Id and MCP-Protocol-Version headers.
class McpTransport {
 public:
  explicit McpTransport(const std::string& base_url);
  ~McpTransport();

  // TODO(Phase 2): Implement transport methods
  // protocol::json SendRequest(const protocol::JsonRpcRequest& request);
  // void SendNotification(const protocol::JsonRpcNotification& notif);
  // void SetSessionId(const std::string& session_id);
  // void Close();

 private:
  std::string base_url_;
  std::string session_id_;
  // httplib::Client http_client_;  // TODO(Phase 2)
};

}  // namespace mchips::client

#endif  // MCHIPS_CLIENT_MCP_TRANSPORT_H_
