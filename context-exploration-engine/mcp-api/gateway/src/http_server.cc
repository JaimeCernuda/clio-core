/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mcp_gateway/http_server.h"
#include "mchips/mcp_gateway/sse_writer.h"

namespace mchips::mcp_gateway {

HttpServer::~HttpServer() {
  Stop();
}

void HttpServer::SetRequestHandler(RequestHandler handler) {
  request_handler_ = std::move(handler);
}

void HttpServer::SetDeleteHandler(DeleteHandler handler) {
  delete_handler_ = std::move(handler);
}

/// Register routes and start the httplib server in a background thread.
void HttpServer::Start(const std::string& host, int port, int num_threads) {
  // POST /mcp — handle JSON-RPC requests
  server_.Post("/mcp", [this](const httplib::Request& req,
                               httplib::Response& res) {
    std::string session_id;
    if (req.has_header("MCP-Session-Id")) {
      session_id = req.get_header_value("MCP-Session-Id");
    }

    if (!request_handler_) {
      res.status = 500;
      res.set_content(R"({"error":"No request handler configured"})",
                      "application/json");
      return;
    }

    HttpResponse response = request_handler_(req.body, session_id);
    res.status = response.status_code;
    res.set_content(response.body, response.content_type.empty()
                                       ? "application/json"
                                       : response.content_type);

    // Echo session ID back if it came from the request
    if (!session_id.empty()) {
      res.set_header("MCP-Session-Id", session_id);
    }
  });

  // GET /mcp — SSE stream for server-to-client notifications
  server_.Get("/mcp", [](const httplib::Request& /*req*/,
                          httplib::Response& res) {
    res.status = 200;
    res.set_header("Content-Type", "text/event-stream");
    res.set_header("Cache-Control", "no-cache");
    res.set_header("Connection", "keep-alive");
    // Minimal SSE response: send a keep-alive comment
    SseWriter writer;
    res.set_content(writer.FormatKeepAlive(), "text/event-stream");
  });

  // DELETE /mcp — close session
  server_.Delete("/mcp", [this](const httplib::Request& req,
                                 httplib::Response& res) {
    std::string session_id;
    if (req.has_header("MCP-Session-Id")) {
      session_id = req.get_header_value("MCP-Session-Id");
    }

    if (!delete_handler_) {
      res.status = 200;
      res.set_content("{}", "application/json");
      return;
    }

    HttpResponse response = delete_handler_(session_id);
    res.status = response.status_code;
    res.set_content(response.body, response.content_type.empty()
                                       ? "application/json"
                                       : response.content_type);
  });

  server_.new_task_queue = [num_threads] {
    return new httplib::ThreadPool(num_threads);
  };

  // Start in background thread
  server_thread_ = std::thread([this, host, port] {
    server_.listen(host.c_str(), port);
  });

  // Give the server a moment to start
  server_.wait_until_ready();
}

/// Gracefully stop the HTTP server and join the thread.
void HttpServer::Stop() {
  server_.stop();
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
}

}  // namespace mchips::mcp_gateway
