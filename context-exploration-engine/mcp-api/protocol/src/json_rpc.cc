/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mchips/protocol/json_rpc.h>
#include <mchips/protocol/mcp_message.h>

#include <stdexcept>

namespace mchips::protocol {

//=============================================================================
// JsonRpcError
//=============================================================================

/// Serialize error to JSON object per JSON-RPC 2.0 spec.
json JsonRpcError::ToJson() const {
  json j;
  j["code"] = code;
  j["message"] = message;
  if (data.has_value()) {
    j["data"] = *data;
  }
  return j;
}

/// Deserialize error from JSON object.
JsonRpcError JsonRpcError::FromJson(const json& j) {
  JsonRpcError err;
  err.code = j.at("code").get<int>();
  err.message = j.at("message").get<std::string>();
  if (j.contains("data")) {
    err.data = j["data"];
  }
  return err;
}

//=============================================================================
// JsonRpcRequest
//=============================================================================

/// Serialize request to JSON object per JSON-RPC 2.0 spec.
json JsonRpcRequest::ToJson() const {
  json j;
  j["jsonrpc"] = jsonrpc;
  j["id"] = id;
  j["method"] = method;
  if (params.has_value()) {
    j["params"] = *params;
  }
  return j;
}

/// Deserialize request from JSON object.
JsonRpcRequest JsonRpcRequest::FromJson(const json& j) {
  JsonRpcRequest req;
  req.jsonrpc = j.value("jsonrpc", "2.0");
  req.method = j.at("method").get<std::string>();
  if (j.contains("params")) {
    req.params = j["params"];
  }
  req.id = j.at("id");
  return req;
}

//=============================================================================
// JsonRpcResponse
//=============================================================================

/// Serialize response to JSON object (result XOR error).
json JsonRpcResponse::ToJson() const {
  json j;
  j["jsonrpc"] = jsonrpc;
  j["id"] = id;
  if (result.has_value()) {
    j["result"] = *result;
  } else if (error.has_value()) {
    j["error"] = error->ToJson();
  }
  return j;
}

/// Deserialize response from JSON object.
JsonRpcResponse JsonRpcResponse::FromJson(const json& j) {
  JsonRpcResponse resp;
  resp.jsonrpc = j.value("jsonrpc", "2.0");
  resp.id = j.at("id");
  if (j.contains("result")) {
    resp.result = j["result"];
  }
  if (j.contains("error")) {
    resp.error = JsonRpcError::FromJson(j["error"]);
  }
  return resp;
}

/// Factory: build a success response.
JsonRpcResponse JsonRpcResponse::Success(const json& id, const json& result) {
  JsonRpcResponse resp;
  resp.id = id;
  resp.result = result;
  return resp;
}

/// Factory: build an error response.
JsonRpcResponse JsonRpcResponse::Error(const json& id,
                                        const JsonRpcError& error) {
  JsonRpcResponse resp;
  resp.id = id;
  resp.error = error;
  return resp;
}

//=============================================================================
// JsonRpcNotification
//=============================================================================

/// Serialize notification to JSON object (no id field).
json JsonRpcNotification::ToJson() const {
  json j;
  j["jsonrpc"] = jsonrpc;
  j["method"] = method;
  if (params.has_value()) {
    j["params"] = *params;
  }
  return j;
}

/// Deserialize notification from JSON object.
JsonRpcNotification JsonRpcNotification::FromJson(const json& j) {
  JsonRpcNotification notif;
  notif.jsonrpc = j.value("jsonrpc", "2.0");
  notif.method = j.at("method").get<std::string>();
  if (j.contains("params")) {
    notif.params = j["params"];
  }
  return notif;
}

//=============================================================================
// Message dispatch (declared in mcp_message.h)
//=============================================================================

/// Parse a raw JSON string into a typed JsonRpcMessage variant.
JsonRpcMessage ParseMessage(const std::string& raw) {
  try {
    auto j = json::parse(raw);
    return ParseMessage(j);
  } catch (const json::parse_error& e) {
    throw std::invalid_argument(
        std::string("JSON parse error: ") + e.what());
  }
}

/// Determine message type from JSON structure and return typed variant.
///
/// Decision rules (per JSON-RPC 2.0 spec):
///   - Has "method" + "id"  → Request
///   - Has "method", no "id" → Notification
///   - Has "result" or "error" → Response
JsonRpcMessage ParseMessage(const json& j) {
  const bool has_method = j.contains("method");
  const bool has_id = j.contains("id");
  const bool has_result = j.contains("result");
  const bool has_error = j.contains("error");

  if (has_method && has_id) {
    return JsonRpcRequest::FromJson(j);
  } else if (has_method && !has_id) {
    return JsonRpcNotification::FromJson(j);
  } else if (has_result || has_error) {
    return JsonRpcResponse::FromJson(j);
  } else {
    throw std::invalid_argument("Cannot determine JSON-RPC message type");
  }
}

/// Serialize any JsonRpcMessage variant to a JSON string.
std::string SerializeMessage(const JsonRpcMessage& msg) {
  json j = std::visit([](const auto& m) { return m.ToJson(); }, msg);
  return j.dump();
}

}  // namespace mchips::protocol
