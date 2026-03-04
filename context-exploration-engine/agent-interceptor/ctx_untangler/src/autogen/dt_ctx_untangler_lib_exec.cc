/**
 * Auto-generated execution implementation for dt_ctx_untangler ChiMod
 */

#include "dt_provenance/ctx_untangler/ctx_untangler_runtime.h"
#include "dt_provenance/ctx_untangler/autogen/dt_ctx_untangler_methods.h"
#include <chimaera/chimaera.h>
#include <chimaera/task.h>

namespace dt_provenance::ctx_untangler {

void Runtime::Init(const chi::PoolId& pool_id, const std::string& pool_name,
                   chi::u32 container_id) {
  chi::Container::Init(pool_id, pool_name, container_id);
  client_ = Client(pool_id);
}

chi::TaskResume Runtime::Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                              chi::RunContext& rctx) {
  switch (method) {
    case Method::kCreate: {
      co_await Create(task_ptr.template Cast<CreateTask>(), rctx); break;
    }
    case Method::kDestroy: {
      co_await Destroy(task_ptr.template Cast<DestroyTask>(), rctx); break;
    }
    case Method::kMonitor: {
      co_await Monitor(task_ptr.template Cast<MonitorTask>(), rctx); break;
    }
    case Method::kComputeDiff: {
      co_await ComputeDiff(task_ptr.template Cast<ComputeDiffTask>(), rctx); break;
    }
    default: break;
  }
  co_return;
}

#define SAVE_CASE(METHOD, TYPE) \
  case Method::METHOD: archive << *task_ptr.template Cast<TYPE>().ptr_; break

#define LOAD_CASE(METHOD, TYPE) \
  case Method::METHOD: archive >> *task_ptr.template Cast<TYPE>().ptr_; break

void Runtime::SaveTask(chi::u32 method, chi::SaveTaskArchive& archive,
                        hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    SAVE_CASE(kCreate, CreateTask);
    SAVE_CASE(kDestroy, DestroyTask);
    SAVE_CASE(kMonitor, MonitorTask);
    SAVE_CASE(kComputeDiff, ComputeDiffTask);
    default: break;
  }
}

void Runtime::LoadTask(chi::u32 method, chi::LoadTaskArchive& archive,
                        hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    LOAD_CASE(kCreate, CreateTask);
    LOAD_CASE(kDestroy, DestroyTask);
    LOAD_CASE(kMonitor, MonitorTask);
    LOAD_CASE(kComputeDiff, ComputeDiffTask);
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
    LOAD_CASE(kCreate, CreateTask);
    LOAD_CASE(kDestroy, DestroyTask);
    LOAD_CASE(kMonitor, MonitorTask);
    LOAD_CASE(kComputeDiff, ComputeDiffTask);
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
    SAVE_CASE(kCreate, CreateTask);
    SAVE_CASE(kDestroy, DestroyTask);
    SAVE_CASE(kMonitor, MonitorTask);
    SAVE_CASE(kComputeDiff, ComputeDiffTask);
    default: break;
  }
}

#undef SAVE_CASE
#undef LOAD_CASE

#define COPY_CASE(METHOD, TYPE) \
  case Method::METHOD: { \
    auto p = ipc->NewTask<TYPE>(); \
    if (!p.IsNull()) { p->Copy(orig.template Cast<TYPE>()); return p.template Cast<chi::Task>(); } \
    break; \
  }

hipc::FullPtr<chi::Task> Runtime::NewCopyTask(
    chi::u32 method, hipc::FullPtr<chi::Task> orig, bool deep) {
  auto* ipc = CHI_IPC;
  if (!ipc) return {};
  (void)deep;

  switch (method) {
    COPY_CASE(kCreate, CreateTask)
    COPY_CASE(kDestroy, DestroyTask)
    COPY_CASE(kMonitor, MonitorTask)
    COPY_CASE(kComputeDiff, ComputeDiffTask)
    default: break;
  }
  return {};
}

#undef COPY_CASE

#define NEW_CASE(METHOD, TYPE) \
  case Method::METHOD: return ipc->NewTask<TYPE>().template Cast<chi::Task>()

hipc::FullPtr<chi::Task> Runtime::NewTask(chi::u32 method) {
  auto* ipc = CHI_IPC;
  if (!ipc) return {};

  switch (method) {
    NEW_CASE(kCreate, CreateTask);
    NEW_CASE(kDestroy, DestroyTask);
    NEW_CASE(kMonitor, MonitorTask);
    NEW_CASE(kComputeDiff, ComputeDiffTask);
    default: return {};
  }
}

#undef NEW_CASE

#define AGG_CASE(METHOD, TYPE) \
  case Method::METHOD: orig.template Cast<TYPE>()->Aggregate(replica); break

void Runtime::Aggregate(chi::u32 method, hipc::FullPtr<chi::Task> orig,
                         const hipc::FullPtr<chi::Task>& replica) {
  switch (method) {
    AGG_CASE(kCreate, CreateTask);
    AGG_CASE(kDestroy, DestroyTask);
    AGG_CASE(kMonitor, MonitorTask);
    AGG_CASE(kComputeDiff, ComputeDiffTask);
    default: orig->Aggregate(replica); break;
  }
}

#undef AGG_CASE

#define DEL_CASE(METHOD, TYPE) \
  case Method::METHOD: ipc->DelTask(task_ptr.template Cast<TYPE>()); break

void Runtime::DelTask(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) {
  auto* ipc = CHI_IPC;
  if (!ipc) return;
  switch (method) {
    DEL_CASE(kCreate, CreateTask);
    DEL_CASE(kDestroy, DestroyTask);
    DEL_CASE(kMonitor, MonitorTask);
    DEL_CASE(kComputeDiff, ComputeDiffTask);
    default: ipc->DelTask(task_ptr); break;
  }
}

#undef DEL_CASE

}  // namespace dt_provenance::ctx_untangler

CHI_TASK_CC(dt_provenance::ctx_untangler::Runtime)
