#ifndef DT_PROVENANCE_INTERCEPTION_ANTHROPIC_CLIENT_H_
#define DT_PROVENANCE_INTERCEPTION_ANTHROPIC_CLIENT_H_

#include <chimaera/chimaera.h>

#include "anthropic_tasks.h"

namespace dt_provenance::interception::anthropic {

/**
 * Client API for the Anthropic Interception ChiMod
 */
class Client : public chi::ContainerClient {
 public:
  Client() = default;
  explicit Client(const chi::PoolId& pool_id) { Init(pool_id); }

  /** Create the Anthropic interception container */
  chi::Future<CreateTask> AsyncCreate(const chi::PoolQuery& pool_query,
                                      const std::string& pool_name,
                                      const chi::PoolId& custom_pool_id,
                                      const std::string& upstream_base_url) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<CreateTask>(
        chi::CreateTaskId(), chi::kAdminPoolId, pool_query,
        CreateParams::chimod_lib_name, pool_name, custom_pool_id, this,
        upstream_base_url);
    return ipc_manager->Send(task);
  }

  /**
   * Intercept and forward a request to Anthropic
   * @param session_id Session identifier
   * @param path API path (e.g., "/v1/messages")
   * @param headers_json JSON-serialized request headers
   * @param request_body Request body
   * @param request_time_ns Timestamp when request was received (nanoseconds)
   */
  chi::Future<InterceptAndForwardTask> AsyncInterceptAndForward(
      const chi::PoolQuery& pool_query, const std::string& session_id,
      const std::string& path, const std::string& headers_json,
      const std::string& request_body, chi::u64 request_time_ns) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<InterceptAndForwardTask>(
        chi::CreateTaskId(), pool_id_, pool_query, session_id, path,
        headers_json, request_body, request_time_ns);
    return ipc_manager->Send(task);
  }

  /** Monitor container state */
  chi::Future<MonitorTask> AsyncMonitor(const chi::PoolQuery& pool_query,
                                        const std::string& query) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<MonitorTask>(chi::CreateTaskId(),
                                                   pool_id_, pool_query, query);
    return ipc_manager->Send(task);
  }
};

}  // namespace dt_provenance::interception::anthropic

#endif  // DT_PROVENANCE_INTERCEPTION_ANTHROPIC_CLIENT_H_
