/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_SSE_WRITER_H_
#define MCHIPS_MCP_SSE_WRITER_H_

#include <mchips/protocol/json_rpc.h>

#include <string>

namespace mchips::mcp {

/// Server-Sent Events writer for MCP streaming responses.
///
/// Formats JSON-RPC messages as SSE events:
///   event: message
///   data: {"jsonrpc": "2.0", ...}
///
/// Supports Last-Event-Id for reconnection.
class SseWriter {
 public:
  SseWriter() = default;

  // TODO(Phase 3): Implement SSE formatting and streaming
  // std::string FormatEvent(const std::string& event_type,
  //                         const protocol::json& data,
  //                         const std::string& id = "");
};

}  // namespace mchips::mcp

#endif  // MCHIPS_MCP_SSE_WRITER_H_
