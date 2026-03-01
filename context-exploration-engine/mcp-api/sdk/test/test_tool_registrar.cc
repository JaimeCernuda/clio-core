/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Unit tests for ToolRegistrar (no Chimaera dependency).
 */

#include "simple_test.h"
#include <mchips/sdk/tool_registrar.h>
#include <mchips/protocol/schema_generator.h>

using namespace mchips::sdk;
using namespace mchips::protocol;

TEST_CASE("ToolRegistrar - Register and invoke", "[registrar][unit]") {
  ToolRegistrar reg;

  ToolDefinition def;
  def.name = "echo";
  def.description = "Return input unchanged";

  reg.Register(std::move(def), [](const json& args) -> json {
    return json{
        {"content", {{{"type", "text"}, {"text", args.value("message", "")}}}},
        {"isError", false}};
  });

  REQUIRE(reg.Size() == 1);
  REQUIRE(reg.HasTool("echo"));

  auto result = reg.Invoke("echo", {{"message", "hello"}});
  REQUIRE_FALSE(result.value("isError", true));
  REQUIRE(result["content"][0]["text"] == "hello");
}

TEST_CASE("ToolRegistrar - Unknown tool returns error", "[registrar][unit]") {
  ToolRegistrar reg;
  auto result = reg.Invoke("nonexistent", json::object());

  REQUIRE(result.value("isError", false) == true);
  auto text = result["content"][0]["text"].get<std::string>();
  REQUIRE(text.find("not found") != std::string::npos);
}

TEST_CASE("ToolRegistrar - ListTools returns definitions", "[registrar][unit]") {
  ToolRegistrar reg;

  ToolDefinition def1;
  def1.name = "tool_a";
  def1.description = "First tool";
  reg.Register(std::move(def1), [](const json&) -> json { return json{}; });

  ToolDefinition def2;
  def2.name = "tool_b";
  def2.description = "Second tool";
  reg.Register(std::move(def2), [](const json&) -> json { return json{}; });

  auto tools = reg.ListTools();
  REQUIRE(tools.size() == 2);
}

TEST_CASE("ToolRegistrar - Overwrite existing tool", "[registrar][unit]") {
  ToolRegistrar reg;

  ToolDefinition def;
  def.name = "echo";
  def.description = "Version 1";
  reg.Register(def, [](const json&) -> json {
    return json{{"content", {{{"type", "text"}, {"text", "v1"}}}},
                {"isError", false}};
  });

  // Overwrite with new handler
  def.description = "Version 2";
  reg.Register(def, [](const json&) -> json {
    return json{{"content", {{{"type", "text"}, {"text", "v2"}}}},
                {"isError", false}};
  });

  REQUIRE(reg.Size() == 1);
  auto result = reg.Invoke("echo", json::object());
  REQUIRE(result["content"][0]["text"] == "v2");
}

TEST_CASE("ToolRegistrar - Handler exception propagates", "[registrar][unit]") {
  ToolRegistrar reg;

  ToolDefinition def;
  def.name = "throws";
  def.description = "Throws on invoke";
  reg.Register(std::move(def), [](const json&) -> json {
    throw std::runtime_error("deliberate test error");
  });

  bool caught = false;
  try {
    reg.Invoke("throws", json::object());
  } catch (const std::runtime_error& e) {
    caught = true;
    REQUIRE(std::string(e.what()) == "deliberate test error");
  }
  REQUIRE(caught);
}

TEST_CASE("ToolRegistrar - 12 tool stress test", "[registrar][unit]") {
  ToolRegistrar reg;

  for (int i = 0; i < 12; ++i) {
    ToolDefinition def;
    def.name = "tool_" + std::to_string(i);
    def.description = "Tool number " + std::to_string(i);
    int captured_i = i;
    reg.Register(std::move(def), [captured_i](const json&) -> json {
      return json{
          {"content",
           {{{"type", "text"},
             {"text", "result_" + std::to_string(captured_i)}}}},
          {"isError", false}};
    });
  }

  REQUIRE(reg.Size() == 12);

  // Invoke each tool and verify result
  for (int i = 0; i < 12; ++i) {
    auto name = "tool_" + std::to_string(i);
    REQUIRE(reg.HasTool(name));
    auto result = reg.Invoke(name, json::object());
    REQUIRE_FALSE(result.value("isError", true));
    auto expected = "result_" + std::to_string(i);
    REQUIRE(result["content"][0]["text"] == expected);
  }
}

SIMPLE_TEST_MAIN()
