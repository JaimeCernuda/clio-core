/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mchip_demo/mchip_demo_runtime.h"

#include <mchips/protocol/schema_generator.h>

#include <string>

namespace mchips::mchip_demo {

//=============================================================================
// RegisterTools — Demo MChiP provides 2 tools for testing/benchmarking
//=============================================================================

void Runtime::RegisterTools() {
  using namespace protocol;

  // 1. echo — returns input message unchanged
  registrar_.Register(
      ToolBuilder("echo")
          .Description("Return the input message unchanged (for testing)")
          .AddParam("message", SchemaType::String, "Message to echo back", true)
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.5})
          .Build(),
      [this](const json& args) { return HandleEcho(args); });

  // 2. add — add two numbers
  registrar_.Register(
      ToolBuilder("add")
          .Description("Add two numbers and return the sum")
          .AddParam("a", SchemaType::Number, "First number", true)
          .AddParam("b", SchemaType::Number, "Second number", true)
          .Annotations(ToolAnnotations{
              .readOnlyHint = true, .destructiveHint = false,
              .idempotentHint = true, .priority = 0.5})
          .Build(),
      [this](const json& args) { return HandleAdd(args); });
}

//=============================================================================
// Tool handlers
//=============================================================================

protocol::json Runtime::HandleEcho(const protocol::json& args) {
  std::string message = args.value("message", "");
  return protocol::json{
      {"content", {{{"type", "text"}, {"text", message}}}},
      {"isError", false}};
}

protocol::json Runtime::HandleAdd(const protocol::json& args) {
  double a = args.value("a", 0.0);
  double b = args.value("b", 0.0);
  double sum = a + b;
  return protocol::json{
      {"content", {{{"type", "text"}, {"text", std::to_string(sum)}}}},
      {"isError", false}};
}

}  // namespace mchips::mchip_demo
