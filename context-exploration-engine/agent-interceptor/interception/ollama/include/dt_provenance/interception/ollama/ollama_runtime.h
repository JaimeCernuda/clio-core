#ifndef DT_PROVENANCE_INTERCEPTION_OLLAMA_RUNTIME_H_
#define DT_PROVENANCE_INTERCEPTION_OLLAMA_RUNTIME_H_

#include <chimaera/chimaera.h>
#include <memory>
#include <string>

#include "ollama_client.h"
#include "ollama_tasks.h"
#include "autogen/dt_intercept_ollama_methods.h"

namespace httplib {
class Client;
}  // namespace httplib

namespace dt_provenance::interception::ollama {

/**
 * Runtime container for the Ollama Interception ChiMod
 *
 * Owns an httplib HTTP client pointing at localhost:11434 (default Ollama).
 * Handles InterceptAndForward: forward → receive → parse → dispatch to tracker.
 *
 * Ollama uses NDJSON streaming (not SSE) and runs locally (no SSL).
 */
class Runtime : public chi::Container {
 public:
  using CreateParams = dt_provenance::interception::ollama::CreateParams;

  Runtime() = default;
  ~Runtime() override = default;

  // Container interface
  void Init(const chi::PoolId& pool_id, const std::string& pool_name,
            chi::u32 container_id = 0) override;
  chi::TaskResume Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                      chi::RunContext& rctx) override;

  // Method handlers
  chi::TaskResume Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx);
  chi::TaskResume InterceptAndForward(
      hipc::FullPtr<InterceptAndForwardTask> task, chi::RunContext& rctx);
  chi::TaskResume Monitor(hipc::FullPtr<MonitorTask> task,
                          chi::RunContext& rctx);
  chi::TaskResume Destroy(hipc::FullPtr<DestroyTask> task,
                          chi::RunContext& rctx);

  // Container virtual methods
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
  Client client_;
  std::string upstream_host_;
  int upstream_port_ = 11434;
  bool upstream_ssl_ = false;  // Ollama is always local HTTP
  std::atomic<uint64_t> active_requests_{0};
};

}  // namespace dt_provenance::interception::ollama

#endif  // DT_PROVENANCE_INTERCEPTION_OLLAMA_RUNTIME_H_
