/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mchip_cte/mchip_cte_runtime.h"

#include <mchips/protocol/schema_generator.h>
#include <wrp_cte/core/core_client.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace mchips::mchip_cte {

//=============================================================================
// Utility: base64 encode / decode
//=============================================================================

namespace {

static constexpr const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const std::vector<char>& data) {
  std::string result;
  size_t i = 0;
  unsigned char char3[3];
  unsigned char char4[4];
  size_t len = data.size();
  const auto* bytes = reinterpret_cast<const unsigned char*>(data.data());

  while (len--) {
    char3[i++] = *bytes++;
    if (i == 3) {
      char4[0] = (char3[0] & 0xfc) >> 2;
      char4[1] = ((char3[0] & 0x03) << 4) + ((char3[1] & 0xf0) >> 4);
      char4[2] = ((char3[1] & 0x0f) << 2) + ((char3[2] & 0xc0) >> 6);
      char4[3] = char3[2] & 0x3f;
      for (i = 0; i < 4; i++) result += kBase64Chars[char4[i]];
      i = 0;
    }
  }

  if (i) {
    for (size_t j = i; j < 3; j++) char3[j] = '\0';
    char4[0] = (char3[0] & 0xfc) >> 2;
    char4[1] = ((char3[0] & 0x03) << 4) + ((char3[1] & 0xf0) >> 4);
    char4[2] = ((char3[1] & 0x0f) << 2) + ((char3[2] & 0xc0) >> 6);
    char4[3] = char3[2] & 0x3f;
    for (size_t j = 0; j < i + 1; j++) result += kBase64Chars[char4[j]];
    while (i++ < 3) result += '=';
  }
  return result;
}

std::vector<char> Base64Decode(const std::string& encoded) {
  static const std::string chars(kBase64Chars);
  std::vector<char> result;
  unsigned char char3[3];
  unsigned char char4[4];
  size_t i = 0;

  for (char c : encoded) {
    if (c == '=') break;
    auto pos = chars.find(c);
    if (pos == std::string::npos) continue;
    char4[i++] = static_cast<unsigned char>(pos);
    if (i == 4) {
      char3[0] = (char4[0] << 2) + ((char4[1] & 0x30) >> 4);
      char3[1] = ((char4[1] & 0xf) << 4) + ((char4[2] & 0x3c) >> 2);
      char3[2] = ((char4[2] & 0x3) << 6) + char4[3];
      for (size_t j = 0; j < 3; j++) result.push_back(char3[j]);
      i = 0;
    }
  }

  if (i) {
    for (size_t j = i; j < 4; j++) char4[j] = 0;
    char3[0] = (char4[0] << 2) + ((char4[1] & 0x30) >> 4);
    char3[1] = ((char4[1] & 0xf) << 4) + ((char4[2] & 0x3c) >> 2);
    for (size_t j = 0; j < i - 1; j++) result.push_back(char3[j]);
  }
  return result;
}

/// Build a successful MCP tool result with a single text message.
protocol::json MakeTextResult(const std::string& text) {
  return protocol::json{
      {"content", {{{"type", "text"}, {"text", text}}}},
      {"isError", false}};
}

/// Build an error MCP tool result.
protocol::json MakeErrorResult(const std::string& error_msg) {
  return protocol::json{
      {"content", {{{"type", "text"}, {"text", error_msg}}}},
      {"isError", true}};
}

/// Return true if the WRP_CTE global client is initialized.
bool IsCteInitialized() {
  return WRP_CTE_CLIENT != nullptr;
}

}  // namespace

//=============================================================================
// RegisterTools — populate the ToolRegistrar with all 12 CTE tools
//=============================================================================

