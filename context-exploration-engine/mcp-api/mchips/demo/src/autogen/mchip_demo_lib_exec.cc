/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Execution implementation for mchip_demo ChiMod.
 * Implements Container virtual APIs using switch-case dispatch.
 */

#include "mchips/mchip_demo/mchip_demo_runtime.h"
#include "mchips/mchip_demo/autogen/mchip_demo_methods.h"
#include <chimaera/chimaera.h>
#include <chimaera/admin/admin_tasks.h>

namespace mchips::mchip_demo {

using DestroyTask = chimaera::admin::DestroyPoolTask;
using MonitorTask = chimaera::admin::MonitorTask;

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
    case Method::kDestroy:
      co_await Destroy(task_ptr.template Cast<DestroyTask>(), rctx);
      break;
    case Method::kMonitor:
      (void)rctx;
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

void Runtime::SaveTask(chi::u32 method, chi::SaveTaskArchive& archive,
                       hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate:
      archive << *task_ptr.template Cast<CreateTask>().ptr_;
      break;
    case Method::kDestroy:
      archive << *task_ptr.template Cast<DestroyTask>().ptr_;
      break;
    case Method::kMonitor:
      archive << *task_ptr.template Cast<MonitorTask>().ptr_;
      break;
    case Method::kListMcpTools:
      archive << *task_ptr.template Cast<sdk::ListMcpToolsTask>().ptr_;
      break;
    case Method::kCallMcpTool:
      archive << *task_ptr.template Cast<sdk::CallMcpToolTask>().ptr_;
      break;
    default:
      break;
  }
}

void Runtime::LoadTask(chi::u32 method, chi::LoadTaskArchive& archive,
                       hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate:
      archive >> *task_ptr.template Cast<CreateTask>().ptr_;
      break;
    case Method::kDestroy:
      archive >> *task_ptr.template Cast<DestroyTask>().ptr_;
      break;
    case Method::kMonitor:
      archive >> *task_ptr.template Cast<MonitorTask>().ptr_;
      break;
    case Method::kListMcpTools:
      archive >> *task_ptr.template Cast<sdk::ListMcpToolsTask>().ptr_;
      break;
    case Method::kCallMcpTool:
      archive >> *task_ptr.template Cast<sdk::CallMcpToolTask>().ptr_;
      break;
    default:
      break;
  }
}

hipc::FullPtr<chi::Task> Runtime::AllocLoadTask(
    chi::u32 method, chi::LoadTaskArchive& archive) {
  auto task_ptr = NewTask(method);
  if (!task_ptr.IsNull()) LoadTask(method, archive, task_ptr);
  return task_ptr;
}

void Runtime::LocalLoadTask(chi::u32 method, chi::LocalLoadTaskArchive& archive,
                            hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate:
      archive >> *task_ptr.template Cast<CreateTask>().ptr_;
      break;
    case Method::kDestroy:
      archive >> *task_ptr.template Cast<DestroyTask>().ptr_;
      break;
    case Method::kMonitor:
      archive >> *task_ptr.template Cast<MonitorTask>().ptr_;
      break;
    case Method::kListMcpTools:
      archive >> *task_ptr.template Cast<sdk::ListMcpToolsTask>().ptr_;
      break;
    case Method::kCallMcpTool:
      archive >> *task_ptr.template Cast<sdk::CallMcpToolTask>().ptr_;
      break;
    default:
      break;
  }
}

hipc::FullPtr<chi::Task> Runtime::LocalAllocLoadTask(
    chi::u32 method, chi::LocalLoadTaskArchive& archive) {
  auto task_ptr = NewTask(method);
  if (!task_ptr.IsNull()) LocalLoadTask(method, archive, task_ptr);
  return task_ptr;
}

void Runtime::LocalSaveTask(chi::u32 method, chi::LocalSaveTaskArchive& archive,
                            hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate:
      archive << *task_ptr.template Cast<CreateTask>().ptr_;
      break;
    case Method::kDestroy:
      archive << *task_ptr.template Cast<DestroyTask>().ptr_;
      break;
    case Method::kMonitor:
      archive << *task_ptr.template Cast<MonitorTask>().ptr_;
      break;
    case Method::kListMcpTools:
      archive << *task_ptr.template Cast<sdk::ListMcpToolsTask>().ptr_;
      break;
    case Method::kCallMcpTool:
      archive << *task_ptr.template Cast<sdk::CallMcpToolTask>().ptr_;
      break;
    default:
      break;
  }
}

