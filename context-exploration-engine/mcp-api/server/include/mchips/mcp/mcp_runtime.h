/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_MCP_RUNTIME_H_
#define MCHIPS_MCP_MCP_RUNTIME_H_

#include <mchips/mcp/http_server.h>
#include <mchips/mcp/session_manager.h>
#include <mchips/mcp/tool_registry.h>

namespace mchips::mcp {

/// ChiMod runtime for the MCP server.
///
/// Owns the HttpServer, SessionManager, and ToolRegistry.
/// Handles Chimaera tasks by routing JSON-RPC requests to the
/// appropriate MCP method handler.
///
/// Data flow:
///   HttpServer receives HTTP request
///   → submits HandleHttpRequestTask to Chimaera
///   → Runtime coroutine parses JSON-RPC
///   → dispatches to ToolRegistry for tools/call
///   → co_awaits CTE/CAE subtasks
///   → returns JSON-RPC response
class Runtime {
 public:
  Runtime() = default;

  // TODO(Phase 3): Implement runtime lifecycle and request handlers
  // void Start(const std::string& host, int port);
  // void Stop();
  // chi::TaskResume HandleHttpRequest(HandleHttpRequestTask* task);
  // chi::TaskResume CallTool(CallToolTask* task);

 private:
  HttpServer http_server_;
  SessionManager session_manager_;
  ToolRegistry tool_registry_;
};

}  // namespace mchips::mcp

#endif  // MCHIPS_MCP_MCP_RUNTIME_H_
