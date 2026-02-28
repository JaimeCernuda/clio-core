/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_TOOLS_CAE_TOOLS_H_
#define MCHIPS_TOOLS_CAE_TOOLS_H_

#include <mchips/protocol/mcp_types.h>

#include <vector>

namespace mchips::tools {

/// Get ToolDefinitions for CAE assimilation tools.
///
/// CAE tools expose data format transformation capabilities:
///   assimilate — ingest data in various formats (HDF5, NetCDF, Zarr, etc.)
///   list_formats — list supported assimilation formats
std::vector<protocol::ToolDefinition> GetCaeToolDefinitions();

// TODO(Phase 4): Handler functions
// protocol::CallToolResult HandleAssimilate(const protocol::json& args);
// protocol::CallToolResult HandleListFormats(const protocol::json& args);

}  // namespace mchips::tools

#endif  // MCHIPS_TOOLS_CAE_TOOLS_H_