void Runtime::RegisterTools() {
  using namespace protocol;

  // 1. put_blob
  registrar_.Register(
      ToolBuilder("put_blob")
          .Description("Store a blob in CTE with automatic tier placement")
          .AddParam("tag_name", SchemaType::String, "Tag namespace", true)
          .AddParam("blob_name", SchemaType::String, "Blob identifier", true)
          .AddParam("data", SchemaType::String,
                    "Base64-encoded blob data", true)
          .AddParam("priority", SchemaType::Number,
                    "Placement priority: 1.0=RAM, 0.5=SSD, 0.0=archive", false)
          .Annotations(ToolAnnotations{
              .readOnlyHint = false, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.5})
          .Build(),
      [this](const json& args) { return HandlePutBlob(args); });

  // 2. get_blob
  registrar_.Register(
      ToolBuilder("get_blob")
          .Description("Retrieve a blob from CTE as base64-encoded data")
          .AddParam("tag_name", SchemaType::String, "Tag namespace", true)
          .AddParam("blob_name", SchemaType::String, "Blob identifier", true)
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.5})
          .Build(),
      [this](const json& args) { return HandleGetBlob(args); });

  // 3. get_blob_size
  registrar_.Register(
      ToolBuilder("get_blob_size")
          .Description("Get the size of a blob in bytes")
          .AddParam("tag_name", SchemaType::String, "Tag namespace", true)
          .AddParam("blob_name", SchemaType::String, "Blob identifier", true)
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.5})
          .Build(),
      [this](const json& args) { return HandleGetBlobSize(args); });

  // 4. list_blobs_in_tag
  registrar_.Register(
      ToolBuilder("list_blobs_in_tag")
          .Description("List all blob names in a tag namespace")
          .AddParam("tag_name", SchemaType::String, "Tag namespace", true)
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.3})
          .Build(),
      [this](const json& args) { return HandleListBlobsInTag(args); });

  // 5. delete_blob
  registrar_.Register(
      ToolBuilder("delete_blob")
          .Description("Delete a blob from CTE")
          .AddParam("tag_name", SchemaType::String, "Tag namespace", true)
          .AddParam("blob_name", SchemaType::String, "Blob identifier", true)
          .Annotations(ToolAnnotations{
              .readOnlyHint = false, .destructiveHint = true,
              .idempotentHint = true, .priority = 0.3})
          .Build(),
      [this](const json& args) { return HandleDeleteBlob(args); });

  // 6. tag_query
  registrar_.Register(
      ToolBuilder("tag_query")
          .Description("Query tag names matching a regex pattern")
          .AddParam("pattern", SchemaType::String, "Tag name regex", true)
          .AddParam("max_results", SchemaType::Integer,
                    "Maximum results (0=unlimited)", false)
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.3})
          .Build(),
      [this](const json& args) { return HandleTagQuery(args); });

  // 7. blob_query
  registrar_.Register(
      ToolBuilder("blob_query")
          .Description("Query blobs matching tag and blob regex patterns")
          .AddParam("tag_pattern", SchemaType::String, "Tag name regex", true)
          .AddParam("blob_pattern", SchemaType::String, "Blob name regex", true)
          .AddParam("max_results", SchemaType::Integer,
                    "Maximum results (0=unlimited)", false)
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.3})
          .Build(),
      [this](const json& args) { return HandleBlobQuery(args); });

  // 8. poll_telemetry_log
  registrar_.Register(
      ToolBuilder("poll_telemetry_log")
          .Description("Poll CTE telemetry events since a logical timestamp")
          .AddParam("min_logical_time", SchemaType::Integer,
                    "Minimum logical time (0=all events)", false)
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = false, .priority = 0.2})
          .Build(),
      [this](const json& args) { return HandlePollTelemetryLog(args); });

  // 9. reorganize_blob
  registrar_.Register(
      ToolBuilder("reorganize_blob")
          .Description("Move a blob to a different storage tier")
          .AddParam("tag_name", SchemaType::String, "Tag namespace", true)
          .AddParam("blob_name", SchemaType::String, "Blob identifier", true)
          .AddParam("target_tier", SchemaType::Number,
                    "Target score: 1.0=RAM, 0.5=SSD, 0.0=archive", false)
          .Annotations(ToolAnnotations{
              .readOnlyHint = false, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.4})
          .Build(),
      [this](const json& args) { return HandleReorganizeBlob(args); });

  // 10. initialize_cte_runtime
  registrar_.Register(
      ToolBuilder("initialize_cte_runtime")
          .Description("Initialize the CTE runtime connection")
          .AddParam("config_path", SchemaType::String,
                    "Path to Chimaera config YAML (optional)", false)
          .Annotations(ToolAnnotations{
              .readOnlyHint = false, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.8})
          .Build(),
      [this](const json& args) { return HandleInitializeCteRuntime(args); });

  // 11. get_client_status
  registrar_.Register(
      ToolBuilder("get_client_status")
          .Description("Get the current CTE client initialization status")
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.5})
          .Build(),
      [this](const json& args) { return HandleGetClientStatus(args); });

  // 12. get_cte_types
  registrar_.Register(
      ToolBuilder("get_cte_types")
          .Description("List CTE storage tier types and their characteristics")
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.2})
          .Build(),
      [this](const json& args) { return HandleGetCteTypes(args); });
}

