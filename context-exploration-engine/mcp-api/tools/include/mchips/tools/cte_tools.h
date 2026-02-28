/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_TOOLS_CTE_TOOLS_H_
#define MCHIPS_TOOLS_CTE_TOOLS_H_

#include <mchips/protocol/mcp_types.h>
#include <mchips/protocol/schema_generator.h>

#include <vector>

namespace mchips::tools {

/// Register all 12 CTE tools into a ToolRegistry.
///
/// Tools (from branch 57-context-interface-mcp Python server):
///   put_blob            — Store a blob in CTE
///   get_blob            — Retrieve blob data
///   get_blob_size       — Get blob size in bytes
///   list_blobs_in_tag   — List all blobs under a tag
///   delete_blob         — Delete a blob
///   tag_query           — Query tags by regex
///   blob_query          — Query blobs by regex
///   poll_telemetry_log  — Get CTE telemetry data
///   reorganize_blob     — Trigger blob reorganization
///   initialize_cte_runtime — Initialize CTE runtime
///   get_client_status   — Check CTE client status
///   get_cte_types       — Return CTE type metadata

/// Get ToolDefinitions for all CTE tools.
std::vector<protocol::ToolDefinition> GetCteToolDefinitions();

// TODO(Phase 4): Handler functions
// protocol::CallToolResult HandlePutBlob(const protocol::json& args);
// protocol::CallToolResult HandleGetBlob(const protocol::json& args);
// protocol::CallToolResult HandleGetBlobSize(const protocol::json& args);
// protocol::CallToolResult HandleListBlobsInTag(const protocol::json& args);
// protocol::CallToolResult HandleDeleteBlob(const protocol::json& args);
// protocol::CallToolResult HandleTagQuery(const protocol::json& args);
// protocol::CallToolResult HandleBlobQuery(const protocol::json& args);
// protocol::CallToolResult HandlePollTelemetryLog(const protocol::json& args);
// protocol::CallToolResult HandleReorganizeBlob(const protocol::json& args);
// protocol::CallToolResult HandleInitializeCteRuntime(const protocol::json& args);
// protocol::CallToolResult HandleGetClientStatus(const protocol::json& args);
// protocol::CallToolResult HandleGetCteTypes(const protocol::json& args);

}  // namespace mchips::tools

#endif  // MCHIPS_TOOLS_CTE_TOOLS_H_
