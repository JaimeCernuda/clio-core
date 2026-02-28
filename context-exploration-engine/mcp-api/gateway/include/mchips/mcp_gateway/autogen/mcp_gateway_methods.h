/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Auto-generated method definitions for mcp_gateway ChiMod.
 * Corresponds to chimaera_mod.yaml method IDs.
 */

#ifndef MCHIPS_MCP_GATEWAY_AUTOGEN_METHODS_H_
#define MCHIPS_MCP_GATEWAY_AUTOGEN_METHODS_H_

#include <chimaera/chimaera.h>

namespace mchips::mcp_gateway {

namespace Method {
// Inherited methods
GLOBAL_CONST chi::u32 kCreate = 0;
GLOBAL_CONST chi::u32 kDestroy = 1;
GLOBAL_CONST chi::u32 kMonitor = 9;

// Gateway-specific methods
GLOBAL_CONST chi::u32 kHandleHttpRequest = 10;
GLOBAL_CONST chi::u32 kInitializeSession = 11;
GLOBAL_CONST chi::u32 kCloseSession = 12;
GLOBAL_CONST chi::u32 kStartHttpServer = 13;
GLOBAL_CONST chi::u32 kStopHttpServer = 14;
}  // namespace Method

}  // namespace mchips::mcp_gateway

#endif  // MCHIPS_MCP_GATEWAY_AUTOGEN_METHODS_H_
