/**
 * Auto-generated execution implementation for dt_proxy ChiMod
 */

#include "dt_provenance/proxy/proxy_runtime.h"
#include "dt_provenance/proxy/autogen/dt_proxy_methods.h"
#include <chimaera/chimaera.h>
#include <chimaera/task.h>

namespace dt_provenance::proxy {

void Runtime::Init(const chi::PoolId& pool_id, const std::string& pool_name,
                   chi::u32 container_id) {
  chi::Container::Init(pool_id, pool_name, container_id);
  client_ = Client(pool_id);
}

chi::TaskResume Runtime::Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                              chi::RunContext& rctx) {
  switch (method) {
    case Method::kCreate: {
      auto typed_task = task_ptr.template Cast<CreateTask>();
      co_await Create(typed_task, rctx);
      break;
    }
    case Method::kDestroy: {
      auto typed_task = task_ptr.template Cast<DestroyTask>();
      co_await Destroy(typed_task, rctx);
      break;
    }
    case Method::kMonitor: {
      auto typed_task = task_ptr.template Cast<MonitorTask>();
      co_await Monitor(typed_task, rctx);
      break;
    }
    default:
      break;
  }
  co_return;
}

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
    default: break;
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
    default: break;
  }
}

hipc::FullPtr<chi::Task> Runtime::AllocLoadTask(
    chi::u32 method, chi::LoadTaskArchive& archive) {
  auto task_ptr = NewTask(method);
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
    default: break;
  }
}

hipc::FullPtr<chi::Task> Runtime::LocalAllocLoadTask(
    chi::u32 method, chi::LocalLoadTaskArchive& archive) {
  auto task_ptr = NewTask(method);
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
    default: break;
  }
}

hipc::FullPtr<chi::Task> Runtime::NewCopyTask(
    chi::u32 method, hipc::FullPtr<chi::Task> orig_task_ptr, bool deep) {
  auto* ipc_manager = CHI_IPC;
  if (!ipc_manager) return hipc::FullPtr<chi::Task>();

  switch (method) {
    case Method::kCreate: {
      auto p = ipc_manager->NewTask<CreateTask>();
      if (!p.IsNull()) {
        p->Copy(orig_task_ptr.template Cast<CreateTask>());
        return p.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kDestroy: {
      auto p = ipc_manager->NewTask<DestroyTask>();
      if (!p.IsNull()) {
        p->Copy(orig_task_ptr.template Cast<DestroyTask>());
        return p.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kMonitor: {
      auto p = ipc_manager->NewTask<MonitorTask>();
      if (!p.IsNull()) {
        p->Copy(orig_task_ptr.template Cast<MonitorTask>());
        return p.template Cast<chi::Task>();
      }
      break;
    }
    default: break;
  }
  (void)deep;
  return hipc::FullPtr<chi::Task>();
}

hipc::FullPtr<chi::Task> Runtime::NewTask(chi::u32 method) {
  auto* ipc_manager = CHI_IPC;
  if (!ipc_manager) return hipc::FullPtr<chi::Task>();

  switch (method) {
    case Method::kCreate:
      return ipc_manager->NewTask<CreateTask>().template Cast<chi::Task>();
    case Method::kDestroy:
      return ipc_manager->NewTask<DestroyTask>().template Cast<chi::Task>();
    case Method::kMonitor:
      return ipc_manager->NewTask<MonitorTask>().template Cast<chi::Task>();
    default:
      return hipc::FullPtr<chi::Task>();
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
    default:
      ipc_manager->DelTask(task_ptr);
      break;
  }
}

}  // namespace dt_provenance::proxy
