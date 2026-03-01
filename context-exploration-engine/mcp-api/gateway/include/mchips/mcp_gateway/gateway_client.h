/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_GATEWAY_GATEWAY_CLIENT_H_
#define MCHIPS_MCP_GATEWAY_GATEWAY_CLIENT_H_

#include <chimaera/container.h>

#include <mchips/mcp_gateway/gateway_tasks.h>

namespace mchips::mcp_gateway {

/// ChiMod client for submitting gateway tasks.
///
/// Used by the HTTP server threads to submit HandleHttpRequestTask
/// to Chimaera for coroutine-based processing.
class Client : public chi::ContainerClient {
 public:
  Client() = default;

  explicit Client(const chi::PoolId& pool_id) { Init(pool_id); }

  /// Create the gateway pool (async).
  chi::Future<CreateTask> AsyncCreate(const chi::PoolQuery& pool_query,
                                      const std::string& pool_name,
                                      const chi::PoolId& custom_pool_id,
                                      CreateParams params = CreateParams{}) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<CreateTask>(
        chi::CreateTaskId(),
        chi::kAdminPoolId,
        pool_query,
        CreateParams::chimod_lib_name,
        pool_name,
        custom_pool_id,
        this,
        std::move(params));
    return ipc_manager->Send(task);
  }

  /// Submit an HTTP request for processing (async).
  chi::Future<HandleHttpRequestTask> AsyncHandleHttpRequest(
      const chi::PoolQuery& pool_query,
      const std::string& request_body,
      const std::string& session_id) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<HandleHttpRequestTask>(
        chi::CreateTaskId(), pool_id_, pool_query,
        request_body, session_id);
    return ipc_manager->Send(task);
  }

  /// Initialize an MCP session (async).
  chi::Future<InitializeSessionTask> AsyncInitializeSession(
      const chi::PoolQuery& pool_query,
      const std::string& request_json) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<InitializeSessionTask>(
        chi::CreateTaskId(), pool_id_, pool_query,
        request_json);
    return ipc_manager->Send(task);
  }

  /// Close an MCP session (async).
  chi::Future<CloseSessionTask> AsyncCloseSession(
      const chi::PoolQuery& pool_query,
      const std::string& session_id) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<CloseSessionTask>(
        chi::CreateTaskId(), pool_id_, pool_query,
        session_id);
    return ipc_manager->Send(task);
  }

  /// Start the HTTP server (async).
  chi::Future<StartHttpServerTask> AsyncStartHttpServer(
      const chi::PoolQuery& pool_query,
      const std::string& host,
      chi::u32 port,
      chi::u32 num_threads) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<StartHttpServerTask>(
        chi::CreateTaskId(), pool_id_, pool_query,
        host, port, num_threads);
    return ipc_manager->Send(task);
  }

  /// Stop the HTTP server (async).
  chi::Future<StopHttpServerTask> AsyncStopHttpServer(
      const chi::PoolQuery& pool_query) {
    auto* ipc_manager = CHI_IPC;
    auto task = ipc_manager->NewTask<StopHttpServerTask>(
        chi::CreateTaskId(), pool_id_, pool_query);
    return ipc_manager->Send(task);
  }
};

}  // namespace mchips::mcp_gateway

#endif  // MCHIPS_MCP_GATEWAY_GATEWAY_CLIENT_H_
