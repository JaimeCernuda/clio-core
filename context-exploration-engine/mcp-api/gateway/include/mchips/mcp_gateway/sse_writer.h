/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_GATEWAY_SSE_WRITER_H_
#define MCHIPS_MCP_GATEWAY_SSE_WRITER_H_

#include <string>

namespace mchips::mcp_gateway {

/// Formats Server-Sent Events for optional streaming in MCP responses.
///
/// In the Streamable HTTP transport, POST responses MAY use SSE format
/// (Content-Type: text/event-stream) for streaming large results or
/// server-initiated notifications. This is the response FORMAT, not
/// the deprecated SSE transport (which used separate /sse + /messages
/// endpoints).
///
/// Event format:
///   event: message\n
///   id: <event-id>\n
///   data: <json-rpc-message>\n
///   \n
class SseWriter {
 public:
  SseWriter() = default;

  /// Format an SSE event string.
  ///
  /// @param event_type  Event type name (e.g., "message", "error")
  /// @param data        Data payload (JSON string)
  /// @param event_id    Optional event ID for reconnection support
  /// @return Formatted SSE event string ready to send
  std::string FormatEvent(const std::string& event_type,
                           const std::string& data,
                           const std::string& event_id = "") const;

  /// Format a keep-alive comment (prevents connection timeouts).
  std::string FormatKeepAlive() const;
};

}  // namespace mchips::mcp_gateway

#endif  // MCHIPS_MCP_GATEWAY_SSE_WRITER_H_
