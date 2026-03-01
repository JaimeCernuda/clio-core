/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_GATEWAY_GATEWAY_TASKS_H_
#define MCHIPS_MCP_GATEWAY_GATEWAY_TASKS_H_

#include <chimaera/task.h>
#include <chimaera/chimaera.h>
#include <chimaera/admin/admin_tasks.h>
#include <yaml-cpp/yaml.h>

#include <mchips/mcp_gateway/autogen/mcp_gateway_methods.h>

namespace mchips::mcp_gateway {

//=============================================================================
// CreateParams
//=============================================================================

/// Parameters for creating the MCP Gateway pool.
struct CreateParams {
  chi::u32 http_port_;
  chi::u32 http_threads_;

  static constexpr const char* chimod_lib_name = "mchips_mcp_gateway";

  CreateParams(chi::u32 http_port = 8080, chi::u32 http_threads = 4)
      : http_port_(http_port), http_threads_(http_threads) {}

  void LoadConfig(const chi::PoolConfig& pool_config) {
    YAML::Node config = YAML::Load(pool_config.config_);
    if (config["http_port"]) {
      http_port_ = config["http_port"].as<chi::u32>();
    }
    if (config["http_threads"]) {
      http_threads_ = config["http_threads"].as<chi::u32>();
    }
  }

  template <class Archive>
  void serialize(Archive& ar) {
    ar(http_port_, http_threads_);
  }
};

/// CreateTask for the gateway pool.
using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;

//=============================================================================
// HandleHttpRequestTask
//=============================================================================

/// Task for processing an incoming HTTP request through the gateway.
///
/// The HTTP server thread submits this task and blocks on the future.
/// A Chimaera worker picks it up, parses JSON-RPC, routes to the
/// appropriate MChiP, and writes the response.
struct HandleHttpRequestTask : public chi::Task {
  IN chi::priv::string request_body_;   ///< Raw HTTP request body (JSON-RPC)
  IN chi::priv::string session_id_;     ///< MCP-Session-Id header value
  OUT chi::priv::string response_body_; ///< JSON-RPC response to send back
  OUT chi::u32 http_status_;            ///< HTTP status code (200, 400, etc.)

  /** SHM default constructor. */
  HandleHttpRequestTask()
      : chi::Task(),
        request_body_(HSHM_MALLOC),
        session_id_(HSHM_MALLOC),
        response_body_(HSHM_MALLOC),
        http_status_(200) {}

  /** Emplace constructor. */
  explicit HandleHttpRequestTask(
      const chi::TaskId& task_node,
      const chi::PoolId& pool_id,
      const chi::PoolQuery& pool_query,
      const std::string& request_body,
      const std::string& session_id)
      : chi::Task(task_node, pool_id, pool_query,
                  Method::kHandleHttpRequest),
        request_body_(HSHM_MALLOC, request_body),
        session_id_(HSHM_MALLOC, session_id),
        response_body_(HSHM_MALLOC),
        http_status_(200) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kHandleHttpRequest;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  ~HandleHttpRequestTask() = default;

  template <typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    ar(request_body_, session_id_);
  }

  template <typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(response_body_, http_status_);
  }

  void Copy(const hipc::FullPtr<HandleHttpRequestTask>& other) {
    Task::Copy(other.template Cast<Task>());
    request_body_ = other->request_body_;
    session_id_ = other->session_id_;
    response_body_ = other->response_body_;
    http_status_ = other->http_status_;
  }

  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<HandleHttpRequestTask>());
  }
};

//=============================================================================
// InitializeSessionTask
//=============================================================================

/// Task for the MCP initialize handshake.
struct InitializeSessionTask : public chi::Task {
  IN chi::priv::string request_json_;    ///< Initialize request params
  OUT chi::priv::string response_json_;  ///< InitializeResult JSON
  OUT chi::priv::string session_id_;     ///< Newly created session ID

  InitializeSessionTask()
      : chi::Task(),
        request_json_(HSHM_MALLOC),
        response_json_(HSHM_MALLOC),
        session_id_(HSHM_MALLOC) {}

