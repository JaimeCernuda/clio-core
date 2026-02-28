/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCHIP_CTE_TASKS_H_
#define MCHIPS_MCHIP_CTE_TASKS_H_

#include <chimaera/admin/admin_tasks.h>

#include <mchips/mchip_cte/autogen/mchip_cte_methods.h>
#include <mchips/sdk/mchip_tasks.h>

namespace mchips::mchip_cte {

/// Parameters for creating the CTE MChiP pool.
struct CreateParams {
  static constexpr const char* chimod_lib_name = "mchips_mchip_cte";

  CreateParams() = default;

  template <class Archive>
  void serialize(Archive& ar) {
    (void)ar;
  }
};

/// CreateTask for the CTE MChiP pool.
using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;

// ListMcpToolsTask and CallMcpToolTask are defined in mchips::sdk
// and reused by all MChiP ChiMods. No CTE-specific task types needed.

}  // namespace mchips::mchip_cte

#endif  // MCHIPS_MCHIP_CTE_TASKS_H_
