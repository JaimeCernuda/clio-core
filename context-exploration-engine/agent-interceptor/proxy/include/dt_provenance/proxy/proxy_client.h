#ifndef DT_PROVENANCE_PROXY_CLIENT_H_
#define DT_PROVENANCE_PROXY_CLIENT_H_

#include <chimaera/chimaera.h>

#include "proxy_tasks.h"

namespace dt_provenance::proxy {

/**
 * Client API for the HTTP Proxy ChiMod
 *
 * The proxy is a lifecycle-only ChiMod: Create starts the HTTP server,
 * Destroy stops it. No custom task methods needed — HTTP threads dispatch
 * directly to interception ChiMod clients.
 */
class Client : public chi::ContainerClient {
 public:
  Client() = default;
  explicit Client(const chi::PoolId& pool_id) { Init(pool_id); }

  /** Create the proxy container (starts HTTP server) */
  chi::Future<CreateTask> AsyncCreate(const chi::PoolQuery& pool_query,
                                      const std::string& pool_name,
                                      const chi::PoolId& custom_pool_id,
                                      chi::u16 port = 9090,
                                      chi::u16 num_threads = 8) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<CreateTask>(
        chi::CreateTaskId(), chi::kAdminPoolId, pool_query,
        CreateParams::chimod_lib_name, pool_name, custom_pool_id, this,
        port, num_threads);
    return ipc_manager->Send(task);
  }

  /** Monitor proxy state */
  chi::Future<MonitorTask> AsyncMonitor(const chi::PoolQuery& pool_query,
                                        const std::string& query) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<MonitorTask>(chi::CreateTaskId(),
                                                   pool_id_, pool_query, query);
    return ipc_manager->Send(task);
  }
};

}  // namespace dt_provenance::proxy

#endif  // DT_PROVENANCE_PROXY_CLIENT_H_
