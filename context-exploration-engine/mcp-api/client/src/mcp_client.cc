/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mchips/client/mcp_client.h>

namespace mchips::client {

McpClient::McpClient(const McpClientConfig& config) : config_(config) {}

McpClient::~McpClient() = default;

// TODO(Phase 2): Implement Initialize, ListTools, CallTool, Close

}  // namespace mchips::client
