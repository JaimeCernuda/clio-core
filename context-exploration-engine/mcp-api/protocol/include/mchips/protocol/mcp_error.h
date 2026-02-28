/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_PROTOCOL_MCP_ERROR_H_
#define MCHIPS_PROTOCOL_MCP_ERROR_H_

#include <mchips/protocol/json_rpc.h>

namespace mchips::protocol {

/// MCP-specific error codes (in addition to JSON-RPC standard codes)
enum class McpErrorCode : int {
  // JSON-RPC standard errors
  ParseError = -32700,
  InvalidRequest = -32600,
  MethodNotFound = -32601,
  InvalidParams = -32602,
  InternalError = -32603,

  // MCP-specific errors
  SessionNotFound = -32000,
  SessionExpired = -32001,
  ToolNotFound = -32002,
  ToolExecutionError = -32003,
  ElicitationError = -32042,
};

/// Create a JsonRpcError from an MCP error code
JsonRpcError MakeError(McpErrorCode code, const std::string& message);

/// Create a JsonRpcError with additional data
JsonRpcError MakeError(McpErrorCode code, const std::string& message,
                       const json& data);

}  // namespace mchips::protocol

#endif  // MCHIPS_PROTOCOL_MCP_ERROR_H_
