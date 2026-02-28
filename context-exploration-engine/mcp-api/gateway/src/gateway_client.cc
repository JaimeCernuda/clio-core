/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mcp_gateway/gateway_client.h"
#include "mchips/mcp_gateway/gateway_tasks.h"

namespace mchips::mcp_gateway {

// Define static constexpr member for proper linkage when address is taken
constexpr const char* CreateParams::chimod_lib_name;

}  // namespace mchips::mcp_gateway
