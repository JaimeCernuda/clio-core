/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mchips/protocol/mcp_error.h>

namespace mchips::protocol {

/// Create a JsonRpcError from a typed MCP error code.
JsonRpcError MakeError(McpErrorCode code, const std::string& message) {
  JsonRpcError err;
  err.code = static_cast<int>(code);
  err.message = message;
  return err;
}

/// Create a JsonRpcError with additional structured data.
JsonRpcError MakeError(McpErrorCode code, const std::string& message,
                       const json& data) {
  JsonRpcError err;
  err.code = static_cast<int>(code);
  err.message = message;
  err.data = data;
  return err;
}

}  // namespace mchips::protocol
