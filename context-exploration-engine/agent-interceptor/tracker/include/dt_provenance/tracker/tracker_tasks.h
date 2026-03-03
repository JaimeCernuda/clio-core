#ifndef DT_PROVENANCE_TRACKER_TASKS_H_
#define DT_PROVENANCE_TRACKER_TASKS_H_

#include <chimaera/admin/admin_tasks.h>
#include <chimaera/chimaera.h>
#include <chimaera/config_manager.h>
#include <yaml-cpp/yaml.h>

#include "autogen/dt_tracker_methods.h"

namespace dt_provenance::tracker {

using MonitorTask = chimaera::admin::MonitorTask;

/**
 * CreateParams for the Conversation Tracker ChiMod
 */
struct CreateParams {
  static constexpr const char* chimod_lib_name = "dt_provenance_dt_tracker";

  CreateParams() = default;

  template <class Archive>
  void serialize(Archive& ar) {
    // No parameters needed
    (void)ar;
  }

  void LoadConfig(const chi::PoolConfig& pool_config) {
    (void)pool_config;
  }
};

using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;
using DestroyTask = chimaera::admin::DestroyTask;

/**
 * StoreInteractionTask — store an interaction record in CTE
 */
struct StoreInteractionTask : public chi::Task {
  IN chi::priv::string interaction_json_;
  OUT chi::u64 sequence_id_;

  StoreInteractionTask()
      : chi::Task(), interaction_json_(HSHM_MALLOC), sequence_id_(0) {}

  explicit StoreInteractionTask(const chi::TaskId& task_node,
                                const chi::PoolId& pool_id,
                                const chi::PoolQuery& pool_query,
                                const std::string& interaction_json)
      : chi::Task(task_node, pool_id, pool_query, 10),
        interaction_json_(HSHM_MALLOC),
        sequence_id_(0) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kStoreInteraction;
    task_flags_.Clear();
    pool_query_ = pool_query;
    interaction_json_ = interaction_json;
  }

  template <typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    ar(interaction_json_);
  }

  template <typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(sequence_id_);
  }

  void Copy(const hipc::FullPtr<StoreInteractionTask>& other) {
    Task::Copy(other.template Cast<Task>());
    interaction_json_ = other->interaction_json_;
    sequence_id_ = other->sequence_id_;
  }

  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<StoreInteractionTask>());
  }
};

/**
 * QuerySessionTask — retrieve all interactions for a session
 */
struct QuerySessionTask : public chi::Task {
  IN chi::priv::string session_id_;
  OUT chi::priv::string interactions_json_;  // JSON array

  QuerySessionTask()
      : chi::Task(),
        session_id_(HSHM_MALLOC),
        interactions_json_(HSHM_MALLOC) {}

  explicit QuerySessionTask(const chi::TaskId& task_node,
                            const chi::PoolId& pool_id,
                            const chi::PoolQuery& pool_query,
                            const std::string& session_id)
      : chi::Task(task_node, pool_id, pool_query, 10),
        session_id_(HSHM_MALLOC),
        interactions_json_(HSHM_MALLOC) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kQuerySession;
    task_flags_.Clear();
    pool_query_ = pool_query;
    session_id_ = session_id;
  }

  template <typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    ar(session_id_);
  }

  template <typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(interactions_json_);
  }

  void Copy(const hipc::FullPtr<QuerySessionTask>& other) {
    Task::Copy(other.template Cast<Task>());
    session_id_ = other->session_id_;
    interactions_json_ = other->interactions_json_;
  }

  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<QuerySessionTask>());
  }
};

/**
 * ListSessionsTask — list all sessions with summary info
 */
struct ListSessionsTask : public chi::Task {
  OUT chi::priv::string sessions_json_;  // JSON array

  ListSessionsTask() : chi::Task(), sessions_json_(HSHM_MALLOC) {}

  explicit ListSessionsTask(const chi::TaskId& task_node,
                            const chi::PoolId& pool_id,
                            const chi::PoolQuery& pool_query)
      : chi::Task(task_node, pool_id, pool_query, 10),
        sessions_json_(HSHM_MALLOC) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kListSessions;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  template <typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
  }

  template <typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(sessions_json_);
  }

  void Copy(const hipc::FullPtr<ListSessionsTask>& other) {
    Task::Copy(other.template Cast<Task>());
    sessions_json_ = other->sessions_json_;
  }

  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<ListSessionsTask>());
  }
};

/**
 * GetInteractionTask — retrieve a single interaction by session + sequence ID
 */
struct GetInteractionTask : public chi::Task {
  IN chi::priv::string session_id_;
  IN chi::u64 sequence_id_;
  OUT chi::priv::string interaction_json_;

  GetInteractionTask()
      : chi::Task(),
        session_id_(HSHM_MALLOC),
        sequence_id_(0),
        interaction_json_(HSHM_MALLOC) {}

  explicit GetInteractionTask(const chi::TaskId& task_node,
                              const chi::PoolId& pool_id,
                              const chi::PoolQuery& pool_query,
                              const std::string& session_id,
                              chi::u64 sequence_id)
      : chi::Task(task_node, pool_id, pool_query, 10),
        session_id_(HSHM_MALLOC),
        sequence_id_(sequence_id),
        interaction_json_(HSHM_MALLOC) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kGetInteraction;
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
    ar(interaction_json_);
  }

  void Copy(const hipc::FullPtr<GetInteractionTask>& other) {
    Task::Copy(other.template Cast<Task>());
    session_id_ = other->session_id_;
    sequence_id_ = other->sequence_id_;
    interaction_json_ = other->interaction_json_;
  }

  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<GetInteractionTask>());
  }
};

}  // namespace dt_provenance::tracker

#endif  // DT_PROVENANCE_TRACKER_TASKS_H_
