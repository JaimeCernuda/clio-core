/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mcp_gateway/mchip_router.h"

// TODO(Phase B.4): Implement MchipRouter discovery and refresh
//
// RefreshTools():
//   - For each registered MChiP route:
//     - Send AsyncListMcpTools via MchipClient
//     - co_await the response
//     - Parse JSON array of ToolDefinitions
//     - Update route.tools cache
//   - Rebuild qualified name index
//
// Called on gateway Create() and when MChiPs send
// notifications/tools/list_changed.

namespace mchips::mcp_gateway {
}  // namespace mchips::mcp_gateway
