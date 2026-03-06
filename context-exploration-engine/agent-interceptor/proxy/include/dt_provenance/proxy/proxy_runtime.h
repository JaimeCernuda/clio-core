#ifndef DT_PROVENANCE_PROXY_RUNTIME_H_
#define DT_PROVENANCE_PROXY_RUNTIME_H_

#include <chimaera/chimaera.h>

#include <atomic>
#include <chrono>
#include <memory>

#include "proxy_client.h"
#include "proxy_tasks.h"
#include "autogen/dt_proxy_methods.h"

namespace dt_provenance::tracker { class Client; }

namespace dt_provenance::proxy {

/**
 * Runtime container for the HTTP Proxy ChiMod
 *
 * All LLM forwarding now happens on Chimaera worker threads via the
 * Monitor handler.  A Python Flask bridge translates agent HTTP requests
 * into pool_stats://800.0:local:<json> queries that land here.
 */
class Runtime : public chi::Container {
 public:
  using CreateParams = dt_provenance::proxy::CreateParams;

  Runtime() = default;
  ~Runtime() override;

  // Container interface
  void Init(const chi::PoolId& pool_id, const std::string& pool_name,
            chi::u32 container_id = 0) override;
  chi::TaskResume Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                      chi::RunContext& rctx) override;

  // Method handlers
  chi::TaskResume Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx);
  chi::TaskResume Monitor(hipc::FullPtr<MonitorTask> task,
                          chi::RunContext& rctx);
  chi::TaskResume Destroy(hipc::FullPtr<DestroyTask> task,
                          chi::RunContext& rctx);
  chi::TaskResume ForwardHttp(hipc::FullPtr<ForwardHttpTask> task,
                              chi::RunContext& rctx);

  // Container virtual methods
  chi::TaskStat GetTaskStats(chi::u32 method_id) const override;
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
  hipc::FullPtr<chi::Task> NewCopyTask(chi::u32 method,
                                       hipc::FullPtr<chi::Task> orig_task_ptr,
                                       bool deep) override;
  hipc::FullPtr<chi::Task> NewTask(chi::u32 method) override;
  void Aggregate(chi::u32 method, hipc::FullPtr<chi::Task> orig_task,
                 const hipc::FullPtr<chi::Task>& replica_task) override;
  void DelTask(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) override;

 private:
  void HandleDispatchStats(hipc::FullPtr<MonitorTask>& task);

  bool EnsureTrackerClient();

  Client client_;

  // Tracker client (lazy-initialized on first use)
  bool tracker_initialized_ = false;
  std::unique_ptr<tracker::Client> tracker_client_;

  // Dispatch stats
  std::atomic<uint64_t> total_requests_{0};
  std::chrono::steady_clock::time_point start_time_;
};

}  // namespace dt_provenance::proxy

#endif  // DT_PROVENANCE_PROXY_RUNTIME_H_
