/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mchips/client/mcp_transport.h>

namespace mchips::client {

McpTransport::McpTransport(const std::string& base_url)
    : base_url_(base_url) {}

McpTransport::~McpTransport() = default;

// TODO(Phase 2): Implement Streamable HTTP transport

}  // namespace mchips::client
