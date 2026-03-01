/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCHIP_DEMO_TASKS_H_
#define MCHIPS_MCHIP_DEMO_TASKS_H_

#include <chimaera/admin/admin_tasks.h>

#include <mchips/mchip_demo/autogen/mchip_demo_methods.h>
#include <mchips/sdk/mchip_tasks.h>

namespace mchips::mchip_demo {

struct CreateParams {
  static constexpr const char* chimod_lib_name = "mchips_mchip_demo";
  CreateParams() = default;
  void LoadConfig(const chi::PoolConfig& /*pool_config*/) {}
  template <class Archive>
  void serialize(Archive& ar) { (void)ar; }
};

using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;

}  // namespace mchips::mchip_demo

#endif  // MCHIPS_MCHIP_DEMO_TASKS_H_
