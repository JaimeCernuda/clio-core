/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_GATEWAY_SSE_WRITER_H_
#define MCHIPS_MCP_GATEWAY_SSE_WRITER_H_

#include <string>

namespace mchips::mcp_gateway {

/// Formats and writes Server-Sent Events for streaming MCP responses.
///
/// MCP uses SSE for:
///   - Streaming tool call results (large outputs)
///   - Server-to-client notifications (progress, logging)
///   - Reconnection via Last-Event-Id header
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
