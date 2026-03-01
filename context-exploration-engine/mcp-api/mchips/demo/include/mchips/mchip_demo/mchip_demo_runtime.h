/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCHIP_DEMO_RUNTIME_H_
#define MCHIPS_MCHIP_DEMO_RUNTIME_H_

#include <mchips/mchip_demo/mchip_demo_client.h>
#include <mchips/mchip_demo/mchip_demo_tasks.h>
#include <mchips/sdk/mchip_base.h>

namespace mchips::mchip_demo {

/// Demo MChiP runtime — simple echo/add tools for testing and benchmarking.
///
/// Tools:
///   echo — Returns the input message unchanged
///   add  — Adds two numbers and returns the sum
class Runtime : public sdk::MchipBase {
 public:
  using CreateParams = mchips::mchip_demo::CreateParams;

  Runtime() = default;
  ~Runtime() override = default;

  void Init(const chi::PoolId& pool_id, const std::string& pool_name,
            chi::u32 container_id = 0) override;

  chi::TaskResume Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                      chi::RunContext& rctx) override;

  chi::TaskResume Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx);
  chi::TaskResume Destroy(hipc::FullPtr<chimaera::admin::DestroyPoolTask> task,
                          chi::RunContext& rctx);

  chi::u64 GetWorkRemaining() const override;

  void SaveTask(chi::u32 method, chi::SaveTaskArchive& archive,
                hipc::FullPtr<chi::Task> task_ptr) override;
  void LoadTask(chi::u32 method, chi::LoadTaskArchive& archive,
                hipc::FullPtr<chi::Task> task_ptr) override;
  hipc::FullPtr<chi::Task> AllocLoadTask(
      chi::u32 method, chi::LoadTaskArchive& archive) override;
  void LocalLoadTask(chi::u32 method, chi::LocalLoadTaskArchive& archive,
                     hipc::FullPtr<chi::Task> task_ptr) override;
  hipc::FullPtr<chi::Task> LocalAllocLoadTask(
      chi::u32 method, chi::LocalLoadTaskArchive& archive) override;
  void LocalSaveTask(chi::u32 method, chi::LocalSaveTaskArchive& archive,
                     hipc::FullPtr<chi::Task> task_ptr) override;
  hipc::FullPtr<chi::Task> NewCopyTask(
      chi::u32 method, hipc::FullPtr<chi::Task> orig_task_ptr,
      bool deep) override;
  hipc::FullPtr<chi::Task> NewTask(chi::u32 method) override;
  void Aggregate(chi::u32 method, hipc::FullPtr<chi::Task> orig_task,
                 const hipc::FullPtr<chi::Task>& replica_task) override;
  void DelTask(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) override;

 protected:
  void RegisterTools() override;

 private:
  Client client_;

  protocol::json HandleEcho(const protocol::json& args);
  protocol::json HandleAdd(const protocol::json& args);
};

}  // namespace mchips::mchip_demo

#endif  // MCHIPS_MCHIP_DEMO_RUNTIME_H_
