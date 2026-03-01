/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Execution implementation for mcp_gateway ChiMod.
 * Implements Container virtual APIs (Run, SaveTask, LoadTask, etc.)
 * using switch-case dispatch over method IDs.
 */

#include "mchips/mcp_gateway/gateway_runtime.h"
#include "mchips/mcp_gateway/autogen/mcp_gateway_methods.h"
#include <chimaera/chimaera.h>
#include <chimaera/admin/admin_tasks.h>

namespace mchips::mcp_gateway {

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
      co_await Monitor(task_ptr.template Cast<MonitorTask>(), rctx);
      break;
    }
    case Method::kHandleHttpRequest: {
      co_await HandleHttpRequest(
          task_ptr.template Cast<HandleHttpRequestTask>(), rctx);
      break;
    }
    case Method::kInitializeSession: {
      co_await InitializeSession(
          task_ptr.template Cast<InitializeSessionTask>(), rctx);
      break;
    }
    case Method::kCloseSession: {
      co_await CloseSession(
          task_ptr.template Cast<CloseSessionTask>(), rctx);
      break;
    }
    case Method::kStartHttpServer: {
      co_await StartHttpServer(
          task_ptr.template Cast<StartHttpServerTask>(), rctx);
      break;
    }
    case Method::kStopHttpServer: {
      co_await StopHttpServer(
          task_ptr.template Cast<StopHttpServerTask>(), rctx);
      break;
    }
    default:
      break;
  }
  co_return;
}

chi::u64 Runtime::GetWorkRemaining() const { return 0; }

