/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/client/native_client.h"

#include <mchips/protocol/json_rpc.h>
#include <mchips/protocol/mcp_message.h>
#include <mchips/protocol/mcp_types.h>

#include <stdexcept>

namespace mchips::client {

void NativeClient::Connect(const chi::PoolId& gateway_pool_id) {
  gateway_client_.Init(gateway_pool_id);
}

std::string NativeClient::Initialize(const std::string& protocol_version) {
  protocol::json params = {
      {"protocolVersion", protocol_version},
      {"capabilities", protocol::json::object()},
      {"clientInfo", {{"name", "mchips-native-client"}, {"version", "0.2.0"}}}
  };

  auto body = BuildJsonRpcRequest(protocol::methods::kInitialize, params);
  auto response = SendRequest(body);

  // Extract session ID from the response (embedded in body by gateway)
  if (response.contains("sessionId")) {
    session_id_ = response["sessionId"].get<std::string>();
  }

  // Send initialized notification
  protocol::json notif_body = {
      {"jsonrpc", "2.0"},
      {"method", protocol::methods::kInitialized}
  };
  SendRequest(notif_body.dump());

  return session_id_;
}

std::vector<protocol::ToolDefinition> NativeClient::ListTools() {
  protocol::json params = protocol::json::object();
  auto body = BuildJsonRpcRequest(protocol::methods::kToolsList, params);
  auto response = SendRequest(body);

  std::vector<protocol::ToolDefinition> tools;
  if (response.contains("result") && response["result"].contains("tools")) {
    for (const auto& tool_json : response["result"]["tools"]) {
      tools.push_back(protocol::ToolDefinition::FromJson(tool_json));
    }
  }
  return tools;
}

protocol::json NativeClient::CallTool(const std::string& tool_name,
                                       const protocol::json& args) {
  protocol::json params = {
      {"name", tool_name},
      {"arguments", args}
  };
  auto body = BuildJsonRpcRequest(protocol::methods::kToolsCall, params);
  auto response = SendRequest(body);

  if (response.contains("result")) {
    return response["result"];
  }
  if (response.contains("error")) {
    throw std::runtime_error(
        "Tool call failed: " +
        response["error"].value("message", "unknown error"));
  }
  return response;
}

std::string NativeClient::BuildJsonRpcRequest(const std::string& method,
                                               const protocol::json& params) {
  protocol::json req = {
      {"jsonrpc", "2.0"},
      {"id", next_id_++},
      {"method", method},
      {"params", params}
  };
  return req.dump();
}

protocol::json NativeClient::SendRequest(const std::string& body) {
  auto future = gateway_client_.AsyncHandleHttpRequest(
      chi::PoolQuery::Local(), body, session_id_);
  future.Wait();

  auto response_str = future->response_body_.str();
  if (response_str.empty()) {
    return protocol::json::object();
  }
  return protocol::json::parse(response_str);
}

}  // namespace mchips::client
