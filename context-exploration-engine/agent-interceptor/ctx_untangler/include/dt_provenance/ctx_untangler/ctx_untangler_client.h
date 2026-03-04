#ifndef DT_PROVENANCE_CTX_UNTANGLER_CLIENT_H_
#define DT_PROVENANCE_CTX_UNTANGLER_CLIENT_H_

#include <chimaera/chimaera.h>

#include "ctx_untangler_tasks.h"

namespace dt_provenance::ctx_untangler {

/**
 * Client API for the Ctx Untangler ChiMod
 */
class Client : public chi::ContainerClient {
 public:
  Client() = default;
  explicit Client(const chi::PoolId& pool_id) { Init(pool_id); }

  /** Create the untangler container */
  chi::Future<CreateTask> AsyncCreate(const chi::PoolQuery& pool_query,
                                      const std::string& pool_name,
                                      const chi::PoolId& custom_pool_id) {
    auto* ipc = CHI_IPC;
    auto task = ipc->NewTask<CreateTask>(
        chi::CreateTaskId(), chi::kAdminPoolId, pool_query,
        CreateParams::chimod_lib_name, pool_name, custom_pool_id, this);
    return ipc->Send(task);
  }

  /** Compute diff for a newly-stored interaction */
  chi::Future<ComputeDiffTask> AsyncComputeDiff(
      const chi::PoolQuery& pool_query,
      const std::string& session_id, chi::u64 sequence_id) {
    auto* ipc = CHI_IPC;
    auto task = ipc->NewTask<ComputeDiffTask>(
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

}  // namespace dt_provenance::ctx_untangler

#endif  // DT_PROVENANCE_CTX_UNTANGLER_CLIENT_H_