void Runtime::SaveTask(chi::u32 method, chi::SaveTaskArchive& archive,
                       hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto typed = task_ptr.template Cast<CreateTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kDestroy: {
      auto typed = task_ptr.template Cast<DestroyTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kMonitor: {
      auto typed = task_ptr.template Cast<MonitorTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kHandleHttpRequest: {
      auto typed = task_ptr.template Cast<HandleHttpRequestTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kInitializeSession: {
      auto typed = task_ptr.template Cast<InitializeSessionTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kCloseSession: {
      auto typed = task_ptr.template Cast<CloseSessionTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kStartHttpServer: {
      auto typed = task_ptr.template Cast<StartHttpServerTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kStopHttpServer: {
      auto typed = task_ptr.template Cast<StopHttpServerTask>();
      archive << *typed.ptr_;
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
      auto typed = task_ptr.template Cast<CreateTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kDestroy: {
      auto typed = task_ptr.template Cast<DestroyTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kMonitor: {
      auto typed = task_ptr.template Cast<MonitorTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kHandleHttpRequest: {
      auto typed = task_ptr.template Cast<HandleHttpRequestTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kInitializeSession: {
      auto typed = task_ptr.template Cast<InitializeSessionTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kCloseSession: {
      auto typed = task_ptr.template Cast<CloseSessionTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kStartHttpServer: {
      auto typed = task_ptr.template Cast<StartHttpServerTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kStopHttpServer: {
      auto typed = task_ptr.template Cast<StopHttpServerTask>();
      archive >> *typed.ptr_;
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

void Runtime::LocalLoadTask(chi::u32 method,
                            chi::LocalLoadTaskArchive& archive,
                            hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto typed = task_ptr.template Cast<CreateTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kDestroy: {
      auto typed = task_ptr.template Cast<DestroyTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kMonitor: {
      auto typed = task_ptr.template Cast<MonitorTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kHandleHttpRequest: {
      auto typed = task_ptr.template Cast<HandleHttpRequestTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kInitializeSession: {
      auto typed = task_ptr.template Cast<InitializeSessionTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kCloseSession: {
      auto typed = task_ptr.template Cast<CloseSessionTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kStartHttpServer: {
      auto typed = task_ptr.template Cast<StartHttpServerTask>();
      archive >> *typed.ptr_;
      break;
    }
    case Method::kStopHttpServer: {
      auto typed = task_ptr.template Cast<StopHttpServerTask>();
      archive >> *typed.ptr_;
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

void Runtime::LocalSaveTask(chi::u32 method,
                            chi::LocalSaveTaskArchive& archive,
                            hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto typed = task_ptr.template Cast<CreateTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kDestroy: {
      auto typed = task_ptr.template Cast<DestroyTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kMonitor: {
      auto typed = task_ptr.template Cast<MonitorTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kHandleHttpRequest: {
      auto typed = task_ptr.template Cast<HandleHttpRequestTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kInitializeSession: {
      auto typed = task_ptr.template Cast<InitializeSessionTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kCloseSession: {
      auto typed = task_ptr.template Cast<CloseSessionTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kStartHttpServer: {
      auto typed = task_ptr.template Cast<StartHttpServerTask>();
      archive << *typed.ptr_;
      break;
    }
    case Method::kStopHttpServer: {
      auto typed = task_ptr.template Cast<StopHttpServerTask>();
      archive << *typed.ptr_;
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
      auto new_task = ipc_manager->NewTask<CreateTask>();
      if (!new_task.IsNull()) {
        new_task->Copy(orig_task_ptr.template Cast<CreateTask>());
        return new_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kDestroy: {
      auto new_task = ipc_manager->NewTask<DestroyTask>();
      if (!new_task.IsNull()) {
        new_task->Copy(orig_task_ptr.template Cast<DestroyTask>());
        return new_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kMonitor: {
      auto new_task = ipc_manager->NewTask<MonitorTask>();
      if (!new_task.IsNull()) {
        new_task->Copy(orig_task_ptr.template Cast<MonitorTask>());
        return new_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kHandleHttpRequest: {
      auto new_task = ipc_manager->NewTask<HandleHttpRequestTask>();
      if (!new_task.IsNull()) {
        new_task->Copy(orig_task_ptr.template Cast<HandleHttpRequestTask>());
        return new_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kInitializeSession: {
      auto new_task = ipc_manager->NewTask<InitializeSessionTask>();
      if (!new_task.IsNull()) {
        new_task->Copy(orig_task_ptr.template Cast<InitializeSessionTask>());
        return new_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kCloseSession: {
      auto new_task = ipc_manager->NewTask<CloseSessionTask>();
      if (!new_task.IsNull()) {
        new_task->Copy(orig_task_ptr.template Cast<CloseSessionTask>());
        return new_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kStartHttpServer: {
      auto new_task = ipc_manager->NewTask<StartHttpServerTask>();
      if (!new_task.IsNull()) {
        new_task->Copy(orig_task_ptr.template Cast<StartHttpServerTask>());
        return new_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kStopHttpServer: {
      auto new_task = ipc_manager->NewTask<StopHttpServerTask>();
      if (!new_task.IsNull()) {
        new_task->Copy(orig_task_ptr.template Cast<StopHttpServerTask>());
        return new_task.template Cast<chi::Task>();
      }
      break;
    }
    default: {
      auto new_task = ipc_manager->NewTask<chi::Task>();
      if (!new_task.IsNull()) {
        new_task->Copy(orig_task_ptr);
        return new_task;
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
    case Method::kHandleHttpRequest:
      return ipc_manager->NewTask<HandleHttpRequestTask>()
          .template Cast<chi::Task>();
    case Method::kInitializeSession:
      return ipc_manager->NewTask<InitializeSessionTask>()
          .template Cast<chi::Task>();
    case Method::kCloseSession:
      return ipc_manager->NewTask<CloseSessionTask>()
          .template Cast<chi::Task>();
    case Method::kStartHttpServer:
      return ipc_manager->NewTask<StartHttpServerTask>()
          .template Cast<chi::Task>();
    case Method::kStopHttpServer:
      return ipc_manager->NewTask<StopHttpServerTask>()
          .template Cast<chi::Task>();
    default:
      return {};
  }
}

void Runtime::Aggregate(chi::u32 method, hipc::FullPtr<chi::Task> orig_task,
                        const hipc::FullPtr<chi::Task>& replica_task) {
  switch (method) {
    case Method::kCreate: {
      orig_task.template Cast<CreateTask>()->Aggregate(replica_task);
      break;
    }
    case Method::kDestroy: {
      orig_task.template Cast<DestroyTask>()->Aggregate(replica_task);
      break;
    }
    case Method::kMonitor: {
      orig_task.template Cast<MonitorTask>()->Aggregate(replica_task);
      break;
    }
    case Method::kHandleHttpRequest: {
      orig_task.template Cast<HandleHttpRequestTask>()->Aggregate(replica_task);
      break;
    }
    case Method::kInitializeSession: {
      orig_task.template Cast<InitializeSessionTask>()->Aggregate(replica_task);
      break;
    }
    case Method::kCloseSession: {
      orig_task.template Cast<CloseSessionTask>()->Aggregate(replica_task);
      break;
    }
    case Method::kStartHttpServer: {
      orig_task.template Cast<StartHttpServerTask>()->Aggregate(replica_task);
      break;
    }
    case Method::kStopHttpServer: {
      orig_task.template Cast<StopHttpServerTask>()->Aggregate(replica_task);
      break;
    }
    default: {
      orig_task->Aggregate(replica_task);
      break;
    }
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
    case Method::kHandleHttpRequest:
      ipc_manager->DelTask(task_ptr.template Cast<HandleHttpRequestTask>());
      break;
    case Method::kInitializeSession:
      ipc_manager->DelTask(task_ptr.template Cast<InitializeSessionTask>());
      break;
    case Method::kCloseSession:
      ipc_manager->DelTask(task_ptr.template Cast<CloseSessionTask>());
      break;
    case Method::kStartHttpServer:
      ipc_manager->DelTask(task_ptr.template Cast<StartHttpServerTask>());
      break;
    case Method::kStopHttpServer:
      ipc_manager->DelTask(task_ptr.template Cast<StopHttpServerTask>());
      break;
    default:
      ipc_manager->DelTask(task_ptr);
      break;
  }
}

}  // namespace mchips::mcp_gateway

CHI_TASK_CC(mchips::mcp_gateway::Runtime)
