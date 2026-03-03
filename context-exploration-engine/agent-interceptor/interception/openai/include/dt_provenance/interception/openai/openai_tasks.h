#ifndef DT_PROVENANCE_INTERCEPTION_OPENAI_TASKS_H_
#define DT_PROVENANCE_INTERCEPTION_OPENAI_TASKS_H_

#include <chimaera/admin/admin_tasks.h>
#include <chimaera/chimaera.h>
#include <chimaera/config_manager.h>
#include <yaml-cpp/yaml.h>

#include "autogen/dt_intercept_openai_methods.h"

namespace dt_provenance::interception::openai {

using MonitorTask = chimaera::admin::MonitorTask;

/**
 * CreateParams for the OpenAI Interception ChiMod
 */
struct CreateParams {
  chi::priv::string upstream_base_url_;

  static constexpr const char* chimod_lib_name =
      "dt_provenance_dt_intercept_openai";

  CreateParams() : upstream_base_url_(HSHM_MALLOC) {
    upstream_base_url_ = "https://api.openai.com";
  }

  explicit CreateParams(const std::string& url)
      : upstream_base_url_(HSHM_MALLOC) {
    upstream_base_url_ = url;
  }

  template <class Archive>
  void serialize(Archive& ar) {
    ar(upstream_base_url_);
  }

  void LoadConfig(const chi::PoolConfig& pool_config) {
    YAML::Node config = YAML::Load(pool_config.config_);
    if (config["upstream_base_url"]) {
      upstream_base_url_ = config["upstream_base_url"].as<std::string>();
    }
  }
};

using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;
using DestroyTask = chimaera::admin::DestroyTask;

/**
 * InterceptAndForwardTask — forward a request to OpenAI and capture the
 * interaction
 */
struct InterceptAndForwardTask : public chi::Task {
  // IN fields
  IN chi::priv::string session_id_;
  IN chi::priv::string path_;
  IN chi::priv::string headers_json_;
  IN chi::priv::string request_body_;
  IN chi::u64 request_time_ns_;

  // OUT fields
  OUT chi::i32 response_status_;
  OUT chi::priv::string response_headers_json_;
  OUT chi::priv::string response_body_;
  OUT double latency_ms_;
  OUT double ttft_ms_;

  /** SHM default constructor */
  InterceptAndForwardTask()
      : chi::Task(),
        session_id_(HSHM_MALLOC),
        path_(HSHM_MALLOC),
        headers_json_(HSHM_MALLOC),
        request_body_(HSHM_MALLOC),
        request_time_ns_(0),
        response_status_(0),
        response_headers_json_(HSHM_MALLOC),
        response_body_(HSHM_MALLOC),
        latency_ms_(0),
        ttft_ms_(0) {}

  /** Emplace constructor */
  explicit InterceptAndForwardTask(
      const chi::TaskId& task_node, const chi::PoolId& pool_id,
      const chi::PoolQuery& pool_query, const std::string& session_id,
      const std::string& path, const std::string& headers_json,
      const std::string& request_body, chi::u64 request_time_ns)
      : chi::Task(task_node, pool_id, pool_query, 10),
        session_id_(HSHM_MALLOC),
        path_(HSHM_MALLOC),
        headers_json_(HSHM_MALLOC),
        request_body_(HSHM_MALLOC),
        request_time_ns_(request_time_ns),
        response_status_(0),
        response_headers_json_(HSHM_MALLOC),
        response_body_(HSHM_MALLOC),
        latency_ms_(0),
        ttft_ms_(0) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kInterceptAndForward;
    task_flags_.Clear();
    pool_query_ = pool_query;

    session_id_ = session_id;
    path_ = path;
    headers_json_ = headers_json;
    request_body_ = request_body;
  }

  template <typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    ar(session_id_, path_, headers_json_, request_body_, request_time_ns_);
  }

  template <typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(response_status_, response_headers_json_, response_body_, latency_ms_,
       ttft_ms_);
  }

  void Copy(const hipc::FullPtr<InterceptAndForwardTask>& other) {
    Task::Copy(other.template Cast<Task>());
    session_id_ = other->session_id_;
    path_ = other->path_;
    headers_json_ = other->headers_json_;
    request_body_ = other->request_body_;
    request_time_ns_ = other->request_time_ns_;
    response_status_ = other->response_status_;
    response_headers_json_ = other->response_headers_json_;
    response_body_ = other->response_body_;
    latency_ms_ = other->latency_ms_;
    ttft_ms_ = other->ttft_ms_;
  }

  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<InterceptAndForwardTask>());
  }
};

}  // namespace dt_provenance::interception::openai

#endif  // DT_PROVENANCE_INTERCEPTION_OPENAI_TASKS_H_
