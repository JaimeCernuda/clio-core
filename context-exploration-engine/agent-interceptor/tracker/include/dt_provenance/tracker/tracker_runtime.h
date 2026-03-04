#ifndef DT_PROVENANCE_TRACKER_RUNTIME_H_
#define DT_PROVENANCE_TRACKER_RUNTIME_H_

#include <atomic>
#include <chimaera/chimaera.h>
#include <string>

#include "autogen/dt_tracker_methods.h"
#include "conversation_threading.h"
#include "tracker_client.h"
#include "tracker_tasks.h"

// Forward-declare untangler client to avoid circular header deps
namespace dt_provenance::ctx_untangler {
class Client;
}

namespace dt_provenance::tracker {

/**
 * Runtime container for the Conversation Tracker ChiMod
 *
 * Stores interaction records using CTE's Tag/Blob model:
 * - Tag = "Agentic_session_{session_id}"
 * - Blob = zero-padded monotonic counter (e.g., "0000000001")
 *
 * Uses CTE for persistent storage across restarts.
 */
class Runtime : public chi::Container {
 public:
  using CreateParams = dt_provenance::tracker::CreateParams;

  Runtime() = default;
  ~Runtime() override;

  // Container interface
  void Init(const chi::PoolId& pool_id, const std::string& pool_name,
            chi::u32 container_id = 0) override;
  chi::TaskResume Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                      chi::RunContext& rctx) override;

  // Method handlers
  chi::TaskResume Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx);
  chi::TaskResume StoreInteraction(hipc::FullPtr<StoreInteractionTask> task,
                                   chi::RunContext& rctx);
  chi::TaskResume QuerySession(hipc::FullPtr<QuerySessionTask> task,
                               chi::RunContext& rctx);
  chi::TaskResume ListSessions(hipc::FullPtr<ListSessionsTask> task,
                               chi::RunContext& rctx);
  chi::TaskResume GetInteraction(hipc::FullPtr<GetInteractionTask> task,
                                 chi::RunContext& rctx);
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
  /** Format sequence_id as a zero-padded blob name */
  static std::string FormatBlobName(uint64_t seq_id);

  /** Build CTE tag name from session_id */
  static std::string BuildTagName(const std::string& session_id);

  Client client_;
  std::atomic<uint64_t> sequence_counter_{0};
  ConversationThreader threader_;

  // Ctx Untangler lazy-init (dispatches ComputeDiff after storing)
  std::unique_ptr<dt_provenance::ctx_untangler::Client> untangler_client_;
  bool untangler_initialized_ = false;
};

}  // namespace dt_provenance::tracker

#endif  // DT_PROVENANCE_TRACKER_RUNTIME_H_
