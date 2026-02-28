/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_PROTOCOL_MCP_MESSAGE_H_
#define MCHIPS_PROTOCOL_MCP_MESSAGE_H_

#include <mchips/protocol/json_rpc.h>
#include <mchips/protocol/mcp_types.h>

#include <variant>

namespace mchips::protocol {

/// Union of all JSON-RPC message types
using JsonRpcMessage =
    std::variant<JsonRpcRequest, JsonRpcResponse, JsonRpcNotification>;

/// Parse a raw JSON string into a JsonRpcMessage
JsonRpcMessage ParseMessage(const std::string& raw);

/// Parse a JSON value into a JsonRpcMessage
JsonRpcMessage ParseMessage(const json& j);

/// Serialize a JsonRpcMessage to JSON string
std::string SerializeMessage(const JsonRpcMessage& msg);

/// MCP method names
namespace methods {
inline constexpr const char* kInitialize = "initialize";
inline constexpr const char* kInitialized = "notifications/initialized";
inline constexpr const char* kToolsList = "tools/list";
inline constexpr const char* kToolsCall = "tools/call";
inline constexpr const char* kPing = "ping";
}  // namespace methods

}  // namespace mchips::protocol

#endif  // MCHIPS_PROTOCOL_MCP_MESSAGE_H_
