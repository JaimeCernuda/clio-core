/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Execution implementation for mchip_cte ChiMod.
 * Implements Container virtual APIs using switch-case dispatch.
 */

#include "mchips/mchip_cte/mchip_cte_runtime.h"
#include "mchips/mchip_cte/autogen/mchip_cte_methods.h"
#include <chimaera/chimaera.h>
#include <chimaera/admin/admin_tasks.h>

namespace mchips::mchip_cte {

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
    case Method::kCreate: {
      co_await Create(task_ptr.template Cast<CreateTask>(), rctx);
      break;
    }
    case Method::kDestroy: {
      co_await Destroy(task_ptr.template Cast<DestroyTask>(), rctx);
      break;
    }
    case Method::kMonitor: {
      // No-op monitor for CTE MChiP
      (void)rctx;
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

chi::TaskResume Runtime::Destroy(hipc::FullPtr<chimaera::admin::DestroyPoolTask> task,
                                 chi::RunContext& rctx) {
  (void)task; (void)rctx;
  co_return;
}

chi::u64 Runtime::GetWorkRemaining() const { return 0; }

void Runtime::SaveTask(chi::u32 method, chi::SaveTaskArchive& archive,
                       hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      archive << *task_ptr.template Cast<CreateTask>().ptr_;
      break;
    }
    case Method::kDestroy: {
      archive << *task_ptr.template Cast<DestroyTask>().ptr_;
      break;
    }
    case Method::kMonitor: {
      archive << *task_ptr.template Cast<MonitorTask>().ptr_;
      break;
    }
    case Method::kListMcpTools: {
      archive << *task_ptr.template Cast<sdk::ListMcpToolsTask>().ptr_;
      break;
    }
    case Method::kCallMcpTool: {
      archive << *task_ptr.template Cast<sdk::CallMcpToolTask>().ptr_;
      break;
    }
    default:
      break;
  }
}

void Runtime::LoadTask(chi::u32 method, chi::LoadTaskArchive& archive,
                       hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      archive >> *task_ptr.template Cast<CreateTask>().ptr_;
      break;
    }
    case Method::kDestroy: {
      archive >> *task_ptr.template Cast<DestroyTask>().ptr_;
      break;
    }
    case Method::kMonitor: {
      archive >> *task_ptr.template Cast<MonitorTask>().ptr_;
      break;
    }
    case Method::kListMcpTools: {
      archive >> *task_ptr.template Cast<sdk::ListMcpToolsTask>().ptr_;
      break;
    }
    case Method::kCallMcpTool: {
      archive >> *task_ptr.template Cast<sdk::CallMcpToolTask>().ptr_;
      break;
    }
    default:
      break;
  }
}

hipc::FullPtr<chi::Task> Runtime::AllocLoadTask(
    chi::u32 method, chi::LoadTaskArchive& archive) {
  hipc::FullPtr<chi::Task> task_ptr = NewTask(method);
  if (!task_ptr.IsNull()) {
    LoadTask(method, archive, task_ptr);
  }
  return task_ptr;
}

void Runtime::LocalLoadTask(chi::u32 method, chi::LocalLoadTaskArchive& archive,
                            hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      archive >> *task_ptr.template Cast<CreateTask>().ptr_;
      break;
    }
    case Method::kDestroy: {
      archive >> *task_ptr.template Cast<DestroyTask>().ptr_;
      break;
    }
    case Method::kMonitor: {
      archive >> *task_ptr.template Cast<MonitorTask>().ptr_;
      break;
    }
    case Method::kListMcpTools: {
      archive >> *task_ptr.template Cast<sdk::ListMcpToolsTask>().ptr_;
      break;
    }
    case Method::kCallMcpTool: {
      archive >> *task_ptr.template Cast<sdk::CallMcpToolTask>().ptr_;
      break;
    }
    default:
      break;
  }
}

hipc::FullPtr<chi::Task> Runtime::LocalAllocLoadTask(
    chi::u32 method, chi::LocalLoadTaskArchive& archive) {
  hipc::FullPtr<chi::Task> task_ptr = NewTask(method);
  if (!task_ptr.IsNull()) {
    LocalLoadTask(method, archive, task_ptr);
  }
  return task_ptr;
}

void Runtime::LocalSaveTask(chi::u32 method, chi::LocalSaveTaskArchive& archive,
                            hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      archive << *task_ptr.template Cast<CreateTask>().ptr_;
      break;
    }
    case Method::kDestroy: {
      archive << *task_ptr.template Cast<DestroyTask>().ptr_;
      break;
    }
    case Method::kMonitor: {
      archive << *task_ptr.template Cast<MonitorTask>().ptr_;
      break;
    }
    case Method::kListMcpTools: {
      archive << *task_ptr.template Cast<sdk::ListMcpToolsTask>().ptr_;
      break;
    }
    case Method::kCallMcpTool: {
      archive << *task_ptr.template Cast<sdk::CallMcpToolTask>().ptr_;
      break;
    }
    default:
      break;
  }
}