  explicit InitializeSessionTask(
      const chi::TaskId& task_node,
      const chi::PoolId& pool_id,
      const chi::PoolQuery& pool_query,
      const std::string& request_json)
      : chi::Task(task_node, pool_id, pool_query,
                  Method::kInitializeSession),
        request_json_(HSHM_MALLOC, request_json),
        response_json_(HSHM_MALLOC),
        session_id_(HSHM_MALLOC) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kInitializeSession;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  ~InitializeSessionTask() = default;

  template <typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    ar(request_json_);
  }

  template <typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(response_json_, session_id_);
  }

  void Copy(const hipc::FullPtr<InitializeSessionTask>& other) {
    Task::Copy(other.template Cast<Task>());
    request_json_ = other->request_json_;
    response_json_ = other->response_json_;
    session_id_ = other->session_id_;
  }

  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<InitializeSessionTask>());
  }
};

//=============================================================================
// CloseSessionTask
//=============================================================================

/// Task for cleaning up an MCP session (HTTP DELETE).
struct CloseSessionTask : public chi::Task {
  IN chi::priv::string session_id_;
  OUT chi::u32 success_;

  CloseSessionTask()
      : chi::Task(),
        session_id_(HSHM_MALLOC),
        success_(0) {}

  explicit CloseSessionTask(
      const chi::TaskId& task_node,
      const chi::PoolId& pool_id,
      const chi::PoolQuery& pool_query,
      const std::string& session_id)
      : chi::Task(task_node, pool_id, pool_query,
                  Method::kCloseSession),
        session_id_(HSHM_MALLOC, session_id),
        success_(0) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kCloseSession;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  ~CloseSessionTask() = default;

  template <typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    ar(session_id_);
  }

  template <typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(success_);
  }

  void Copy(const hipc::FullPtr<CloseSessionTask>& other) {
    Task::Copy(other.template Cast<Task>());
    session_id_ = other->session_id_;
    success_ = other->success_;
  }

  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<CloseSessionTask>());
  }
};

//=============================================================================
// StartHttpServerTask / StopHttpServerTask
//=============================================================================

/// Task for starting the HTTP listener.
struct StartHttpServerTask : public chi::Task {
  IN chi::priv::string host_;
  IN chi::u32 port_;
  IN chi::u32 num_threads_;
  OUT chi::u32 success_;

  StartHttpServerTask()
      : chi::Task(),
        host_(HSHM_MALLOC),
        port_(8080),
        num_threads_(4),
        success_(0) {}

  explicit StartHttpServerTask(
      const chi::TaskId& task_node,
      const chi::PoolId& pool_id,
      const chi::PoolQuery& pool_query,
      const std::string& host,
      chi::u32 port,
      chi::u32 num_threads)
      : chi::Task(task_node, pool_id, pool_query,
                  Method::kStartHttpServer),
        host_(HSHM_MALLOC, host),
        port_(port),
        num_threads_(num_threads),
        success_(0) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kStartHttpServer;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  ~StartHttpServerTask() = default;

  template <typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    ar(host_, port_, num_threads_);
  }

  template <typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(success_);
  }

  void Copy(const hipc::FullPtr<StartHttpServerTask>& other) {
    Task::Copy(other.template Cast<Task>());
    host_ = other->host_;
    port_ = other->port_;
    num_threads_ = other->num_threads_;
    success_ = other->success_;
  }

  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<StartHttpServerTask>());
  }
};

/// Task for stopping the HTTP listener.
struct StopHttpServerTask : public chi::Task {
  OUT chi::u32 success_;

  StopHttpServerTask()
      : chi::Task(),
        success_(0) {}

  explicit StopHttpServerTask(
      const chi::TaskId& task_node,
      const chi::PoolId& pool_id,
      const chi::PoolQuery& pool_query)
      : chi::Task(task_node, pool_id, pool_query,
                  Method::kStopHttpServer),
        success_(0) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kStopHttpServer;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  ~StopHttpServerTask() = default;

  template <typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
  }

  template <typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(success_);
  }

  void Copy(const hipc::FullPtr<StopHttpServerTask>& other) {
    Task::Copy(other.template Cast<Task>());
    success_ = other->success_;
  }

  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<StopHttpServerTask>());
  }
};

}  // namespace mchips::mcp_gateway

#endif  // MCHIPS_MCP_GATEWAY_GATEWAY_TASKS_H_
