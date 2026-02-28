/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * AUTO-GENERATED from chimaera_mod.yaml — do not edit manually.
 * TODO(Phase 3): Generate from chimaera_mod.yaml via CMake codegen.
 */

#ifndef MCHIPS_MCP_AUTOGEN_MCP_METHODS_H_
#define MCHIPS_MCP_AUTOGEN_MCP_METHODS_H_

namespace mchips::mcp {

/// Method IDs matching chimaera_mod.yaml
enum McpMethod : int {
  kCreate = 0,
  kDestroy = 1,
  kMonitor = 9,
  kHandleHttpRequest = 10,
  kCallTool = 11,
  kListTools = 12,
  kInitializeSession = 13,
  kCloseSession = 14,
  kStartHttpServer = 15,
  kStopHttpServer = 16,
};

}  // namespace mchips::mcp

#endif  // MCHIPS_MCP_AUTOGEN_MCP_METHODS_H_
