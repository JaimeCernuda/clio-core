/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mchip_cae/mchip_cae_runtime.h"

#include <mchips/protocol/schema_generator.h>

// CAE assimilation API — included when wrp_cae_core_client is available
// #include <wrp_cae/core/core_client.h>

#include <string>

namespace mchips::mchip_cae {

namespace {

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

}  // namespace

//=============================================================================
// RegisterTools — CAE MChiP provides 2 tools
//=============================================================================

void Runtime::RegisterTools() {
  using namespace protocol;

  // 1. assimilate — format conversion
  registrar_.Register(
      ToolBuilder("assimilate")
          .Description(
              "Transform scientific data between formats "
              "(HDF5, NetCDF, Zarr, CSV, binary). "
              "Source and destination can be file:// or iowarp:// URIs.")
          .AddParam("src", SchemaType::String,
                    "Source URI (file:///path or iowarp://tag/blob)", true)
          .AddParam("dst", SchemaType::String,
                    "Destination URI (file:///path or iowarp://tag/blob)", true)
          .AddParam("format", SchemaType::String,
                    "Target format: hdf5, netcdf, zarr, csv, binary", false)
          .AddParam("range_off", SchemaType::Integer,
                    "Byte offset for range read (optional)", false)
          .AddParam("range_size", SchemaType::Integer,
                    "Byte size for range read (optional)", false)
          .Annotations(ToolAnnotations{
              .readOnlyHint = false, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.6})
          .Build(),
      [this](const json& args) { return HandleAssimilate(args); });

  // 2. list_formats — static format catalog
  registrar_.Register(
      ToolBuilder("list_formats")
          .Description("List supported scientific data format pairs for "
                       "assimilation (source → destination)")
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.2})
          .Build(),
      [this](const json& args) { return HandleListFormats(args); });
}

//=============================================================================
// Tool handlers
//=============================================================================

/// Assimilate (convert) data between scientific formats.
///
/// When the CAE runtime is available, this calls the CAE assimilation API.
/// Currently returns an informative error if CAE is not linked.
protocol::json Runtime::HandleAssimilate(const protocol::json& args) {
  try {
    auto src = args.at("src").get<std::string>();
    auto dst = args.at("dst").get<std::string>();
    auto format = args.value("format", "binary");

    // CAE integration: call wrp_cae_core::Client::AsyncAssimilate(...)
    // The CAE runtime requires wrp_cae_core to be available.
    // When linked, this would be:
    //   AssimilationCtx ctx;
    //   ctx.src = src; ctx.dst = dst; ctx.format = format;
    //   auto future = WRP_CAE_CLIENT->AsyncAssimilate(ctx);
    //   future.Wait();
    //   return MakeTextResult("Assimilation complete: " + src + " -> " + dst);

    return MakeErrorResult(
        "CAE assimilation requires the wrp_cae_core runtime. "
        "Start the CAE pool to enable format conversion. "
        "Requested: " + src + " -> " + dst + " (format: " + format + ")");
  } catch (const std::exception& e) {
    return MakeErrorResult(std::string("assimilate error: ") + e.what());
  }
}

/// Return the static list of supported format pairs.
protocol::json Runtime::HandleListFormats(const protocol::json& /*args*/) {
  protocol::json formats = protocol::json::array();

  // Standard scientific data format conversions supported by CAE
  auto add_format = [&](const char* src, const char* dst,
                         const char* desc) {
    formats.push_back({
        {"source_format", src},
        {"target_format", dst},
        {"description", desc},
        {"available", true}
    });
  };

  add_format("hdf5",   "netcdf",  "HDF5 → NetCDF-4");
  add_format("hdf5",   "zarr",    "HDF5 → Zarr (cloud-optimized)");
  add_format("hdf5",   "csv",     "HDF5 datasets → CSV");
  add_format("netcdf", "hdf5",    "NetCDF → HDF5");
  add_format("netcdf", "zarr",    "NetCDF → Zarr");
  add_format("zarr",   "hdf5",    "Zarr → HDF5");
  add_format("zarr",   "netcdf",  "Zarr → NetCDF");
  add_format("csv",    "hdf5",    "CSV → HDF5 dataset");
  add_format("binary", "hdf5",    "Raw binary → HDF5 dataset");
  add_format("binary", "zarr",    "Raw binary → Zarr array");

  std::string text = "Supported CAE format pairs:\n";
  for (const auto& f : formats) {
    text += "  " + f["source_format"].get<std::string>() +
            " → " + f["target_format"].get<std::string>() +
            " (" + f["description"].get<std::string>() + ")\n";
  }

  return protocol::json{
      {"content", {{{"type", "text"}, {"text", text}}}},
      {"isError", false},
      {"formats", formats}};
}

}  // namespace mchips::mchip_cae
