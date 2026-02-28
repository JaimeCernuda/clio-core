/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_PROTOCOL_JSON_RPC_H_
#define MCHIPS_PROTOCOL_JSON_RPC_H_

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>

namespace mchips::protocol {

using json = nlohmann::ordered_json;

/// JSON-RPC 2.0 error object
struct JsonRpcError {
  int code;
  std::string message;
  std::optional<json> data;

  json ToJson() const;
  static JsonRpcError FromJson(const json& j);
};

/// JSON-RPC 2.0 request (has id)
struct JsonRpcRequest {
  std::string jsonrpc = "2.0";
  std::string method;
  std::optional<json> params;
  json id;  // string | number

  json ToJson() const;
  static JsonRpcRequest FromJson(const json& j);
};

/// JSON-RPC 2.0 response (has id, result XOR error)
struct JsonRpcResponse {
  std::string jsonrpc = "2.0";
  json id;
  std::optional<json> result;
  std::optional<JsonRpcError> error;

  json ToJson() const;
  static JsonRpcResponse FromJson(const json& j);

  static JsonRpcResponse Success(const json& id, const json& result);
  static JsonRpcResponse Error(const json& id, const JsonRpcError& error);
};

/// JSON-RPC 2.0 notification (no id)
struct JsonRpcNotification {
  std::string jsonrpc = "2.0";
  std::string method;
  std::optional<json> params;

  json ToJson() const;
  static JsonRpcNotification FromJson(const json& j);
};

/// Standard JSON-RPC 2.0 error codes
enum class JsonRpcErrorCode : int {
  ParseError = -32700,
  InvalidRequest = -32600,
  MethodNotFound = -32601,
  InvalidParams = -32602,
  InternalError = -32603,
};

}  // namespace mchips::protocol

#endif  // MCHIPS_PROTOCOL_JSON_RPC_H_
