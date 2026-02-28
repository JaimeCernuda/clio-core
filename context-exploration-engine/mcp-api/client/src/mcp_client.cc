/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mchips/client/mcp_client.h>
#include <mchips/protocol/mcp_message.h>

#include <stdexcept>

namespace mchips::client {

//=============================================================================
// McpSession
//=============================================================================

void McpSession::OnInitialized(const std::string& session_id,
                                const protocol::InitializeResult& result) {
  session_id_ = session_id;
  init_result_ = result;
  active_ = true;
}

const protocol::ServerCapabilities& McpSession::ServerCaps() const {
  if (!init_result_.has_value()) {
    throw std::runtime_error("Session not initialized");
  }
  return init_result_->capabilities;
}

//=============================================================================
// McpClient
//=============================================================================

McpClient::McpClient(const McpClientConfig& config)
    : config_(config), transport_(config.url) {}

McpClient::~McpClient() {
  if (session_.IsActive()) {
    try {
      Close();
    } catch (...) {
      // Best-effort cleanup in destructor
    }
  }
}

/// Perform the MCP initialize handshake.
protocol::InitializeResult McpClient::Initialize() {
  // Build initialize request
  protocol::JsonRpcRequest req;
  req.id = NextId();
  req.method = protocol::methods::kInitialize;
  req.params = protocol::json{
      {"protocolVersion", protocol::kMcpProtocolVersion},
      {"capabilities", protocol::ClientCapabilities{}.ToJson()},
      {"clientInfo", {{"name", "mchips-client"}, {"version", "0.1.0"}}}
  };

  auto response_json = transport_.SendRequest(req);

  // Extract session ID from response headers if present
  // (httplib doesn't expose headers easily from SendRequest, so we rely
  //  on the server setting the session ID in the response if needed)

  // Parse InitializeResult
  if (!response_json.contains("result")) {
    if (response_json.contains("error")) {
      auto err = protocol::JsonRpcError::FromJson(response_json["error"]);
      throw std::runtime_error(
          "Initialize failed: " + err.message);
    }
    throw std::runtime_error("Invalid initialize response");
  }

  auto init_result = protocol::InitializeResult::FromJson(
      response_json["result"]);

  // Send initialized notification (required by MCP spec)
  protocol::JsonRpcNotification notif;
  notif.method = protocol::methods::kInitialized;
  transport_.SendNotification(notif);

  // Session ID was auto-captured from the Mcp-Session-Id response header
  // inside SendRequest (per MCP Streamable HTTP spec 2025-11-25).
  std::string session_id = transport_.GetSessionId();
  session_.OnInitialized(session_id, init_result);

  return init_result;
}

/// List all tools from the connected MCP server.
std::vector<protocol::ToolDefinition> McpClient::ListTools() {
  if (!session_.IsActive()) {
    throw std::runtime_error("Not initialized — call Initialize() first");
  }

  protocol::JsonRpcRequest req;
  req.id = NextId();
  req.method = protocol::methods::kToolsList;
  req.params = protocol::json::object();

  auto response_json = transport_.SendRequest(req);

  if (!response_json.contains("result")) {
    if (response_json.contains("error")) {
      auto err = protocol::JsonRpcError::FromJson(response_json["error"]);
      throw std::runtime_error("tools/list failed: " + err.message);
    }
    throw std::runtime_error("Invalid tools/list response");
  }

  const auto& result = response_json["result"];
  std::vector<protocol::ToolDefinition> tools;

  if (result.contains("tools") && result["tools"].is_array()) {
    for (const auto& tool_json : result["tools"]) {
      tools.push_back(protocol::ToolDefinition::FromJson(tool_json));
    }
  }

  return tools;
}

/// Call a named tool with arguments.
protocol::CallToolResult McpClient::CallTool(const std::string& name,
                                              const protocol::json& args) {
  if (!session_.IsActive()) {
    throw std::runtime_error("Not initialized — call Initialize() first");
  }

  protocol::JsonRpcRequest req;
  req.id = NextId();
  req.method = protocol::methods::kToolsCall;
  req.params = protocol::json{
      {"name", name},
      {"arguments", args}
  };

  auto response_json = transport_.SendRequest(req);

  if (!response_json.contains("result")) {
    if (response_json.contains("error")) {
      auto err = protocol::JsonRpcError::FromJson(response_json["error"]);
      // Wrap JSON-RPC errors as tool errors
      protocol::CallToolResult err_result;
      protocol::ContentItem item;
      item.type = "text";
      item.text = "JSON-RPC error: " + err.message;
      err_result.content.push_back(item);
      err_result.isError = true;
      return err_result;
    }
    throw std::runtime_error("Invalid tools/call response");
  }

  return protocol::CallToolResult::FromJson(response_json["result"]);
}

/// Close the session.
void McpClient::Close() {
  if (session_.IsActive()) {
    transport_.Close();
    session_.Close();
  }
}

}  // namespace mchips::client
