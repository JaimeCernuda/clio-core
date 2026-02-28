/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_GATEWAY_HTTP_SERVER_H_
#define MCHIPS_MCP_GATEWAY_HTTP_SERVER_H_

#include <httplib.h>

#include <functional>
#include <string>
#include <thread>

namespace mchips::mcp_gateway {

/// HTTP response payload returned by the request handler callback.
struct HttpResponse {
  int status_code;
  std::string body;
  std::string content_type;
};

/// HTTP server wrapper around cpp-httplib.
///
/// Handles the MCP Streamable HTTP transport:
///   POST /mcp   — JSON-RPC requests (tools/call, tools/list, initialize, etc.)
///   GET  /mcp   — SSE stream for server-to-client notifications
///   DELETE /mcp  — Close session
///
/// Threading: httplib runs its own thread pool. Each POST handler submits a
/// Chimaera task (via the request callback) and blocks on Future::Wait().
class HttpServer {
 public:
  /// Callback for handling POST /mcp requests.
  /// Receives: request body, MCP-Session-Id header value.
  /// Returns: HttpResponse to send back.
  using RequestHandler = std::function<HttpResponse(
      const std::string& body, const std::string& session_id)>;

  /// Callback for handling DELETE /mcp requests.
  /// Receives: MCP-Session-Id header value.
  /// Returns: HttpResponse to send back.
  using DeleteHandler = std::function<HttpResponse(
      const std::string& session_id)>;

  HttpServer() = default;
  ~HttpServer();

  /// Register the POST /mcp request handler.
  void SetRequestHandler(RequestHandler handler);

  /// Register the DELETE /mcp session-close handler.
  void SetDeleteHandler(DeleteHandler handler);

  /// Start listening on the given address and port.
  ///
  /// Spawns a background thread. Returns after the server is ready.
  void Start(const std::string& host, int port, int num_threads);

  /// Gracefully stop the HTTP server and join the listener thread.
  void Stop();

 private:
  httplib::Server server_;
  std::thread server_thread_;
  RequestHandler request_handler_;
  DeleteHandler delete_handler_;
};

}  // namespace mchips::mcp_gateway

#endif  // MCHIPS_MCP_GATEWAY_HTTP_SERVER_H_