//=============================================================================
// Tool handlers
//=============================================================================

/// Store a blob in CTE.
protocol::json Runtime::HandlePutBlob(const protocol::json& args) {
  if (!IsCteInitialized()) {
    return MakeErrorResult(
        "CTE runtime not initialized — call cte__initialize_cte_runtime first");
  }

  try {
    auto tag_name = args.at("tag_name").get<std::string>();
    auto blob_name = args.at("blob_name").get<std::string>();
    auto data_b64 = args.at("data").get<std::string>();
    auto priority = static_cast<float>(args.value("priority", 0.5));

    auto data = Base64Decode(data_b64);
    if (data.empty()) {
      return MakeErrorResult("Failed to decode base64 data");
    }

    wrp_cte::core::Tag tag(tag_name);
    tag.PutBlob(blob_name, data.data(), data.size(), 0, priority);

    return MakeTextResult(
        "Blob '" + blob_name + "' stored in tag '" + tag_name +
        "' (" + std::to_string(data.size()) + " bytes, priority=" +
        std::to_string(priority) + ")");
  } catch (const std::exception& e) {
    return MakeErrorResult(std::string("put_blob error: ") + e.what());
  }
}

/// Retrieve a blob from CTE.
protocol::json Runtime::HandleGetBlob(const protocol::json& args) {
  if (!IsCteInitialized()) {
    return MakeErrorResult(
        "CTE runtime not initialized — call cte__initialize_cte_runtime first");
  }

  try {
    auto tag_name = args.at("tag_name").get<std::string>();
    auto blob_name = args.at("blob_name").get<std::string>();

    wrp_cte::core::Tag tag(tag_name);
    auto size = tag.GetBlobSize(blob_name);

    if (size == 0) {
      return MakeTextResult("Blob '" + blob_name + "' has zero size");
    }

    std::vector<char> buf(size);
    tag.GetBlob(blob_name, buf.data(), size);

    auto encoded = Base64Encode(buf);

    protocol::json result;
    result["content"] = protocol::json::array();
    result["content"].push_back({
        {"type", "text"},
        {"text", "Blob '" + blob_name + "' from tag '" + tag_name +
                 "' (" + std::to_string(size) + " bytes):"}
    });
    result["content"].push_back({
        {"type", "text"},
        {"text", encoded}
    });
    result["isError"] = false;
    return result;
  } catch (const std::exception& e) {
    return MakeErrorResult(std::string("get_blob error: ") + e.what());
  }
}

/// Get the size of a blob.
protocol::json Runtime::HandleGetBlobSize(const protocol::json& args) {
  if (!IsCteInitialized()) {
    return MakeErrorResult(
        "CTE runtime not initialized — call cte__initialize_cte_runtime first");
  }

  try {
    auto tag_name = args.at("tag_name").get<std::string>();
    auto blob_name = args.at("blob_name").get<std::string>();

    wrp_cte::core::Tag tag(tag_name);
    auto size = tag.GetBlobSize(blob_name);

    protocol::json result_json;
    result_json["size_bytes"] = size;
    return protocol::json{
        {"content", {{{"type", "text"},
                      {"text", "Blob '" + blob_name + "' size: " +
                               std::to_string(size) + " bytes"}}}},
        {"isError", false},
        {"size_bytes", size}};
  } catch (const std::exception& e) {
    return MakeErrorResult(std::string("get_blob_size error: ") + e.what());
  }
}

/// List all blobs in a tag.
protocol::json Runtime::HandleListBlobsInTag(const protocol::json& args) {
  if (!IsCteInitialized()) {
    return MakeErrorResult(
        "CTE runtime not initialized — call cte__initialize_cte_runtime first");
  }

  try {
    auto tag_name = args.at("tag_name").get<std::string>();

    wrp_cte::core::Tag tag(tag_name);
    auto blob_names = tag.GetContainedBlobs();

    std::string list_text = "Blobs in tag '" + tag_name + "':";
    for (const auto& name : blob_names) {
      list_text += "\n  - " + name;
    }
    if (blob_names.empty()) {
      list_text += " (empty)";
    }

    protocol::json result;
    result["content"] = protocol::json::array();
    result["content"].push_back({{"type", "text"}, {"text", list_text}});
    result["isError"] = false;
    result["blob_count"] = blob_names.size();
    return result;
  } catch (const std::exception& e) {
    return MakeErrorResult(std::string("list_blobs_in_tag error: ") + e.what());
  }
}

