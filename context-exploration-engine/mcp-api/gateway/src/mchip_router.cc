/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mcp_gateway/mchip_router.h"

// MchipRouter core functionality (routing, discovery) is implemented as
// inline methods in the header since they only manipulate the routes_ map
// and forward to MchipClient.
//
// RefreshTools() requires Chimaera co_await — implemented in
// gateway_runtime.cc within the Create() coroutine where the RunContext
// is available.

namespace mchips::mcp_gateway {
}  // namespace mchips::mcp_gateway
