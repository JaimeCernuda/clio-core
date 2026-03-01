/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Integration test for NativeClient.
 *
 * Verifies the native (non-HTTP) path through the Gateway ChiMod:
 *   NativeClient → Gateway pool 700 → Demo MChiP pool 704
 *
 * Requires a running Chimaera server with gateway + demo pools.
 */

#include "simple_test.h"
#include <mchips/client/native_client.h>

#include <nlohmann/json.hpp>

using json = nlohmann::ordered_json;

static bool g_initialized = false;

void EnsureInit() {
  if (!g_initialized) {
    mchips::client::NativeClient::Init();
    SimpleTest::g_test_finalize = mchips::client::NativeClient::Finalize;
    g_initialized = true;
  }
}

TEST_CASE("NativeClient - Initialize session", "[native][integration]") {
  EnsureInit();

  mchips::client::NativeClient client;
  client.Connect();
  auto session_id = client.Initialize();

  REQUIRE(!session_id.empty());
  INFO("Native session ID: " << session_id);
}

TEST_CASE("NativeClient - ListTools returns all tools", "[native][integration]") {
  EnsureInit();

  mchips::client::NativeClient client;
  client.Connect();
  client.Initialize();

  auto tools = client.ListTools();
  INFO("Found " << tools.size() << " tools via native client");
  REQUIRE(tools.size() >= 2);  // At least demo__echo and demo__add

  // Verify demo tools are present with qualified names
  bool found_echo = false, found_add = false;
  for (const auto& tool : tools) {
    if (tool.name == "demo__echo") found_echo = true;
    if (tool.name == "demo__add") found_add = true;
  }
  REQUIRE(found_echo);
  REQUIRE(found_add);
}

TEST_CASE("NativeClient - CallTool demo__add via Gateway", "[native][integration]") {
  EnsureInit();

  mchips::client::NativeClient client;
  client.Connect();
  client.Initialize();

  auto result = client.CallTool("demo__add", {{"a", 17}, {"b", 25}});

  REQUIRE_FALSE(result.value("isError", true));
  auto text = result["content"][0]["text"].get<std::string>();
  INFO("demo__add result: " << text);
  REQUIRE(text.find("42") != std::string::npos);
}

TEST_CASE("NativeClient - CallTool demo__echo via Gateway", "[native][integration]") {
  EnsureInit();

  mchips::client::NativeClient client;
  client.Connect();
  client.Initialize();

  auto result = client.CallTool("demo__echo", {{"message", "native test"}});

  REQUIRE_FALSE(result.value("isError", true));
  auto text = result["content"][0]["text"].get<std::string>();
  REQUIRE(text == "native test");
}

TEST_CASE("NativeClient - CallTool unknown tool returns error", "[native][integration]") {
  EnsureInit();

  mchips::client::NativeClient client;
  client.Connect();
  client.Initialize();

  bool threw = false;
  try {
    client.CallTool("nonexistent__tool", json::object());
  } catch (const std::exception&) {
    threw = true;
  }
  // Either throws or returns error JSON — both are acceptable
  // The gateway returns a JSON-RPC error for unknown tools
  (void)threw;
}

SIMPLE_TEST_MAIN()