/// Delete a blob.
protocol::json Runtime::HandleDeleteBlob(const protocol::json& args) {
  if (!IsCteInitialized()) {
    return MakeErrorResult(
        "CTE runtime not initialized — call cte__initialize_cte_runtime first");
  }

  try {
    auto tag_name = args.at("tag_name").get<std::string>();
    auto blob_name = args.at("blob_name").get<std::string>();

    // Get tag ID then delete blob
    auto tag_future = WRP_CTE_CLIENT->AsyncGetOrCreateTag(tag_name);
    tag_future.Wait();
    auto tag_id = tag_future->tag_id_;

    auto del_future = WRP_CTE_CLIENT->AsyncDelBlob(tag_id, blob_name);
    del_future.Wait();

    return MakeTextResult(
        "Blob '" + blob_name + "' deleted from tag '" + tag_name + "'");
  } catch (const std::exception& e) {
    return MakeErrorResult(std::string("delete_blob error: ") + e.what());
  }
}

/// Query tags by regex pattern.
protocol::json Runtime::HandleTagQuery(const protocol::json& args) {
  if (!IsCteInitialized()) {
    return MakeErrorResult(
        "CTE runtime not initialized — call cte__initialize_cte_runtime first");
  }

  try {
    auto pattern = args.at("pattern").get<std::string>();
    auto max_results = static_cast<chi::u32>(args.value("max_results", 0));

    auto future = WRP_CTE_CLIENT->AsyncTagQuery(pattern, max_results);
    future.Wait();

    std::string result_text = "Tags matching '" + pattern + "':";
    auto& tags = future->tags_;
    for (size_t i = 0; i < tags.size(); ++i) {
      result_text += "\n  - " + std::string(tags[i].str());
    }
    if (tags.empty()) {
      result_text += " (none)";
    }

    protocol::json result;
    result["content"] = protocol::json::array();
    result["content"].push_back({{"type", "text"}, {"text", result_text}});
    result["isError"] = false;
    result["tag_count"] = tags.size();
    return result;
  } catch (const std::exception& e) {
    return MakeErrorResult(std::string("tag_query error: ") + e.what());
  }
}

/// Query blobs matching tag and blob patterns.
protocol::json Runtime::HandleBlobQuery(const protocol::json& args) {
  if (!IsCteInitialized()) {
    return MakeErrorResult(
        "CTE runtime not initialized — call cte__initialize_cte_runtime first");
  }

  try {
    auto tag_pattern = args.at("tag_pattern").get<std::string>();
    auto blob_pattern = args.at("blob_pattern").get<std::string>();
    auto max_results = static_cast<chi::u32>(args.value("max_results", 0));

    auto future = WRP_CTE_CLIENT->AsyncBlobQuery(
        tag_pattern, blob_pattern, max_results);
    future.Wait();

    std::string result_text =
        "Blobs matching tag='" + tag_pattern +
        "', blob='" + blob_pattern + "':";
    auto& blobs = future->blobs_;
    for (size_t i = 0; i < blobs.size(); ++i) {
      result_text += "\n  - " + std::string(blobs[i].str());
    }
    if (blobs.empty()) {
      result_text += " (none)";
    }

    protocol::json result;
    result["content"] = protocol::json::array();
    result["content"].push_back({{"type", "text"}, {"text", result_text}});
    result["isError"] = false;
    result["blob_count"] = blobs.size();
    return result;
  } catch (const std::exception& e) {
    return MakeErrorResult(std::string("blob_query error: ") + e.what());
  }
}

/// Poll telemetry log.
protocol::json Runtime::HandlePollTelemetryLog(const protocol::json& args) {
  if (!IsCteInitialized()) {
    return MakeErrorResult(
        "CTE runtime not initialized — call cte__initialize_cte_runtime first");
  }

  try {
    auto min_time = static_cast<uint64_t>(args.value("min_logical_time", 0));

    auto future = WRP_CTE_CLIENT->AsyncPollTelemetryLog(min_time);
    future.Wait();

    std::string result_text = "Telemetry log entries:";
    auto& entries = future->log_entries_;
    for (size_t i = 0; i < entries.size(); ++i) {
      result_text += "\n  [" + std::to_string(i) + "] " +
                     std::string(entries[i].str());
    }
    if (entries.empty()) {
      result_text += " (no entries since logical time " +
                     std::to_string(min_time) + ")";
    }

    protocol::json result;
    result["content"] = protocol::json::array();
    result["content"].push_back({{"type", "text"}, {"text", result_text}});
    result["isError"] = false;
    result["entry_count"] = entries.size();
    return result;
  } catch (const std::exception& e) {
    return MakeErrorResult(
        std::string("poll_telemetry_log error: ") + e.what());
  }
}

