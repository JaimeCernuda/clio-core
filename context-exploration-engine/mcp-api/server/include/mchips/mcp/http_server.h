/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_HTTP_SERVER_H_
#define MCHIPS_MCP_HTTP_SERVER_H_

#include <functional>
#include <string>

namespace mchips::mcp {

/// HTTP server wrapper around cpp-httplib.
///
/// Handles the MCP Streamable HTTP transport:
///   POST /mcp  — JSON-RPC requests (tools/call, tools/list, initialize, etc.)
///   GET  /mcp  — SSE stream for server-to-client notifications
///   DELETE /mcp — Close session
///
/// Threading: httplib runs its own thread pool. Each handler submits a
/// Chimaera task and blocks on Future::Wait().
class HttpServer {
 public:
  using RequestHandler = std::function<std::string(const std::string& body,
                                                   const std::string& session_id)>;

  HttpServer() = default;

  // TODO(Phase 3): Implement HTTP server lifecycle
  // void Start(const std::string& host, int port, int num_threads);
  // void Stop();
  // void SetRequestHandler(RequestHandler handler);

 private:
  // httplib::Server server_;  // TODO(Phase 3)
};

}  // namespace mchips::mcp

#endif  // MCHIPS_MCP_HTTP_SERVER_H_
