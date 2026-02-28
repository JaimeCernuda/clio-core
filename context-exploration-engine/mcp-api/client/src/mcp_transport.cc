/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mchips/client/mcp_transport.h>
#include <mchips/protocol/mcp_types.h>

#include <stdexcept>
#include <string>

namespace mchips::client {

namespace {

/// Split a full URL into (host:port, path) components for httplib.
///
/// Handles http:// and https:// schemes. Extracts optional port.
/// Returns {host_with_optional_port, path}.
std::pair<std::string, std::string> SplitUrl(const std::string& url) {
  // Strip scheme
  std::string rest = url;
  if (rest.substr(0, 7) == "http://") {
    rest = rest.substr(7);
  } else if (rest.substr(0, 8) == "https://") {
    rest = rest.substr(8);
  }

  // Find path separator
  auto slash = rest.find('/');
  std::string host;
  std::string path;
  if (slash == std::string::npos) {
    host = rest;
    path = "/";
  } else {
    host = rest.substr(0, slash);
    path = rest.substr(slash);
  }

  return {host, path};
}

}  // namespace

McpTransport::McpTransport(const std::string& base_url)
    : base_url_(base_url) {
  auto [host, path] = SplitUrl(base_url);
  path_ = path;
  http_client_ = std::make_unique<httplib::Client>(host);
  http_client_->set_connection_timeout(10);
  http_client_->set_read_timeout(60);
}

McpTransport::~McpTransport() = default;

httplib::Headers McpTransport::MakeHeaders() const {
  httplib::Headers headers = {
      {"Content-Type", "application/json"},
      {"Accept", "application/json, text/event-stream"},
      {"MCP-Protocol-Version", protocol::kMcpProtocolVersion},
  };
  if (!session_id_.empty()) {
    headers.emplace("MCP-Session-Id", session_id_);
  }
  return headers;
}

/// Send a JSON-RPC request via HTTP POST and return the parsed JSON body.
protocol::json McpTransport::SendRequest(
    const protocol::JsonRpcRequest& request) {
  auto body = request.ToJson().dump();
  auto headers = MakeHeaders();

  auto result = http_client_->Post(path_, headers, body, "application/json");
  if (!result) {
    throw std::runtime_error(
        std::string("HTTP request failed: ") +
        httplib::to_string(result.error()));
  }

  if (result->status < 200 || result->status >= 300) {
    throw std::runtime_error(
        "HTTP error " + std::to_string(result->status) +
        ": " + result->body);
  }

  try {
    return protocol::json::parse(result->body);
  } catch (const protocol::json::parse_error& e) {
    throw std::runtime_error(
        std::string("Response JSON parse error: ") + e.what() +
        " body: " + result->body);
  }
}

/// Send a notification (no response expected).
void McpTransport::SendNotification(
    const protocol::JsonRpcNotification& notif) {
  auto body = notif.ToJson().dump();
  auto headers = MakeHeaders();
  // Fire and forget — ignore errors on notifications
  http_client_->Post(path_, headers, body, "application/json");
}

/// Set the MCP-Session-Id for subsequent requests.
void McpTransport::SetSessionId(const std::string& session_id) {
  session_id_ = session_id;
}

/// Send HTTP DELETE to tear down the session.
void McpTransport::Close() {
  if (session_id_.empty()) {
    return;
  }
  auto headers = MakeHeaders();
  http_client_->Delete(path_, headers);
  session_id_.clear();
}

}  // namespace mchips::client
