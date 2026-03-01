/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mcp_gateway/http_server.h"

#include <algorithm>

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

void HttpServer::SetAllowedOrigins(std::vector<std::string> origins) {
  allowed_origins_ = std::move(origins);
}

bool HttpServer::IsOriginAllowed(const std::string& origin) const {
  if (allowed_origins_.empty()) return true;  // accept all when unconfigured
  return std::find(allowed_origins_.begin(), allowed_origins_.end(), origin)
         != allowed_origins_.end();
}

/// Register routes and start the httplib server in a background thread.
void HttpServer::Start(const std::string& host, int port, int num_threads) {
  // POST /mcp — handle JSON-RPC requests (MCP Streamable HTTP transport)
  server_.Post("/mcp", [this](const httplib::Request& req,
                               httplib::Response& res) {
    // Issue #3: Origin header validation
    if (req.has_header("Origin")) {
      auto origin = req.get_header_value("Origin");
      if (!IsOriginAllowed(origin)) {
        res.status = 403;
        res.set_content(R"({"error":"Forbidden: invalid Origin"})",
                        "application/json");
        return;
      }
    }

    std::string session_id;
    if (req.has_header("MCP-Session-Id")) {
      session_id = req.get_header_value("MCP-Session-Id");
    }

    // Issue #5: MCP-Protocol-Version header check on post-init requests.
    // Initialize requests don't yet have a session, so skip the check for them.
    if (!session_id.empty() && !req.has_header("MCP-Protocol-Version")) {
      res.status = 400;
      res.set_content(
          R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Missing required MCP-Protocol-Version header"}})",
          "application/json");
      return;
    }

    if (!request_handler_) {
      res.status = 500;
      res.set_content(R"({"error":"No request handler configured"})",
                      "application/json");
      return;
    }

    HttpResponse response = request_handler_(req.body, session_id);
    res.status = response.status_code;

    // Issue #4: 202 Accepted with no body for notifications
    if (response.no_body) {
      // Send empty response (no content)
    } else {
      res.set_content(response.body, response.content_type.empty()
                                         ? "application/json"
                                         : response.content_type);
    }

    // Issue #1: Set session ID from the response struct (not just echo).
    // This allows initialize to set the header on the first response.
    if (!response.session_id.empty()) {
      res.set_header("Mcp-Session-Id", response.session_id);
    } else if (!session_id.empty()) {
      // Echo back the client-provided session ID for non-init requests
      res.set_header("Mcp-Session-Id", session_id);
    }
  });

  // Issue #7: GET /mcp — return 405 Method Not Allowed
  server_.Get("/mcp", [](const httplib::Request& /*req*/,
                          httplib::Response& res) {
    res.status = 405;
    res.set_header("Allow", "POST, DELETE");
    res.set_content(R"({"error":"Method Not Allowed. Use POST or DELETE."})",
                    "application/json");
  });

  // DELETE /mcp — session teardown (Streamable HTTP spec)
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
