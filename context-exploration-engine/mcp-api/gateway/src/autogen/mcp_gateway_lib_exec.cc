/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Auto-generated execution implementation for mcp_gateway ChiMod.
 * Implements Container virtual APIs (Run, SaveTask, LoadTask, etc.)
 * using switch-case dispatch over method IDs.
 *
 * TODO(Phase B.4): Implement full dispatch for all gateway methods.
 */

#include "mchips/mcp_gateway/gateway_runtime.h"
#include "mchips/mcp_gateway/autogen/mcp_gateway_methods.h"

namespace mchips::mcp_gateway {

void Runtime::Init(const chi::PoolId& pool_id, const std::string& pool_name,
                   chi::u32 container_id) {
  chi::Container::Init(pool_id, pool_name, container_id);
  client_ = Client(pool_id);
}

chi::TaskResume Runtime::Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                             chi::RunContext& rctx) {
  switch (method) {
    case Method::kCreate: {
      co_await Create(task_ptr.template Cast<CreateTask>(), rctx);
      break;
    }
    case Method::kHandleHttpRequest: {
      co_await HandleHttpRequest(
          task_ptr.template Cast<HandleHttpRequestTask>(), rctx);
      break;
    }
    case Method::kInitializeSession: {
      co_await InitializeSession(
          task_ptr.template Cast<InitializeSessionTask>(), rctx);
      break;
    }
    case Method::kCloseSession: {
      co_await CloseSession(
          task_ptr.template Cast<CloseSessionTask>(), rctx);
      break;
    }
    case Method::kStartHttpServer: {
      co_await StartHttpServer(
          task_ptr.template Cast<StartHttpServerTask>(), rctx);
      break;
    }
    case Method::kStopHttpServer: {
      co_await StopHttpServer(
          task_ptr.template Cast<StopHttpServerTask>(), rctx);
      break;
    }
    default:
      break;
  }
  co_return;
}

// TODO(Phase B.4): Implement SaveTask, LoadTask, AllocLoadTask,
// LocalLoadTask, LocalAllocLoadTask, LocalSaveTask,
// NewCopyTask, NewTask, Aggregate, DelTask
// (same switch-case pattern, one case per method)

chi::u64 Runtime::GetWorkRemaining() const { return 0; }

void Runtime::SaveTask(chi::u32 method, chi::SaveTaskArchive& archive,
                       hipc::FullPtr<chi::Task> task_ptr) {
  // TODO(Phase B.4)
  (void)method; (void)archive; (void)task_ptr;
}

void Runtime::LoadTask(chi::u32 method, chi::LoadTaskArchive& archive,
                       hipc::FullPtr<chi::Task> task_ptr) {
  (void)method; (void)archive; (void)task_ptr;
}

hipc::FullPtr<chi::Task> Runtime::AllocLoadTask(
    chi::u32 method, chi::LoadTaskArchive& archive) {
  (void)method; (void)archive;
  return {};
}

void Runtime::LocalLoadTask(chi::u32 method,
                            chi::LocalLoadTaskArchive& archive,
                            hipc::FullPtr<chi::Task> task_ptr) {
  (void)method; (void)archive; (void)task_ptr;
}

hipc::FullPtr<chi::Task> Runtime::LocalAllocLoadTask(
    chi::u32 method, chi::LocalLoadTaskArchive& archive) {
  (void)method; (void)archive;
  return {};
}

void Runtime::LocalSaveTask(chi::u32 method,
                            chi::LocalSaveTaskArchive& archive,
                            hipc::FullPtr<chi::Task> task_ptr) {
  (void)method; (void)archive; (void)task_ptr;
}

hipc::FullPtr<chi::Task> Runtime::NewCopyTask(
    chi::u32 method, hipc::FullPtr<chi::Task> orig_task_ptr, bool deep) {
  (void)method; (void)orig_task_ptr; (void)deep;
  return {};
}

hipc::FullPtr<chi::Task> Runtime::NewTask(chi::u32 method) {
  (void)method;
  return {};
}

void Runtime::Aggregate(chi::u32 method, hipc::FullPtr<chi::Task> orig_task,
                        const hipc::FullPtr<chi::Task>& replica_task) {
  (void)method; (void)orig_task; (void)replica_task;
}

void Runtime::DelTask(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) {
  (void)method; (void)task_ptr;
}

}  // namespace mchips::mcp_gateway

CHI_TASK_CC(mchips::mcp_gateway::Runtime)
