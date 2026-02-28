/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_SDK_MCHIP_CLIENT_H_
#define MCHIPS_SDK_MCHIP_CLIENT_H_

#include <chimaera/container_client.h>

#include <mchips/sdk/mchip_tasks.h>

namespace mchips::sdk {

/// Generic client for calling any MChiP ChiMod.
///
/// The gateway uses one MchipClient per discovered MChiP pool to
/// send ListMcpTools and CallMcpTool tasks. The client is initialized
/// with the target MChiP's pool_id.
///
/// Example (in the gateway's MchipRouter):
/// ```cpp
/// MchipClient cte_client(cte_pool_id);
/// auto future = cte_client.AsyncCallMcpTool(
///     chi::PoolQuery::Local(), "put_blob", args_json);
/// co_await future;
/// auto result = std::string(future->result_json_.c_str());
/// ```
class MchipClient : public chi::ContainerClient {
 public:
  MchipClient() = default;

  explicit MchipClient(const chi::PoolId& pool_id) { Init(pool_id); }

  /// Request the list of tools from a MChiP (async).
  chi::Future<ListMcpToolsTask> AsyncListMcpTools(
      const chi::PoolQuery& pool_query) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<ListMcpToolsTask>(
        chi::CreateTaskId(), pool_id_, pool_query);
    return ipc_manager->Send(task);
  }

  /// Invoke a tool on a MChiP (async).
  ///
  /// @param pool_query Routing information
  /// @param tool_name  Local tool name (without MChiP prefix)
  /// @param args_json  JSON string of tool arguments
  chi::Future<CallMcpToolTask> AsyncCallMcpTool(
      const chi::PoolQuery& pool_query,
      const std::string& tool_name,
      const std::string& args_json) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<CallMcpToolTask>(
        chi::CreateTaskId(), pool_id_, pool_query,
        tool_name, args_json);
    return ipc_manager->Send(task);
  }
};

}  // namespace mchips::sdk

#endif  // MCHIPS_SDK_MCHIP_CLIENT_H_