hipc::FullPtr<chi::Task> Runtime::NewCopyTask(
    chi::u32 method, hipc::FullPtr<chi::Task> orig, bool deep) {
  auto* ipc = CHI_IPC;
  if (!ipc) return {};
  switch (method) {
    case Method::kCreate: {
      auto t = ipc->NewTask<CreateTask>();
      if (!t.IsNull()) { t->Copy(orig.template Cast<CreateTask>()); return t.template Cast<chi::Task>(); }
      break;
    }
    case Method::kDestroy: {
      auto t = ipc->NewTask<DestroyTask>();
      if (!t.IsNull()) { t->Copy(orig.template Cast<DestroyTask>()); return t.template Cast<chi::Task>(); }
      break;
    }
    case Method::kMonitor: {
      auto t = ipc->NewTask<MonitorTask>();
      if (!t.IsNull()) { t->Copy(orig.template Cast<MonitorTask>()); return t.template Cast<chi::Task>(); }
      break;
    }
    case Method::kListMcpTools: {
      auto t = ipc->NewTask<sdk::ListMcpToolsTask>();
      if (!t.IsNull()) { t->Copy(orig.template Cast<sdk::ListMcpToolsTask>()); return t.template Cast<chi::Task>(); }
      break;
    }
    case Method::kCallMcpTool: {
      auto t = ipc->NewTask<sdk::CallMcpToolTask>();
      if (!t.IsNull()) { t->Copy(orig.template Cast<sdk::CallMcpToolTask>()); return t.template Cast<chi::Task>(); }
      break;
    }
    default: {
      auto t = ipc->NewTask<chi::Task>();
      if (!t.IsNull()) { t->Copy(orig); return t; }
      break;
    }
  }
  (void)deep;
  return {};
}

hipc::FullPtr<chi::Task> Runtime::NewTask(chi::u32 method) {
  auto* ipc = CHI_IPC;
  if (!ipc) return {};
  switch (method) {
    case Method::kCreate:
      return ipc->NewTask<CreateTask>().template Cast<chi::Task>();
    case Method::kDestroy:
      return ipc->NewTask<DestroyTask>().template Cast<chi::Task>();
    case Method::kMonitor:
      return ipc->NewTask<MonitorTask>().template Cast<chi::Task>();
    case Method::kListMcpTools:
      return ipc->NewTask<sdk::ListMcpToolsTask>().template Cast<chi::Task>();
    case Method::kCallMcpTool:
      return ipc->NewTask<sdk::CallMcpToolTask>().template Cast<chi::Task>();
    default:
      return {};
  }
}

void Runtime::Aggregate(chi::u32 method, hipc::FullPtr<chi::Task> orig,
                        const hipc::FullPtr<chi::Task>& replica) {
  switch (method) {
    case Method::kCreate:
      orig.template Cast<CreateTask>()->Aggregate(replica); break;
    case Method::kDestroy:
      orig.template Cast<DestroyTask>()->Aggregate(replica); break;
    case Method::kMonitor:
      orig.template Cast<MonitorTask>()->Aggregate(replica); break;
    case Method::kListMcpTools:
      orig.template Cast<sdk::ListMcpToolsTask>()->Aggregate(replica); break;
    case Method::kCallMcpTool:
      orig.template Cast<sdk::CallMcpToolTask>()->Aggregate(replica); break;
    default:
      orig->Aggregate(replica); break;
  }
}

void Runtime::DelTask(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) {
  auto* ipc = CHI_IPC;
  if (!ipc) return;
  switch (method) {
    case Method::kCreate:
      ipc->DelTask(task_ptr.template Cast<CreateTask>()); break;
    case Method::kDestroy:
      ipc->DelTask(task_ptr.template Cast<DestroyTask>()); break;
    case Method::kMonitor:
      ipc->DelTask(task_ptr.template Cast<MonitorTask>()); break;
    case Method::kListMcpTools:
      ipc->DelTask(task_ptr.template Cast<sdk::ListMcpToolsTask>()); break;
    case Method::kCallMcpTool:
      ipc->DelTask(task_ptr.template Cast<sdk::CallMcpToolTask>()); break;
    default:
      ipc->DelTask(task_ptr); break;
  }
}

}  // namespace mchips::mchip_demo

CHI_TASK_CC(mchips::mchip_demo::Runtime)
