#ifndef DT_PROVENANCE_PROXY_TASKS_H_
#define DT_PROVENANCE_PROXY_TASKS_H_

#include <chimaera/admin/admin_tasks.h>
#include <chimaera/chimaera.h>
#include <chimaera/config_manager.h>
#include <yaml-cpp/yaml.h>

#include "autogen/dt_proxy_methods.h"

namespace dt_provenance::proxy {

using MonitorTask = chimaera::admin::MonitorTask;

/**
 * CreateParams for the HTTP Proxy ChiMod
 */
struct CreateParams {
  uint16_t port_;
  uint16_t num_threads_;

  static constexpr const char* chimod_lib_name = "dt_provenance_dt_proxy";

  CreateParams() : port_(9090), num_threads_(8) {}

  CreateParams(uint16_t port, uint16_t num_threads)
      : port_(port), num_threads_(num_threads) {}

  template <class Archive>
  void serialize(Archive& ar) {
    ar(port_, num_threads_);
  }

  /** Load from compose YAML config */
  void LoadConfig(const chi::PoolConfig& pool_config) {
    YAML::Node config = YAML::Load(pool_config.config_);
    if (config["port"]) {
      port_ = config["port"].as<uint16_t>();
    }
    if (config["num_threads"]) {
      num_threads_ = config["num_threads"].as<uint16_t>();
    }
  }
};

using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;
using DestroyTask = chimaera::admin::DestroyTask;

/**
 * ForwardHttpTask — forward an HTTP request to upstream API on an I/O worker
 */
struct ForwardHttpTask : public chi::Task {
  IN chi::priv::string query_json_;        // Full JSON forward request
  OUT chi::priv::string response_msgpack_; // Msgpack {status, headers, body}
  OUT chi::priv::string record_json_;      // Interaction record for tracker

  ForwardHttpTask()
      : chi::Task(),
        query_json_(HSHM_MALLOC),
        response_msgpack_(HSHM_MALLOC),
        record_json_(HSHM_MALLOC) {}

  explicit ForwardHttpTask(const chi::TaskId& task_node,
                           const chi::PoolId& pool_id,
                           const chi::PoolQuery& pool_query,
                           const std::string& query_json)
      : chi::Task(task_node, pool_id, pool_query, 10),
        query_json_(HSHM_MALLOC),
        response_msgpack_(HSHM_MALLOC),
        record_json_(HSHM_MALLOC) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kForwardHttp;
    task_flags_.Clear();
    pool_query_ = pool_query;
    query_json_ = query_json;
  }

  template <typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    ar(query_json_);
  }

  template <typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(response_msgpack_, record_json_);
  }

  void Copy(const hipc::FullPtr<ForwardHttpTask>& other) {
    Task::Copy(other.template Cast<Task>());
    query_json_ = other->query_json_;
    response_msgpack_ = other->response_msgpack_;
    record_json_ = other->record_json_;
  }

  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<ForwardHttpTask>());
  }
};

}  // namespace dt_provenance::proxy

#endif  // DT_PROVENANCE_PROXY_TASKS_H_
