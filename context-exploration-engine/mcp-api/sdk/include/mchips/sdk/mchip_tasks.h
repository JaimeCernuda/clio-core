/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_SDK_MCHIP_TASKS_H_
#define MCHIPS_SDK_MCHIP_TASKS_H_

#include <chimaera/task.h>
#include <chimaera/chimaera.h>

namespace mchips::sdk {

//=============================================================================
// Method IDs — every MChiP ChiMod implements these two standard methods
//=============================================================================

namespace MchipMethod {
/// Returns a JSON array of ToolDefinitions this MChiP provides.
GLOBAL_CONST chi::u32 kListMcpTools = 10;
/// Dispatches a named tool call and returns a JSON result.
GLOBAL_CONST chi::u32 kCallMcpTool = 11;
}  // namespace MchipMethod

//=============================================================================
// ListMcpToolsTask
//=============================================================================

/// Task for retrieving the list of MCP tools a MChiP provides.
///
/// The gateway sends this to each discovered MChiP during aggregation.
/// The MChiP returns a JSON string containing an array of ToolDefinitions.
struct ListMcpToolsTask : public chi::Task {
  OUT chi::priv::string result_json_;  ///< JSON array of ToolDefinitions

  /** SHM default constructor. */
  ListMcpToolsTask()
      : chi::Task(),
        result_json_(HSHM_MALLOC) {}

  /** Emplace constructor. */
  explicit ListMcpToolsTask(
      const chi::TaskId& task_node,
      const chi::PoolId& pool_id,
      const chi::PoolQuery& pool_query)
      : chi::Task(task_node, pool_id, pool_query,
                  MchipMethod::kListMcpTools),
        result_json_(HSHM_MALLOC) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = MchipMethod::kListMcpTools;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  ~ListMcpToolsTask() = default;

  /** Serialize input parameters for network transfer. */
  template <typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
  }

  /** Serialize output parameters for network transfer. */
  template <typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(result_json_);
  }

  /** Deep copy from another task. */
  void Copy(const hipc::FullPtr<ListMcpToolsTask>& other) {
    Task::Copy(other.template Cast<Task>());
    result_json_ = other->result_json_;
  }

  /** Aggregate replica results. */
  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<ListMcpToolsTask>());
  }
};

//=============================================================================
// CallMcpToolTask
//=============================================================================

/// Task for invoking a specific MCP tool on a MChiP.
///
/// The gateway routes `tools/call` requests by splitting the qualified name
/// (e.g., "cte__put_blob") and dispatching the local tool name ("put_blob")
/// to the target MChiP's pool.
struct CallMcpToolTask : public chi::Task {
  IN chi::priv::string tool_name_;     ///< Local tool name (e.g., "put_blob")
  IN chi::priv::string args_json_;     ///< JSON object of tool arguments
  OUT chi::priv::string result_json_;  ///< JSON CallToolResult
  OUT chi::u32 is_error_;              ///< 1 if tool returned an error, 0 otherwise

  /** SHM default constructor. */
  CallMcpToolTask()
      : chi::Task(),
        tool_name_(HSHM_MALLOC),
        args_json_(HSHM_MALLOC),
        result_json_(HSHM_MALLOC),
        is_error_(0) {}

  /** Emplace constructor. */
  explicit CallMcpToolTask(
      const chi::TaskId& task_node,
      const chi::PoolId& pool_id,
      const chi::PoolQuery& pool_query,
      const std::string& tool_name,
      const std::string& args_json)
      : chi::Task(task_node, pool_id, pool_query,
                  MchipMethod::kCallMcpTool),
        tool_name_(HSHM_MALLOC, tool_name),
        args_json_(HSHM_MALLOC, args_json),
        result_json_(HSHM_MALLOC),
        is_error_(0) {
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = MchipMethod::kCallMcpTool;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  ~CallMcpToolTask() = default;

  /** Serialize input parameters for network transfer. */
  template <typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    ar(tool_name_, args_json_);
  }

  /** Serialize output parameters for network transfer. */
  template <typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(result_json_, is_error_);
  }

  /** Deep copy from another task. */
  void Copy(const hipc::FullPtr<CallMcpToolTask>& other) {
    Task::Copy(other.template Cast<Task>());
    tool_name_ = other->tool_name_;
    args_json_ = other->args_json_;
    result_json_ = other->result_json_;
    is_error_ = other->is_error_;
  }

  /** Aggregate replica results. */
  void Aggregate(const hipc::FullPtr<chi::Task>& other_base) {
    Task::Aggregate(other_base);
    Copy(other_base.template Cast<CallMcpToolTask>());
  }
};

}  // namespace mchips::sdk

#endif  // MCHIPS_SDK_MCHIP_TASKS_H_