/// Reorganize a blob to a different tier.
protocol::json Runtime::HandleReorganizeBlob(const protocol::json& args) {
  if (!IsCteInitialized()) {
    return MakeErrorResult(
        "CTE runtime not initialized — call cte__initialize_cte_runtime first");
  }

  try {
    auto tag_name = args.at("tag_name").get<std::string>();
    auto blob_name = args.at("blob_name").get<std::string>();
    auto target_score = static_cast<float>(args.value("target_tier", 0.5));

    // Get tag ID
    auto tag_future = WRP_CTE_CLIENT->AsyncGetOrCreateTag(tag_name);
    tag_future.Wait();
    auto tag_id = tag_future->tag_id_;

    auto future = WRP_CTE_CLIENT->AsyncReorganizeBlob(
        tag_id, blob_name, target_score);
    future.Wait();

    std::string tier;
    if (target_score >= 0.8f) tier = "RAM";
    else if (target_score >= 0.4f) tier = "SSD";
    else tier = "archive";

    return MakeTextResult(
        "Blob '" + blob_name + "' reorganized to " + tier +
        " tier (score=" + std::to_string(target_score) + ")");
  } catch (const std::exception& e) {
    return MakeErrorResult(std::string("reorganize_blob error: ") + e.what());
  }
}

/// Initialize the CTE runtime.
protocol::json Runtime::HandleInitializeCteRuntime(
    const protocol::json& args) {
  try {
    if (IsCteInitialized()) {
      return MakeTextResult("CTE runtime already initialized");
    }

    std::string config_path = args.value("config_path", "");
    bool ok = WRP_CTE_CLIENT_INIT(config_path);

    if (!ok) {
      return MakeErrorResult("Failed to initialize CTE runtime");
    }

    return MakeTextResult("CTE runtime initialized successfully");
  } catch (const std::exception& e) {
    return MakeErrorResult(
        std::string("initialize_cte_runtime error: ") + e.what());
  }
}

/// Get CTE client status.
protocol::json Runtime::HandleGetClientStatus(
    const protocol::json& /*args*/) {
  bool initialized = IsCteInitialized();
  protocol::json result;
  result["content"] = protocol::json::array();
  result["content"].push_back({
      {"type", "text"},
      {"text", initialized
               ? "CTE client is initialized and ready"
               : "CTE client is NOT initialized — call initialize_cte_runtime"}
  });
  result["isError"] = false;
  result["initialized"] = initialized;
  return result;
}

/// Return static CTE tier metadata.
protocol::json Runtime::HandleGetCteTypes(const protocol::json& /*args*/) {
  protocol::json tiers = protocol::json::array();
  tiers.push_back({
      {"name", "ram"}, {"score_range", "0.8-1.0"},
      {"description", "DRAM — fastest, volatile"},
      {"latency_us", 0.5}, {"bandwidth_gbps", 50}
  });
  tiers.push_back({
      {"name", "nvme"}, {"score_range", "0.5-0.8"},
      {"description", "NVMe SSD — fast, persistent"},
      {"latency_us", 100}, {"bandwidth_gbps", 7}
  });
  tiers.push_back({
      {"name", "ssd"}, {"score_range", "0.2-0.5"},
      {"description", "SATA SSD — moderate speed, persistent"},
      {"latency_us", 500}, {"bandwidth_gbps", 0.5}
  });
  tiers.push_back({
      {"name", "archive"}, {"score_range", "0.0-0.2"},
      {"description", "HDD/tape archive — slow, bulk storage"},
      {"latency_us", 10000}, {"bandwidth_gbps", 0.1}
  });

  std::string text = "CTE Storage Tiers:\n"
      "  RAM (score 0.8-1.0): ~50 GB/s, ~0.5us latency\n"
      "  NVMe (score 0.5-0.8): ~7 GB/s, ~100us latency\n"
      "  SSD (score 0.2-0.5): ~0.5 GB/s, ~500us latency\n"
      "  Archive (score 0.0-0.2): ~0.1 GB/s, ~10ms latency";

  protocol::json result;
  result["content"] = protocol::json::array();
  result["content"].push_back({{"type", "text"}, {"text", text}});
  result["isError"] = false;
  result["tiers"] = tiers;
  return result;
}

}  // namespace mchips::mchip_cte
