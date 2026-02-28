/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_GATEWAY_SESSION_MANAGER_H_
#define MCHIPS_MCP_GATEWAY_SESSION_MANAGER_H_

#include <string>

namespace mchips::mcp_gateway {

/// Manages MCP session lifecycle (MCP-Session-Id header).
///
/// Sessions are created during the `initialize` handshake and destroyed
/// via HTTP DELETE or timeout expiry. The session stores negotiated
/// capabilities and protocol version.
///
/// Threading: Uses chi::CoRwLock when running inside Chimaera, or
/// std::shared_mutex for standalone mode (Stage 1).
class SessionManager {
 public:
  SessionManager() = default;

  // TODO(Phase B.4): Implement session lifecycle
  // std::string CreateSession(const std::string& protocol_version);
  // bool ValidateSession(const std::string& session_id);
  // void DestroySession(const std::string& session_id);
  // void CleanupExpired();
};

}  // namespace mchips::mcp_gateway

#endif  // MCHIPS_MCP_GATEWAY_SESSION_MANAGER_H_
