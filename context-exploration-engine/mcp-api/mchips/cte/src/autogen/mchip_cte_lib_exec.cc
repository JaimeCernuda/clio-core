/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Auto-generated execution implementation for mchip_cte ChiMod.
 * TODO(Phase B.5): Implement full dispatch.
 */

#include "mchips/mchip_cte/mchip_cte_runtime.h"
#include "mchips/mchip_cte/autogen/mchip_cte_methods.h"

namespace mchips::mchip_cte {

void Runtime::Init(const chi::PoolId& pool_id, const std::string& pool_name,
                   chi::u32 container_id) {
  chi::Container::Init(pool_id, pool_name, container_id);
  client_ = Client(pool_id);
}

chi::TaskResume Runtime::Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                             chi::RunContext& rctx) {
  switch (method) {
    case Method::kCreate: {
      co_await Create(task_ptr.template Cast<CreateTask>(), rctx);
      break;
    }
    case Method::kListMcpTools: {
      co_await ListMcpTools(
          task_ptr.template Cast<sdk::ListMcpToolsTask>(), rctx);
      break;
    }
    case Method::kCallMcpTool: {
      co_await CallMcpTool(
          task_ptr.template Cast<sdk::CallMcpToolTask>(), rctx);
      break;
    }
    default:
      break;
  }
  co_return;
}

chi::TaskResume Runtime::Create(hipc::FullPtr<CreateTask> task,
                                chi::RunContext& rctx) {
  RegisterTools();
  (void)task; (void)rctx;
  co_return;
}

chi::TaskResume Runtime::Destroy(hipc::FullPtr<chi::admin::DestroyPoolTask> task,
                                 chi::RunContext& rctx) {
  (void)task; (void)rctx;
  co_return;
}

chi::u64 Runtime::GetWorkRemaining() const { return 0; }

// TODO(Phase B.5): Implement serialization stubs
void Runtime::SaveTask(chi::u32 method, chi::SaveTaskArchive& archive,
                       hipc::FullPtr<chi::Task> task_ptr) {
  (void)method; (void)archive; (void)task_ptr;
}
void Runtime::LoadTask(chi::u32 method, chi::LoadTaskArchive& archive,
                       hipc::FullPtr<chi::Task> task_ptr) {
  (void)method; (void)archive; (void)task_ptr;
}
hipc::FullPtr<chi::Task> Runtime::AllocLoadTask(
    chi::u32 method, chi::LoadTaskArchive& archive) {
  (void)method; (void)archive; return {};
}
void Runtime::LocalLoadTask(chi::u32 method, chi::LocalLoadTaskArchive& archive,
                            hipc::FullPtr<chi::Task> task_ptr) {
  (void)method; (void)archive; (void)task_ptr;
}
hipc::FullPtr<chi::Task> Runtime::LocalAllocLoadTask(
    chi::u32 method, chi::LocalLoadTaskArchive& archive) {
  (void)method; (void)archive; return {};
}
void Runtime::LocalSaveTask(chi::u32 method, chi::LocalSaveTaskArchive& archive,
                            hipc::FullPtr<chi::Task> task_ptr) {
  (void)method; (void)archive; (void)task_ptr;
}
hipc::FullPtr<chi::Task> Runtime::NewCopyTask(
    chi::u32 method, hipc::FullPtr<chi::Task> orig_task_ptr, bool deep) {
  (void)method; (void)orig_task_ptr; (void)deep; return {};
}
hipc::FullPtr<chi::Task> Runtime::NewTask(chi::u32 method) {
  (void)method; return {};
}
void Runtime::Aggregate(chi::u32 method, hipc::FullPtr<chi::Task> orig_task,
                        const hipc::FullPtr<chi::Task>& replica_task) {
  (void)method; (void)orig_task; (void)replica_task;
}
void Runtime::DelTask(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) {
  (void)method; (void)task_ptr;
}

// RegisterTools() is implemented in mchip_cte_runtime.cc

}  // namespace mchips::mchip_cte

CHI_TASK_CC(mchips::mchip_cte::Runtime)
