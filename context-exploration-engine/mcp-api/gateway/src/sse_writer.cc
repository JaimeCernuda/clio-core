/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mcp_gateway/sse_writer.h"

#include <sstream>

namespace mchips::mcp_gateway {

/// Format a Server-Sent Event string.
///
/// Produces the standard SSE wire format:
///   event: <event_type>\n
///   id: <event_id>\n        (only if event_id is non-empty)
///   data: <data>\n
///   \n
std::string SseWriter::FormatEvent(const std::string& event_type,
                                    const std::string& data,
                                    const std::string& event_id) const {
  std::ostringstream oss;
  oss << "event: " << event_type << "\n";
  if (!event_id.empty()) {
    oss << "id: " << event_id << "\n";
  }
  // Per SSE spec, multi-line data requires each line to be prefixed
  // with "data: ". For JSON (which is single-line when compacted), this
  // is always one line.
  oss << "data: " << data << "\n";
  oss << "\n";
  return oss.str();
}

/// Format a keep-alive SSE comment (prevents connection timeouts).
///
/// SSE comments start with ": " and are ignored by clients.
std::string SseWriter::FormatKeepAlive() const {
  return ": keep-alive\n\n";
}

}  // namespace mchips::mcp_gateway
