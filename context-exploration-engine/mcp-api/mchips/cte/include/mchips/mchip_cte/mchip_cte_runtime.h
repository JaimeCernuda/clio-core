/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_MCHIP_CTE_RUNTIME_H_
#define MCHIPS_MCHIP_CTE_RUNTIME_H_

#include <mchips/mchip_cte/mchip_cte_client.h>
#include <mchips/mchip_cte/mchip_cte_tasks.h>
#include <mchips/sdk/mchip_base.h>

namespace mchips::mchip_cte {

/// CTE MChiP runtime — exposes 12 CTE storage tools via the MChiP interface.
///
/// Tools (from branch 57's Python MCP server):
///   put_blob, get_blob, get_blob_size, list_blobs_in_tag, delete_blob,
///   tag_query, blob_query, poll_telemetry_log, reorganize_blob,
///   initialize_cte_runtime, get_client_status, get_cte_types
///
/// Each tool handler calls real CTE APIs (AsyncGetOrCreateTag, AsyncPutBlob,
/// etc.) via inter-ChiMod task submission. If the CTE runtime is not
/// initialized, handlers return honest MCP errors (isError: true).
class Runtime : public sdk::MchipBase {
 public:
  using CreateParams = mchips::mchip_cte::CreateParams;

  Runtime() = default;
  ~Runtime() override = default;

  void Init(const chi::PoolId& pool_id, const std::string& pool_name,
            chi::u32 container_id = 0) override;

  chi::TaskResume Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                      chi::RunContext& rctx) override;

  chi::TaskResume Create(hipc::FullPtr<CreateTask> task,
                         chi::RunContext& rctx);
  chi::TaskResume Destroy(hipc::FullPtr<chi::admin::DestroyPoolTask> task,
                          chi::RunContext& rctx);

  chi::u64 GetWorkRemaining() const override;

  // Container virtual API (serialization)
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

 protected:
  /// Register all 12 CTE tools with the ToolRegistrar.
  void RegisterTools() override;

 private:
  Client client_;

  // Tool handler methods (each implements one CTE MCP tool)
  protocol::json HandlePutBlob(const protocol::json& args);
  protocol::json HandleGetBlob(const protocol::json& args);
  protocol::json HandleGetBlobSize(const protocol::json& args);
  protocol::json HandleListBlobsInTag(const protocol::json& args);
  protocol::json HandleDeleteBlob(const protocol::json& args);
  protocol::json HandleTagQuery(const protocol::json& args);
  protocol::json HandleBlobQuery(const protocol::json& args);
  protocol::json HandlePollTelemetryLog(const protocol::json& args);
  protocol::json HandleReorganizeBlob(const protocol::json& args);
  protocol::json HandleInitializeCteRuntime(const protocol::json& args);
  protocol::json HandleGetClientStatus(const protocol::json& args);
  protocol::json HandleGetCteTypes(const protocol::json& args);
};

}  // namespace mchips::mchip_cte

#endif  // MCHIPS_MCHIP_CTE_RUNTIME_H_
