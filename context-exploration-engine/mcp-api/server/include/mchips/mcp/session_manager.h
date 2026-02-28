/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_SESSION_MANAGER_H_
#define MCHIPS_MCP_SESSION_MANAGER_H_

#include <mchips/protocol/mcp_types.h>

#include <string>

namespace mchips::mcp {

/// Manages MCP sessions identified by MCP-Session-Id header.
///
/// Sessions are stored in-memory, protected by chi::CoRwLock for
/// concurrent access from Chimaera coroutine workers.
///
/// Lifecycle:
///   1. Client sends initialize request (no session ID)
///   2. Server creates session, returns MCP-Session-Id in response header
///   3. Client includes MCP-Session-Id on all subsequent requests
///   4. Client sends DELETE /mcp to close session
///   5. Sessions expire after configurable timeout (default: 30 min)
class SessionManager {
 public:
  SessionManager() = default;

  // TODO(Phase 3): Implement session lifecycle
  // std::string CreateSession(const protocol::ClientCapabilities& caps);
  // bool ValidateSession(const std::string& session_id);
  // void DestroySession(const std::string& session_id);
  // void CleanupExpired();
};

}  // namespace mchips::mcp

#endif  // MCHIPS_MCP_SESSION_MANAGER_H_