hipc::FullPtr<chi::Task> Runtime::NewCopyTask(
    chi::u32 method, hipc::FullPtr<chi::Task> orig_task_ptr, bool deep) {
  auto* ipc_manager = CHI_IPC;
  if (!ipc_manager) return {};

  switch (method) {
    case Method::kCreate: {
      auto t = ipc_manager->NewTask<CreateTask>();
      if (!t.IsNull()) {
        t->Copy(orig_task_ptr.template Cast<CreateTask>());
        return t.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kDestroy: {
      auto t = ipc_manager->NewTask<DestroyTask>();
      if (!t.IsNull()) {
        t->Copy(orig_task_ptr.template Cast<DestroyTask>());
        return t.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kMonitor: {
      auto t = ipc_manager->NewTask<MonitorTask>();
      if (!t.IsNull()) {
        t->Copy(orig_task_ptr.template Cast<MonitorTask>());
        return t.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kListMcpTools: {
      auto t = ipc_manager->NewTask<sdk::ListMcpToolsTask>();
      if (!t.IsNull()) {
        t->Copy(orig_task_ptr.template Cast<sdk::ListMcpToolsTask>());
        return t.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kCallMcpTool: {
      auto t = ipc_manager->NewTask<sdk::CallMcpToolTask>();
      if (!t.IsNull()) {
        t->Copy(orig_task_ptr.template Cast<sdk::CallMcpToolTask>());
        return t.template Cast<chi::Task>();
      }
      break;
    }
    default: {
      auto t = ipc_manager->NewTask<chi::Task>();
      if (!t.IsNull()) {
        t->Copy(orig_task_ptr);
        return t;
      }
      break;
    }
  }
  (void)deep;
  return {};
}

hipc::FullPtr<chi::Task> Runtime::NewTask(chi::u32 method) {
  auto* ipc_manager = CHI_IPC;
  if (!ipc_manager) return {};

  switch (method) {
    case Method::kCreate:
      return ipc_manager->NewTask<CreateTask>().template Cast<chi::Task>();
    case Method::kDestroy:
      return ipc_manager->NewTask<DestroyTask>().template Cast<chi::Task>();
    case Method::kMonitor:
      return ipc_manager->NewTask<MonitorTask>().template Cast<chi::Task>();
    case Method::kListMcpTools:
      return ipc_manager->NewTask<sdk::ListMcpToolsTask>()
          .template Cast<chi::Task>();
    case Method::kCallMcpTool:
      return ipc_manager->NewTask<sdk::CallMcpToolTask>()
          .template Cast<chi::Task>();
    default:
      return {};
  }
}

void Runtime::Aggregate(chi::u32 method, hipc::FullPtr<chi::Task> orig_task,
                        const hipc::FullPtr<chi::Task>& replica_task) {
  switch (method) {
    case Method::kCreate:
      orig_task.template Cast<CreateTask>()->Aggregate(replica_task);
      break;
    case Method::kDestroy:
      orig_task.template Cast<DestroyTask>()->Aggregate(replica_task);
      break;
    case Method::kMonitor:
      orig_task.template Cast<MonitorTask>()->Aggregate(replica_task);
      break;
    case Method::kListMcpTools:
      orig_task.template Cast<sdk::ListMcpToolsTask>()->Aggregate(replica_task);
      break;
    case Method::kCallMcpTool:
      orig_task.template Cast<sdk::CallMcpToolTask>()->Aggregate(replica_task);
      break;
    default:
      orig_task->Aggregate(replica_task);
      break;
  }
}

void Runtime::DelTask(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) {
  auto* ipc_manager = CHI_IPC;
  if (!ipc_manager) return;
  switch (method) {
    case Method::kCreate:
      ipc_manager->DelTask(task_ptr.template Cast<CreateTask>());
      break;
    case Method::kDestroy:
      ipc_manager->DelTask(task_ptr.template Cast<DestroyTask>());
      break;
    case Method::kMonitor:
      ipc_manager->DelTask(task_ptr.template Cast<MonitorTask>());
      break;
    case Method::kListMcpTools:
      ipc_manager->DelTask(task_ptr.template Cast<sdk::ListMcpToolsTask>());
      break;
    case Method::kCallMcpTool:
      ipc_manager->DelTask(task_ptr.template Cast<sdk::CallMcpToolTask>());
      break;
    default:
      ipc_manager->DelTask(task_ptr);
      break;
  }
}

// RegisterTools() is implemented in mchip_cte_runtime.cc

}  // namespace mchips::mchip_cte

CHI_TASK_CC(mchips::mchip_cte::Runtime)
