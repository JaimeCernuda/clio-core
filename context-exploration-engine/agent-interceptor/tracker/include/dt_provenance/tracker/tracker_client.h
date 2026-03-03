#ifndef DT_PROVENANCE_TRACKER_CLIENT_H_
#define DT_PROVENANCE_TRACKER_CLIENT_H_

#include <chimaera/chimaera.h>

#include "tracker_tasks.h"

namespace dt_provenance::tracker {

/**
 * Client API for the Conversation Tracker ChiMod
 */
class Client : public chi::ContainerClient {
 public:
  Client() = default;
  explicit Client(const chi::PoolId& pool_id) { Init(pool_id); }

  /** Create the tracker container */
  chi::Future<CreateTask> AsyncCreate(const chi::PoolQuery& pool_query,
                                      const std::string& pool_name,
                                      const chi::PoolId& custom_pool_id) {
    auto* ipc = CHI_IPC;
    auto task = ipc->NewTask<CreateTask>(
        chi::CreateTaskId(), chi::kAdminPoolId, pool_query,
        CreateParams::chimod_lib_name, pool_name, custom_pool_id, this);
    return ipc->Send(task);
  }

  /** Store an interaction record */
  chi::Future<StoreInteractionTask> AsyncStoreInteraction(
      const chi::PoolQuery& pool_query,
      const std::string& interaction_json) {
    auto* ipc = CHI_IPC;
    auto task = ipc->NewTask<StoreInteractionTask>(
        chi::CreateTaskId(), pool_id_, pool_query, interaction_json);
    return ipc->Send(task);
  }

  /** Query all interactions in a session */
  chi::Future<QuerySessionTask> AsyncQuerySession(
      const chi::PoolQuery& pool_query,
      const std::string& session_id) {
    auto* ipc = CHI_IPC;
    auto task = ipc->NewTask<QuerySessionTask>(
        chi::CreateTaskId(), pool_id_, pool_query, session_id);
    return ipc->Send(task);
  }

  /** List all sessions */
  chi::Future<ListSessionsTask> AsyncListSessions(
      const chi::PoolQuery& pool_query) {
    auto* ipc = CHI_IPC;
    auto task = ipc->NewTask<ListSessionsTask>(
        chi::CreateTaskId(), pool_id_, pool_query);
    return ipc->Send(task);
  }

  /** Get a single interaction by session + sequence_id */
  chi::Future<GetInteractionTask> AsyncGetInteraction(
      const chi::PoolQuery& pool_query,
      const std::string& session_id, chi::u64 sequence_id) {
    auto* ipc = CHI_IPC;
    auto task = ipc->NewTask<GetInteractionTask>(
        chi::CreateTaskId(), pool_id_, pool_query, session_id, sequence_id);
    return ipc->Send(task);
  }

  /** Monitor container state */
  chi::Future<MonitorTask> AsyncMonitor(const chi::PoolQuery& pool_query,
                                        const std::string& query) {
    auto* ipc = CHI_IPC;
    auto task = ipc->NewTask<MonitorTask>(
        chi::CreateTaskId(), pool_id_, pool_query, query);
    return ipc->Send(task);
  }
};

}  // namespace dt_provenance::tracker

#endif  // DT_PROVENANCE_TRACKER_CLIENT_H_
