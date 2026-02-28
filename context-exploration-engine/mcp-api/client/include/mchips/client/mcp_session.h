/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_CLIENT_MCP_SESSION_H_
#define MCHIPS_CLIENT_MCP_SESSION_H_

#include <mchips/protocol/mcp_types.h>

#include <optional>
#include <string>

namespace mchips::client {

/// Client-side MCP session state.
///
/// Tracks the MCP-Session-Id, negotiated capabilities, and server info
/// after a successful initialize handshake.
class McpSession {
 public:
  McpSession() = default;

  /// Record a completed initialize handshake.
  void OnInitialized(const std::string& session_id,
                     const protocol::InitializeResult& result);

  /// True if Initialize() has been called and the session is active.
  bool IsActive() const { return active_; }

  /// Return the MCP-Session-Id header value.
  const std::string& SessionId() const { return session_id_; }

  /// Return the negotiated server capabilities.
  const protocol::ServerCapabilities& ServerCaps() const;

  /// Mark the session as closed.
  void Close() { active_ = false; }

 private:
  std::string session_id_;
  bool active_ = false;
  std::optional<protocol::InitializeResult> init_result_;
};

}  // namespace mchips::client

#endif  // MCHIPS_CLIENT_MCP_SESSION_H_
