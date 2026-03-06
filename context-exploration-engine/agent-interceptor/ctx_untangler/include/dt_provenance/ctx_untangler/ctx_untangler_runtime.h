#ifndef DT_PROVENANCE_CTX_UNTANGLER_RUNTIME_H_
#define DT_PROVENANCE_CTX_UNTANGLER_RUNTIME_H_

#include <chimaera/chimaera.h>
#include <string>

#include "autogen/dt_ctx_untangler_methods.h"
#include "ctx_untangler_client.h"
#include "ctx_untangler_tasks.h"

namespace dt_provenance::ctx_untangler {

/**
 * Runtime container for the Context Untangler ChiMod
 *
 * Eagerly computes diffs when interactions arrive and stores
 * them in sister CTE buckets (Ctx_graph_<session_id>).
 */
class Runtime : public chi::Container {
 public:
  using CreateParams = dt_provenance::ctx_untangler::CreateParams;

  Runtime() = default;
  ~Runtime() override = default;

  // Container interface
  void Init(const chi::PoolId& pool_id, const std::string& pool_name,
            chi::u32 container_id = 0) override;
  chi::TaskResume Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                      chi::RunContext& rctx) override;

  // Method handlers
  chi::TaskResume Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx);
  chi::TaskResume ComputeDiff(hipc::FullPtr<ComputeDiffTask> task,
                              chi::RunContext& rctx);
  chi::TaskResume Monitor(hipc::FullPtr<MonitorTask> task,
                          chi::RunContext& rctx);
  chi::TaskResume Destroy(hipc::FullPtr<DestroyTask> task,
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
  /** Format sequence_id as a zero-padded blob name */
  static std::string FormatBlobName(uint64_t seq_id);

  /** Build CTE graph tag name from session_id */
  static std::string BuildGraphTagName(const std::string& session_id);

  /** Build CTE interaction tag name from session_id */
  static std::string BuildInteractionTagName(const std::string& session_id);

  Client client_;
};

}  // namespace dt_provenance::ctx_untangler

#endif  // DT_PROVENANCE_CTX_UNTANGLER_RUNTIME_H_
