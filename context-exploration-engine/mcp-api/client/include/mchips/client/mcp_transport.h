/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_CLIENT_MCP_TRANSPORT_H_
#define MCHIPS_CLIENT_MCP_TRANSPORT_H_

#include <mchips/protocol/json_rpc.h>

#include <httplib.h>

#include <memory>
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

  /// Send a JSON-RPC request and return the parsed response body as JSON.
  ///
  /// Sets Content-Type: application/json and MCP-Protocol-Version headers.
  /// If a session ID has been set, includes MCP-Session-Id.
  ///
  /// @throws std::runtime_error on HTTP error or connection failure.
  protocol::json SendRequest(const protocol::JsonRpcRequest& request);

  /// Send a JSON-RPC notification (fire-and-forget, no response expected).
  void SendNotification(const protocol::JsonRpcNotification& notif);

  /// Store the session ID for all subsequent requests.
  void SetSessionId(const std::string& session_id);

  /// Return the current session ID (may be populated from server header).
  const std::string& GetSessionId() const;

  /// Send HTTP DELETE to close the session, then reset session state.
  void Close();

 private:
  std::string base_url_;
  std::string path_;          ///< URL path component (e.g., "/mcp")
  std::string session_id_;
  std::unique_ptr<httplib::Client> http_client_;

  /// Apply common MCP headers to a request (called before every POST/DELETE).
  httplib::Headers MakeHeaders() const;
};

}  // namespace mchips::client

#endif  // MCHIPS_CLIENT_MCP_TRANSPORT_H_
