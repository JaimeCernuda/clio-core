/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Auto-generated execution for mchip_demo ChiMod.
 */

#include "mchips/mchip_demo/mchip_demo_runtime.h"
#include "mchips/mchip_demo/autogen/mchip_demo_methods.h"

namespace mchips::mchip_demo {

void Runtime::Init(const chi::PoolId& pool_id, const std::string& pool_name,
                   chi::u32 container_id) {
  chi::Container::Init(pool_id, pool_name, container_id);
  client_ = Client(pool_id);
}

chi::TaskResume Runtime::Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                             chi::RunContext& rctx) {
  switch (method) {
    case Method::kCreate:
      co_await Create(task_ptr.template Cast<CreateTask>(), rctx);
      break;
    case Method::kListMcpTools:
      co_await ListMcpTools(
          task_ptr.template Cast<sdk::ListMcpToolsTask>(), rctx);
      break;
    case Method::kCallMcpTool:
      co_await CallMcpTool(
          task_ptr.template Cast<sdk::CallMcpToolTask>(), rctx);
      break;
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

chi::TaskResume Runtime::Destroy(hipc::FullPtr<chimaera::admin::DestroyPoolTask> task,
                                 chi::RunContext& rctx) {
  (void)task; (void)rctx;
  co_return;
}

chi::u64 Runtime::GetWorkRemaining() const { return 0; }

void Runtime::SaveTask(chi::u32 m, chi::SaveTaskArchive& a, hipc::FullPtr<chi::Task> t) { (void)m; (void)a; (void)t; }
void Runtime::LoadTask(chi::u32 m, chi::LoadTaskArchive& a, hipc::FullPtr<chi::Task> t) { (void)m; (void)a; (void)t; }
hipc::FullPtr<chi::Task> Runtime::AllocLoadTask(chi::u32 m, chi::LoadTaskArchive& a) { (void)m; (void)a; return {}; }
void Runtime::LocalLoadTask(chi::u32 m, chi::LocalLoadTaskArchive& a, hipc::FullPtr<chi::Task> t) { (void)m; (void)a; (void)t; }
hipc::FullPtr<chi::Task> Runtime::LocalAllocLoadTask(chi::u32 m, chi::LocalLoadTaskArchive& a) { (void)m; (void)a; return {}; }
void Runtime::LocalSaveTask(chi::u32 m, chi::LocalSaveTaskArchive& a, hipc::FullPtr<chi::Task> t) { (void)m; (void)a; (void)t; }
hipc::FullPtr<chi::Task> Runtime::NewCopyTask(chi::u32 m, hipc::FullPtr<chi::Task> o, bool d) { (void)m; (void)o; (void)d; return {}; }
hipc::FullPtr<chi::Task> Runtime::NewTask(chi::u32 m) { (void)m; return {}; }
void Runtime::Aggregate(chi::u32 m, hipc::FullPtr<chi::Task> o, const hipc::FullPtr<chi::Task>& r) { (void)m; (void)o; (void)r; }
void Runtime::DelTask(chi::u32 m, hipc::FullPtr<chi::Task> t) { (void)m; (void)t; }

}  // namespace mchips::mchip_demo

CHI_TASK_CC(mchips::mchip_demo::Runtime)
