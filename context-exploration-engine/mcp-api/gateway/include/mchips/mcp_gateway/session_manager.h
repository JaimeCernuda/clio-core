/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_GATEWAY_SESSION_MANAGER_H_
#define MCHIPS_MCP_GATEWAY_SESSION_MANAGER_H_

#include <chrono>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace mchips::mcp_gateway {

/// Server-side MCP session state.
struct McpSessionState {
  std::string session_id;
  std::string protocol_version;
  std::chrono::steady_clock::time_point created_at;
  std::chrono::steady_clock::time_point last_active;
};

/// Manages MCP session lifecycle (MCP-Session-Id header).
///
/// Sessions are created during the `initialize` handshake and destroyed
/// via HTTP DELETE or timeout expiry. The session stores negotiated
/// capabilities and protocol version.
///
/// Threading: Uses std::shared_mutex for concurrent access (upgrade to
/// chi::CoRwLock when running inside Chimaera).
class SessionManager {
 public:
  explicit SessionManager(
      std::chrono::seconds timeout = std::chrono::seconds(1800));

  /// Create a new session and return its ID.
  ///
  /// Generates a UUID-like random ID, stores state, returns the ID.
  std::string CreateSession(const std::string& protocol_version);

  /// Return true if the session exists and has not expired.
  bool ValidateSession(const std::string& session_id);

  /// Destroy a session immediately.
  void DestroySession(const std::string& session_id);

  /// Remove all sessions that have exceeded the timeout.
  void CleanupExpired();

  /// Return the number of active sessions.
  size_t Count() const;

 private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, McpSessionState> sessions_;
  std::chrono::seconds timeout_;

  static std::string GenerateSessionId();
};

}  // namespace mchips::mcp_gateway

#endif  // MCHIPS_MCP_GATEWAY_SESSION_MANAGER_H_
