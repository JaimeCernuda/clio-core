/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Integration test for the Gateway ChiMod.
 *
 * Verifies that HandleHttpRequestTask is dispatched through the Chimaera
 * task system, parsed as JSON-RPC, and routed to MChiP tools.
 *
 * Requires a running Chimaera server with gateway + demo MChiP pools.
 * Run: srun -N 1 ./mchips_demo_server &
 *       ./test_gateway_chimod
 */

#include "simple_test.h"
#include <chimaera/chimaera.h>
#include <mchips/mcp_gateway/gateway_client.h>
#include <mchips/protocol/mcp_types.h>

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

TEST_CASE("Gateway - HandleHttpRequest initialize", "[gateway][integration]") {
  EnsureInit();

  mchips::mcp_gateway::Client client(chi::PoolId(700, 0));

  // Build initialize JSON-RPC request
  json req = {
      {"jsonrpc", "2.0"},
      {"id", 1},
      {"method", "initialize"},
      {"params", {
          {"protocolVersion", "2025-11-25"},
          {"capabilities", json::object()},
          {"clientInfo", {{"name", "test"}, {"version", "0.1.0"}}}
      }}
  };

  auto future = client.AsyncHandleHttpRequest(
      chi::PoolQuery::Local(), req.dump(), "");
  future.Wait();

  REQUIRE(future->http_status_ == 200);

  auto resp = json::parse(future->response_body_.str());
  REQUIRE(resp.contains("result"));
  REQUIRE(resp.contains("sessionId"));
  INFO("Session ID: " << resp["sessionId"].get<std::string>());
}

TEST_CASE("Gateway - HandleHttpRequest tools/list", "[gateway][integration]") {
  EnsureInit();

  mchips::mcp_gateway::Client client(chi::PoolId(700, 0));

  // First initialize to get a session
  json init_req = {
      {"jsonrpc", "2.0"},
      {"id", 1},
      {"method", "initialize"},
      {"params", {
          {"protocolVersion", "2025-11-25"},
          {"capabilities", json::object()},
          {"clientInfo", {{"name", "test"}, {"version", "0.1.0"}}}
      }}
  };
  auto init_future = client.AsyncHandleHttpRequest(
      chi::PoolQuery::Local(), init_req.dump(), "");
  init_future.Wait();
  auto init_resp = json::parse(init_future->response_body_.str());
  auto session_id = init_resp.value("sessionId", "");

  // Now list tools
  json list_req = {
      {"jsonrpc", "2.0"},
      {"id", 2},
      {"method", "tools/list"}
  };
  auto list_future = client.AsyncHandleHttpRequest(
      chi::PoolQuery::Local(), list_req.dump(), session_id);
  list_future.Wait();

  REQUIRE(list_future->http_status_ == 200);

  auto resp = json::parse(list_future->response_body_.str());
  REQUIRE(resp.contains("result"));
  REQUIRE(resp["result"].contains("tools"));

  auto& tools = resp["result"]["tools"];
  INFO("Found " << tools.size() << " tools");
  REQUIRE(tools.size() >= 2);  // At least demo__echo and demo__add
}

TEST_CASE("Gateway - HandleHttpRequest tools/call demo__add", "[gateway][integration]") {
  EnsureInit();

  mchips::mcp_gateway::Client client(chi::PoolId(700, 0));

  // Initialize
  json init_req = {
      {"jsonrpc", "2.0"},
      {"id", 1},
      {"method", "initialize"},
      {"params", {
          {"protocolVersion", "2025-11-25"},
          {"capabilities", json::object()},
          {"clientInfo", {{"name", "test"}, {"version", "0.1.0"}}}
      }}
  };
  auto init_future = client.AsyncHandleHttpRequest(
      chi::PoolQuery::Local(), init_req.dump(), "");
  init_future.Wait();
  auto init_resp = json::parse(init_future->response_body_.str());
  auto session_id = init_resp.value("sessionId", "");

  // Call demo__add(17, 25)
  json call_req = {
      {"jsonrpc", "2.0"},
      {"id", 3},
      {"method", "tools/call"},
      {"params", {
          {"name", "demo__add"},
          {"arguments", {{"a", 17}, {"b", 25}}}
      }}
  };
  auto call_future = client.AsyncHandleHttpRequest(
      chi::PoolQuery::Local(), call_req.dump(), session_id);
  call_future.Wait();

  REQUIRE(call_future->http_status_ == 200);

  auto resp = json::parse(call_future->response_body_.str());
  REQUIRE(resp.contains("result"));

  auto& result = resp["result"];
  REQUIRE_FALSE(result.value("isError", true));
  auto text = result["content"][0]["text"].get<std::string>();
  INFO("demo__add result: " << text);
  REQUIRE(text.find("42") != std::string::npos);
}

TEST_CASE("Gateway - HandleHttpRequest ping", "[gateway][integration]") {
  EnsureInit();

  mchips::mcp_gateway::Client client(chi::PoolId(700, 0));

  json ping_req = {
      {"jsonrpc", "2.0"},
      {"id", 99},
      {"method", "ping"}
  };
  auto future = client.AsyncHandleHttpRequest(
      chi::PoolQuery::Local(), ping_req.dump(), "");
  future.Wait();

  REQUIRE(future->http_status_ == 200);
  auto resp = json::parse(future->response_body_.str());
  REQUIRE(resp.contains("result"));
}

SIMPLE_TEST_MAIN()
