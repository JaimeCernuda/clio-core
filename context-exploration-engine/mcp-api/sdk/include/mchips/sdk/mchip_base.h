/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_SDK_MCHIP_BASE_H_
#define MCHIPS_SDK_MCHIP_BASE_H_

#include <chimaera/container.h>

#include <mchips/sdk/mchip_tasks.h>
#include <mchips/sdk/tool_registrar.h>

namespace mchips::sdk {

/// Base class for all MChiP ChiMod runtimes.
///
/// Provides a ToolRegistrar and default implementations of the two
/// standard MChiP methods (kListMcpTools, kCallMcpTool). Concrete
/// MChiPs override RegisterTools() to populate the registrar during
/// Create(), then rely on the base class for dispatch.
///
/// Example (in a concrete MChiP runtime):
/// ```cpp
/// class MchipCteRuntime : public mchips::sdk::MchipBase {
///  public:
///   using CreateParams = mchips::mchip_cte::CreateParams;
///
///  protected:
///   void RegisterTools() override {
///     registrar_.Register(
///         ToolBuilder("put_blob").Description("...").Build(),
///         [this](const json& args) { return HandlePutBlob(args); });
///   }
/// };
/// ```
class MchipBase : public chi::Container {
 public:
  MchipBase() = default;
  virtual ~MchipBase() = default;

  /// Handle the ListMcpTools task.
  ///
  /// Serializes all registered tool definitions to JSON and writes
  /// the result to the task's output field.
  chi::TaskResume ListMcpTools(hipc::FullPtr<ListMcpToolsTask> task,
                               chi::RunContext& rctx) {
    auto tools_json = registrar_.ListTools();
    task->result_json_ = tools_json.dump();
    (void)rctx;
    co_return;
  }

  /// Handle the CallMcpTool task.
  ///
  /// Looks up the tool by name and invokes its handler. If the tool
  /// is not found, returns an error result.
  chi::TaskResume CallMcpTool(hipc::FullPtr<CallMcpToolTask> task,
                              chi::RunContext& rctx) {
    std::string tool_name(task->tool_name_.c_str());
    auto args = protocol::json::parse(
        std::string(task->args_json_.c_str()),
        nullptr, false);

    if (args.is_discarded()) {
      protocol::json error_result = {
          {"content",
           {{{"type", "text"},
             {"text", "Invalid JSON arguments"}}}},
          {"isError", true}};
      task->result_json_ = error_result.dump();
      task->is_error_ = 1;
      (void)rctx;
      co_return;
    }

    auto result = registrar_.Invoke(tool_name, args);

    task->result_json_ = result.dump();
    task->is_error_ = result.value("isError", false) ? 1 : 0;
    (void)rctx;
    co_return;
  }

 protected:
  /// Override this to register tools during Create().
  ///
  /// Called by the concrete MChiP's Create() handler after base
  /// initialization. Implementations should call registrar_.Register()
  /// for each tool they provide.
  virtual void RegisterTools() = 0;

  ToolRegistrar registrar_;  ///< Tool definitions + handlers
};

}  // namespace mchips::sdk

#endif  // MCHIPS_SDK_MCHIP_BASE_H_
