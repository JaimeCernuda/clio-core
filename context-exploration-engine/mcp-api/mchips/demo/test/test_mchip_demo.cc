/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Integration test for the Demo MChiP ChiMod.
 *
 * Tests CallMcpToolTask and ListMcpToolsTask directly against the
 * Demo MChiP pool (704) via Chimaera IPC.
 *
 * Requires a running Chimaera server with demo MChiP pool.
 */

#include "simple_test.h"
#include <chimaera/chimaera.h>
#include <mchips/sdk/mchip_client.h>

#include <nlohmann/json.hpp>

using json = nlohmann::ordered_json;

static bool g_initialized = false;

void EnsureInit() {
  if (!g_initialized) {
    chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, false);
    SimpleTest::g_test_finalize = chi::CHIMAERA_FINALIZE;
    g_initialized = true;
  }
}

TEST_CASE("Demo MChiP - ListMcpTools", "[demo][integration]") {
  EnsureInit();

  mchips::sdk::MchipClient client(chi::PoolId(704, 0));
  auto future = client.AsyncListMcpTools(chi::PoolQuery::Local());
  future.Wait();

  auto tools_str = future->result_json_.str();
  auto tools = json::parse(tools_str);

  INFO("Demo tools: " << tools.dump(2));
  REQUIRE(tools.is_array());
  REQUIRE(tools.size() == 2);

  // Verify echo and add tools are present
  bool found_echo = false, found_add = false;
  for (const auto& tool : tools) {
    auto name = tool["name"].get<std::string>();
    if (name == "echo") found_echo = true;
    if (name == "add") found_add = true;
  }
  REQUIRE(found_echo);
  REQUIRE(found_add);
}

TEST_CASE("Demo MChiP - CallMcpTool echo", "[demo][integration]") {
  EnsureInit();

  mchips::sdk::MchipClient client(chi::PoolId(704, 0));
  json args = {{"message", "hello from test"}};

  auto future = client.AsyncCallMcpTool(
      chi::PoolQuery::Local(), "echo", args.dump());
  future.Wait();

  auto result_str = future->result_json_.str();
  auto result = json::parse(result_str);

  REQUIRE_FALSE(result.value("isError", true));
  auto text = result["content"][0]["text"].get<std::string>();
  REQUIRE(text == "hello from test");
}

TEST_CASE("Demo MChiP - CallMcpTool add", "[demo][integration]") {
  EnsureInit();

  mchips::sdk::MchipClient client(chi::PoolId(704, 0));
  json args = {{"a", 17}, {"b", 25}};

  auto future = client.AsyncCallMcpTool(
      chi::PoolQuery::Local(), "add", args.dump());
  future.Wait();

  auto result_str = future->result_json_.str();
  auto result = json::parse(result_str);

  REQUIRE_FALSE(result.value("isError", true));
  auto text = result["content"][0]["text"].get<std::string>();
  REQUIRE(text.find("42") != std::string::npos);
}

TEST_CASE("Demo MChiP - CallMcpTool unknown tool", "[demo][integration]") {
  EnsureInit();

  mchips::sdk::MchipClient client(chi::PoolId(704, 0));
  json args = json::object();

  auto future = client.AsyncCallMcpTool(
      chi::PoolQuery::Local(), "nonexistent", args.dump());
  future.Wait();

  auto result_str = future->result_json_.str();
  auto result = json::parse(result_str);

  REQUIRE(result.value("isError", false) == true);
}

SIMPLE_TEST_MAIN()
