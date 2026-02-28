/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_MCP_TASKS_H_
#define MCHIPS_MCP_MCP_TASKS_H_

// TODO(Phase 3): Include Chimaera task base headers
// #include <chimaera/task.h>
// #include <mchips/mcp/autogen/mcp_methods.h>

namespace mchips::mcp {

// Forward declarations — task types for the MCP server ChiMod
//
// Each task maps to a method ID in chimaera_mod.yaml:
//   kHandleHttpRequest (10) — process an incoming HTTP request
//   kCallTool (11)          — invoke a registered tool by name
//   kListTools (12)         — return available tools
//   kInitializeSession (13) — MCP initialize handshake
//   kCloseSession (14)      — clean up session
//   kStartHttpServer (15)   — start HTTP listener
//   kStopHttpServer (16)    — stop HTTP listener

class HandleHttpRequestTask;
class CallToolTask;
class ListToolsTask;
class InitializeSessionTask;
class CloseSessionTask;
class StartHttpServerTask;
class StopHttpServerTask;

}  // namespace mchips::mcp

#endif  // MCHIPS_MCP_MCP_TASKS_H_
