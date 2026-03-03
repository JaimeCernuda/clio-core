/**
 * Auto-generated execution implementation for dt_intercept_anthropic ChiMod
 */

#include "dt_provenance/interception/anthropic/anthropic_runtime.h"
#include "dt_provenance/interception/anthropic/autogen/dt_intercept_anthropic_methods.h"
#include <chimaera/chimaera.h>
#include <chimaera/task.h>

namespace dt_provenance::interception::anthropic {

void Runtime::Init(const chi::PoolId& pool_id, const std::string& pool_name,
                   chi::u32 container_id) {
  chi::Container::Init(pool_id, pool_name, container_id);
  client_ = Client(pool_id);
}

chi::TaskResume Runtime::Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                              chi::RunContext& rctx) {
  switch (method) {
    case Method::kCreate: {
      auto typed = task_ptr.template Cast<CreateTask>();
      co_await Create(typed, rctx);
      break;
    }
    case Method::kDestroy: {
      auto typed = task_ptr.template Cast<DestroyTask>();
      co_await Destroy(typed, rctx);
      break;
    }
    case Method::kMonitor: {
      auto typed = task_ptr.template Cast<MonitorTask>();
      co_await Monitor(typed, rctx);
      break;
    }
    case Method::kInterceptAndForward: {
      auto typed = task_ptr.template Cast<InterceptAndForwardTask>();
      co_await InterceptAndForward(typed, rctx);
      break;
    }
    default: break;
  }
  co_return;
}

// --- Serialization ---

void Runtime::SaveTask(chi::u32 method, chi::SaveTaskArchive& archive,
                        hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate:
      archive << *task_ptr.template Cast<CreateTask>().ptr_; break;
    case Method::kDestroy:
      archive << *task_ptr.template Cast<DestroyTask>().ptr_; break;
    case Method::kMonitor:
      archive << *task_ptr.template Cast<MonitorTask>().ptr_; break;
    case Method::kInterceptAndForward:
      archive << *task_ptr.template Cast<InterceptAndForwardTask>().ptr_; break;
    default: break;
  }
}

void Runtime::LoadTask(chi::u32 method, chi::LoadTaskArchive& archive,
                        hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate:
      archive >> *task_ptr.template Cast<CreateTask>().ptr_; break;
    case Method::kDestroy:
      archive >> *task_ptr.template Cast<DestroyTask>().ptr_; break;
    case Method::kMonitor:
      archive >> *task_ptr.template Cast<MonitorTask>().ptr_; break;
    case Method::kInterceptAndForward:
      archive >> *task_ptr.template Cast<InterceptAndForwardTask>().ptr_; break;
    default: break;
  }
}

hipc::FullPtr<chi::Task> Runtime::AllocLoadTask(
    chi::u32 method, chi::LoadTaskArchive& archive) {
  auto task_ptr = NewTask(method);
  if (!task_ptr.IsNull()) LoadTask(method, archive, task_ptr);
  return task_ptr;
}

void Runtime::LocalLoadTask(chi::u32 method,
                             chi::LocalLoadTaskArchive& archive,
                             hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate:
      archive >> *task_ptr.template Cast<CreateTask>().ptr_; break;
    case Method::kDestroy:
      archive >> *task_ptr.template Cast<DestroyTask>().ptr_; break;
    case Method::kMonitor:
      archive >> *task_ptr.template Cast<MonitorTask>().ptr_; break;
    case Method::kInterceptAndForward:
      archive >> *task_ptr.template Cast<InterceptAndForwardTask>().ptr_; break;
    default: break;
  }
}

hipc::FullPtr<chi::Task> Runtime::LocalAllocLoadTask(
    chi::u32 method, chi::LocalLoadTaskArchive& archive) {
  auto task_ptr = NewTask(method);
  if (!task_ptr.IsNull()) LocalLoadTask(method, archive, task_ptr);
  return task_ptr;
}

void Runtime::LocalSaveTask(chi::u32 method,
                              chi::LocalSaveTaskArchive& archive,
                              hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate:
      archive << *task_ptr.template Cast<CreateTask>().ptr_; break;
    case Method::kDestroy:
      archive << *task_ptr.template Cast<DestroyTask>().ptr_; break;
    case Method::kMonitor:
      archive << *task_ptr.template Cast<MonitorTask>().ptr_; break;
    case Method::kInterceptAndForward:
      archive << *task_ptr.template Cast<InterceptAndForwardTask>().ptr_; break;
    default: break;
  }
}

hipc::FullPtr<chi::Task> Runtime::NewCopyTask(
    chi::u32 method, hipc::FullPtr<chi::Task> orig, bool deep) {
  auto* ipc = CHI_IPC;
  if (!ipc) return {};
  (void)deep;

  switch (method) {
    case Method::kCreate: {
      auto p = ipc->NewTask<CreateTask>();
      if (!p.IsNull()) { p->Copy(orig.template Cast<CreateTask>()); return p.template Cast<chi::Task>(); }
      break;
    }
    case Method::kDestroy: {
      auto p = ipc->NewTask<DestroyTask>();
      if (!p.IsNull()) { p->Copy(orig.template Cast<DestroyTask>()); return p.template Cast<chi::Task>(); }
      break;
    }
    case Method::kMonitor: {
      auto p = ipc->NewTask<MonitorTask>();
      if (!p.IsNull()) { p->Copy(orig.template Cast<MonitorTask>()); return p.template Cast<chi::Task>(); }
      break;
    }
    case Method::kInterceptAndForward: {
      auto p = ipc->NewTask<InterceptAndForwardTask>();
      if (!p.IsNull()) { p->Copy(orig.template Cast<InterceptAndForwardTask>()); return p.template Cast<chi::Task>(); }
      break;
    }
    default: break;
  }
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
    case Method::kInterceptAndForward:
      return ipc->NewTask<InterceptAndForwardTask>().template Cast<chi::Task>();
    default: return {};
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
    case Method::kInterceptAndForward:
      orig.template Cast<InterceptAndForwardTask>()->Aggregate(replica); break;
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
    case Method::kInterceptAndForward:
      ipc->DelTask(task_ptr.template Cast<InterceptAndForwardTask>()); break;
    default:
      ipc->DelTask(task_ptr); break;
  }
}

}  // namespace dt_provenance::interception::anthropic
