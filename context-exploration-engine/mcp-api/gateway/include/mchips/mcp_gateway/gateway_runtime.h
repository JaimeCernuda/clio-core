/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCP_GATEWAY_GATEWAY_RUNTIME_H_
#define MCHIPS_MCP_GATEWAY_GATEWAY_RUNTIME_H_

#include <chimaera/container.h>

#include <mchips/mcp_gateway/gateway_client.h>
#include <mchips/mcp_gateway/gateway_tasks.h>
#include <mchips/mcp_gateway/http_server.h>
#include <mchips/mcp_gateway/mchip_router.h>
#include <mchips/mcp_gateway/session_manager.h>
#include <mchips/mcp_gateway/sse_writer.h>

namespace mchips::mcp_gateway {

// Forward declarations
struct HandleHttpRequestTask;
struct InitializeSessionTask;
struct CloseSessionTask;
struct StartHttpServerTask;
struct StopHttpServerTask;

/// Gateway ChiMod runtime — the MCP HTTP frontend for IOWarp.
///
/// Owns the HttpServer, SessionManager, SseWriter, and MchipRouter.
/// Routes incoming JSON-RPC requests to the appropriate MChiP ChiMod
/// via the Chimaera task system.
///
/// Data flow:
///   HTTP POST /mcp → HttpServer → HandleHttpRequestTask → Chimaera worker
///     → Parse JSON-RPC → MchipRouter::Route("cte__put_blob")
///     → Split on "__" → MchipClient::AsyncCallMcpTool(cte_pool, "put_blob", args)
///     → co_await → CTE MChiP handles it (calls real CTE APIs)
///     → Response flows back → JSON-RPC response → HTTP response
class Runtime : public chi::Container {
 public:
  using CreateParams = mchips::mcp_gateway::CreateParams;

  Runtime() = default;
  ~Runtime() override = default;

  void Init(const chi::PoolId& pool_id, const std::string& pool_name,
            chi::u32 container_id = 0) override;

  chi::TaskResume Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                      chi::RunContext& rctx) override;

  //===========================================================================
  // Method implementations
  //===========================================================================

  chi::TaskResume Create(hipc::FullPtr<CreateTask> task,
                         chi::RunContext& rctx);
  chi::TaskResume Destroy(hipc::FullPtr<chi::admin::DestroyPoolTask> task,
                          chi::RunContext& rctx);
  chi::TaskResume HandleHttpRequest(hipc::FullPtr<HandleHttpRequestTask> task,
                                    chi::RunContext& rctx);
  chi::TaskResume InitializeSession(hipc::FullPtr<InitializeSessionTask> task,
                                    chi::RunContext& rctx);
  chi::TaskResume CloseSession(hipc::FullPtr<CloseSessionTask> task,
                               chi::RunContext& rctx);
  chi::TaskResume StartHttpServer(hipc::FullPtr<StartHttpServerTask> task,
                                  chi::RunContext& rctx);
  chi::TaskResume StopHttpServer(hipc::FullPtr<StopHttpServerTask> task,
                                 chi::RunContext& rctx);

  chi::u64 GetWorkRemaining() const override;

  //===========================================================================
  // Serialization (Container virtual API)
  //===========================================================================

  void SaveTask(chi::u32 method, chi::SaveTaskArchive& archive,
                hipc::FullPtr<chi::Task> task_ptr) override;
  void LoadTask(chi::u32 method, chi::LoadTaskArchive& archive,
                hipc::FullPtr<chi::Task> task_ptr) override;
  hipc::FullPtr<chi::Task> AllocLoadTask(
      chi::u32 method, chi::LoadTaskArchive& archive) override;
  void LocalLoadTask(chi::u32 method, chi::LocalLoadTaskArchive& archive,
                     hipc::FullPtr<chi::Task> task_ptr) override;
  hipc::FullPtr<chi::Task> LocalAllocLoadTask(
      chi::u32 method, chi::LocalLoadTaskArchive& archive) override;
  void LocalSaveTask(chi::u32 method, chi::LocalSaveTaskArchive& archive,
                     hipc::FullPtr<chi::Task> task_ptr) override;
  hipc::FullPtr<chi::Task> NewCopyTask(
      chi::u32 method, hipc::FullPtr<chi::Task> orig_task_ptr,
      bool deep) override;
  hipc::FullPtr<chi::Task> NewTask(chi::u32 method) override;
  void Aggregate(chi::u32 method, hipc::FullPtr<chi::Task> orig_task,
                 const hipc::FullPtr<chi::Task>& replica_task) override;
  void DelTask(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) override;

 private:
  Client client_;
  HttpServer http_server_;
  SessionManager session_manager_;
  SseWriter sse_writer_;
  MchipRouter router_;
};

}  // namespace mchips::mcp_gateway

#endif  // MCHIPS_MCP_GATEWAY_GATEWAY_RUNTIME_H_
