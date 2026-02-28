/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mchips/client/mcp_transport.h>
#include <mchips/protocol/mcp_types.h>

#include <sstream>
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

namespace {

/// Extract the first JSON payload from an SSE response body.
///
/// SSE lines look like:
///   event: message\n
///   data: {...}\n
///   \n
///
/// Returns the first non-empty "data:" value found, or throws if none.
std::string ExtractSseData(const std::string& sse_body) {
  std::string data_value;
  std::istringstream stream(sse_body);
  std::string line;
  while (std::getline(stream, line)) {
    // Strip trailing CR (Windows line endings)
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.substr(0, 5) == "data:") {
      std::string val = line.substr(5);
      // Strip leading space
      if (!val.empty() && val.front() == ' ') {
        val = val.substr(1);
      }
      if (!val.empty()) {
        return val;  // Return first non-empty data line
      }
    }
  }
  throw std::runtime_error(
      "No 'data:' field found in SSE response: " + sse_body);
}

}  // namespace

/// Send a JSON-RPC request via HTTP POST and return the parsed JSON body.
///
/// Handles both plain JSON and SSE-formatted responses, as per MCP
/// Streamable HTTP spec: servers may respond with either content type.
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

  // Capture session ID if the server sends it as a response header.
  // Per MCP spec 2025-11-25: the server assigns a session ID via
  // 'Mcp-Session-Id' on the initialize response.
  auto sid = result->get_header_value("Mcp-Session-Id");
  if (!sid.empty() && session_id_.empty()) {
    session_id_ = sid;
  }

  // Check response content type — MCP servers may return either format
  auto content_type = result->get_header_value("Content-Type");
  std::string json_str;
  if (content_type.find("text/event-stream") != std::string::npos) {
    // SSE response: extract first data: line
    json_str = ExtractSseData(result->body);
  } else {
    json_str = result->body;
  }

  try {
    return protocol::json::parse(json_str);
  } catch (const protocol::json::parse_error& e) {
    throw std::runtime_error(
        std::string("Response JSON parse error: ") + e.what() +
        " body: " + json_str);
  }
}

/// Return the current session ID (captured from server response headers).
const std::string& McpTransport::GetSessionId() const {
  return session_id_;
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
