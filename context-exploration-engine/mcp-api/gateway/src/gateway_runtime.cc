/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mcp_gateway/gateway_runtime.h"

// TODO(Phase B.4): Implement gateway runtime methods
//
// Create():
//   - Initialize HttpServer, SessionManager, MchipRouter
//   - Discover MChiP pools from compose config
//   - Query each MChiP's kListMcpTools to populate routing table
//
// HandleHttpRequest():
//   - Parse JSON-RPC from request body
//   - Route by method:
//       "initialize"     → InitializeSession
//       "tools/list"     → router_.ListAllTools()
//       "tools/call"     → router_.Route() → MchipClient::AsyncCallMcpTool()
//       "ping"           → pong
//       "notifications/" → event handling
//   - Build JSON-RPC response, write to task output
//
// InitializeSession():
//   - Create session via SessionManager
//   - Return ServerCapabilities + ServerInfo
//
// CloseSession():
//   - Destroy session via SessionManager
//
// StartHttpServer() / StopHttpServer():
//   - Lifecycle control for the cpp-httplib server thread

namespace mchips::mcp_gateway {
}  // namespace mchips::mcp_gateway
