#ifndef DT_PROVENANCE_CTX_UNTANGLER_TASKS_H_
#define DT_PROVENANCE_CTX_UNTANGLER_TASKS_H_

#include <chimaera/admin/admin_tasks.h>
#include <chimaera/chimaera.h>
#include <chimaera/config_manager.h>
#include <yaml-cpp/yaml.h>

#include "autogen/dt_ctx_untangler_methods.h"

namespace dt_provenance::ctx_untangler {

using MonitorTask = chimaera::admin::MonitorTask;

/**
 * CreateParams for the Ctx Untangler ChiMod
 */
struct CreateParams {
  static constexpr const char* chimod_lib_name = "dt_provenance_dt_ctx_untangler";

  CreateParams() = default;

  template <class Archive>
  void serialize(Archive& ar) {
    (void)ar;
  }

  void LoadConfig(const chi::PoolConfig& pool_config) {
    (void)pool_config;
  }
};

using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;
using DestroyTask = chimaera::admin::DestroyTask;

/**
 * ComputeDiffTask — compute diff for a newly-stored interaction
 */
struct ComputeDiffTask : public chi::Task {
  IN chi::priv::string session_id_;
  IN chi::u64 sequence_id_;
  OUT chi::u64 success_;

  ComputeDiffTask()
      : chi::Task(), session_id_(HSHM_MALLOC), sequence_id_(0), success_(0) {}

  explicit ComputeDiffTask(const chi::TaskId& task_node,
                           const chi::PoolId& pool_id,
                           const chi::PoolQuery& pool_query,
                           const std::string& session_id,
                           chi::u64 sequence_id)
      : chi::Task(task_node, pool_id, pool_query, 10),
        session_id_(HSHM_MALLOC),
        sequence_id_(sequence_id),
        success_(0) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kComputeDiff;
    task_flags_.Clear();
    pool_query_ = pool_query;
    session_id_ = session_id;
  }

  template <typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    ar(session_id_, sequence_id_);
  }

  template <typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(success_);
  }

  void Copy(const hipc::FullPtr<ComputeDiffTask>& other) {
    Task::Copy(other.template Cast<Task>());
    session_id_ = other->session_id_;
    sequence_id_ = other->sequence_id_;
    success_ = other->success_;
  }

  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<ComputeDiffTask>());
  }
};

}  // namespace dt_provenance::ctx_untangler

#endif  // DT_PROVENANCE_CTX_UNTANGLER_TASKS_H_
