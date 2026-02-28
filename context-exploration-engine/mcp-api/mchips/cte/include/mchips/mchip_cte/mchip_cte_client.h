/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCHIP_CTE_CLIENT_H_
#define MCHIPS_MCHIP_CTE_CLIENT_H_

#include <chimaera/container_client.h>

#include <mchips/mchip_cte/mchip_cte_tasks.h>
#include <mchips/sdk/mchip_client.h>

namespace mchips::mchip_cte {

/// Client for the CTE MChiP ChiMod.
///
/// Extends the generic MchipClient with CTE-specific Create().
/// The gateway uses MchipClient directly for ListMcpTools/CallMcpTool;
/// this client is for direct CTE MChiP lifecycle management.
class Client : public chi::ContainerClient {
 public:
  Client() = default;
  explicit Client(const chi::PoolId& pool_id) { Init(pool_id); }

  /// Create the CTE MChiP pool (async).
  chi::Future<CreateTask> AsyncCreate(const chi::PoolQuery& pool_query,
                                      const std::string& pool_name,
                                      const chi::PoolId& custom_pool_id) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<CreateTask>(
        chi::CreateTaskId(), chi::kAdminPoolId, pool_query,
        CreateParams::chimod_lib_name, pool_name, custom_pool_id, this);
    return ipc_manager->Send(task);
  }
};

}  // namespace mchips::mchip_cte

#endif  // MCHIPS_MCHIP_CTE_CLIENT_H_
