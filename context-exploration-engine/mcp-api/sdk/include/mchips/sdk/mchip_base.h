/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_SDK_MCHIP_BASE_H_
#define MCHIPS_SDK_MCHIP_BASE_H_

#include <chimaera/container.h>
#include <chimaera/admin/admin_tasks.h>

#include <mchips/sdk/mchip_tasks.h>
#include <mchips/sdk/tool_registrar.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>

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
  /// is not found, returns an error result. Tracks call counts and latency.
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
      total_errors_.fetch_add(1, std::memory_order_relaxed);
      (void)rctx;
      co_return;
    }

    auto start = std::chrono::steady_clock::now();
    auto result = registrar_.Invoke(tool_name, args);
    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    total_calls_.fetch_add(1, std::memory_order_relaxed);
    total_latency_us_.fetch_add(static_cast<uint64_t>(us),
                                std::memory_order_relaxed);

    bool is_error = result.value("isError", false);
    if (is_error) {
      total_errors_.fetch_add(1, std::memory_order_relaxed);
    }

    // Track per-tool stats
    {
      std::lock_guard<std::mutex> lock(tool_stats_mutex_);
      auto& ts = tool_stats_[tool_name];
      ts.calls++;
      ts.total_us += static_cast<uint64_t>(us);
      if (is_error) ts.errors++;
    }

    task->result_json_ = result.dump();
    task->is_error_ = is_error ? 1 : 0;
    (void)rctx;
    co_return;
  }

  /// Handle Monitor queries for MChiP stats.
  ///
  /// Supported queries:
  ///   "mchip_stats" — aggregate call counts, latencies, errors
  ///   "tool_stats"  — per-tool breakdown
  chi::TaskResume Monitor(hipc::FullPtr<chimaera::admin::MonitorTask> task,
                          chi::RunContext& /*rctx*/) {
    if (task->query_ == "mchip_stats" || task->query_ == "tool_stats") {
      uint64_t calls = total_calls_.load(std::memory_order_relaxed);
      uint64_t lat = total_latency_us_.load(std::memory_order_relaxed);
      double avg_us = calls > 0
          ? static_cast<double>(lat) / static_cast<double>(calls)
          : 0.0;

      protocol::json result = {
          {"total_calls", calls},
          {"total_errors", total_errors_.load(std::memory_order_relaxed)},
          {"avg_latency_us", avg_us},
          {"total_latency_us", lat},
          {"num_tools", registrar_.ListTools().size()}
      };

      if (task->query_ == "tool_stats") {
        protocol::json per_tool = protocol::json::object();
        std::lock_guard<std::mutex> lock(tool_stats_mutex_);
        for (const auto& [name, ts] : tool_stats_) {
          double tool_avg = ts.calls > 0
              ? static_cast<double>(ts.total_us) /
                static_cast<double>(ts.calls)
              : 0.0;
          per_tool[name] = {
              {"calls", ts.calls},
              {"errors", ts.errors},
              {"avg_latency_us", tool_avg}
          };
        }
        result["tools"] = per_tool;
      }

      task->results_[container_id_] = result.dump();
    }
    task->SetReturnCode(0);
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

 private:
  /// Per-tool statistics.
  struct ToolCallStats {
    uint64_t calls = 0;
    uint64_t errors = 0;
    uint64_t total_us = 0;
  };

  std::atomic<uint64_t> total_calls_{0};
  std::atomic<uint64_t> total_errors_{0};
  std::atomic<uint64_t> total_latency_us_{0};
  std::mutex tool_stats_mutex_;
  std::unordered_map<std::string, ToolCallStats> tool_stats_;
};

}  // namespace mchips::sdk

#endif  // MCHIPS_SDK_MCHIP_BASE_H_
