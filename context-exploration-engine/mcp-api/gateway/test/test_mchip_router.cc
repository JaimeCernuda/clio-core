/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Unit tests for MchipRouter (no Chimaera dependency).
 * Uses RegisterMchipDirect() to populate routes without Chimaera clients.
 */

#include "simple_test.h"
#include <mchips/mcp_gateway/mchip_router.h>

using namespace mchips::mcp_gateway;
using namespace mchips;

/// Helper: create a route with pre-populated tools (no Chimaera).
static MchipRoute MakeTestRoute(const std::string& name,
                                 std::vector<std::string> tool_names) {
  MchipRoute route;
  route.name = name;
  route.pool_id = chi::PoolId(0, 0);  // Dummy pool ID
  for (const auto& tn : tool_names) {
    protocol::ToolDefinition def;
    def.name = tn;
    def.description = "Test tool: " + tn;
    route.tools.push_back(std::move(def));
  }
  return route;
}

TEST_CASE("MchipRouter - Route splits qualified name", "[router][unit]") {
  MchipRouter router;
  router.RegisterMchipDirect(MakeTestRoute("cte", {"put_blob", "get_blob"}));

  std::string tool_name;
  auto* route = router.Route("cte__put_blob", tool_name);

  REQUIRE(route != nullptr);
  REQUIRE(route->name == "cte");
  REQUIRE(tool_name == "put_blob");
}

TEST_CASE("MchipRouter - Route unknown prefix returns nullptr", "[router][unit]") {
  MchipRouter router;
  router.RegisterMchipDirect(MakeTestRoute("cte", {"put_blob"}));

  std::string tool_name;
  auto* route = router.Route("unknown__tool", tool_name);
  REQUIRE(route == nullptr);
}

TEST_CASE("MchipRouter - Route without separator returns nullptr", "[router][unit]") {
  MchipRouter router;
  router.RegisterMchipDirect(MakeTestRoute("cte", {"put_blob"}));

  std::string tool_name;
  auto* route = router.Route("no_separator", tool_name);
  REQUIRE(route == nullptr);
}

TEST_CASE("MchipRouter - ListAllTools aggregates with prefix", "[router][unit]") {
  MchipRouter router;
  router.RegisterMchipDirect(MakeTestRoute("cte", {"put_blob", "get_blob"}));
  router.RegisterMchipDirect(MakeTestRoute("demo", {"echo", "add"}));

  auto all_tools = router.ListAllTools();
  REQUIRE(all_tools.size() == 4);

  // Verify all tools have qualified names
  std::set<std::string> names;
  for (const auto& tool : all_tools) {
    names.insert(tool.name);
  }
  REQUIRE(names.count("cte__put_blob") == 1);
  REQUIRE(names.count("cte__get_blob") == 1);
  REQUIRE(names.count("demo__echo") == 1);
  REQUIRE(names.count("demo__add") == 1);
}

TEST_CASE("MchipRouter - Empty router returns no tools", "[router][unit]") {
  MchipRouter router;
  REQUIRE_FALSE(router.HasRoutes());
  REQUIRE(router.NumMchips() == 0);
  REQUIRE(router.ListAllTools().empty());
}

TEST_CASE("MchipRouter - GetRoutes accessor", "[router][unit]") {
  MchipRouter router;
  router.RegisterMchipDirect(MakeTestRoute("cte", {"put_blob"}));
  router.RegisterMchipDirect(MakeTestRoute("cae", {"assimilate"}));

  auto& routes = router.GetRoutes();
  REQUIRE(routes.size() == 2);
  REQUIRE(routes.count("cte") == 1);
  REQUIRE(routes.count("cae") == 1);
}

TEST_CASE("MchipRouter - Multiple MChiPs with many tools", "[router][unit]") {
  MchipRouter router;

  // 12 CTE tools
  std::vector<std::string> cte_tools = {
      "put_blob", "get_blob", "get_blob_size", "list_blobs_in_tag",
      "delete_blob", "tag_query", "blob_query", "poll_telemetry_log",
      "reorganize_blob", "initialize_cte_runtime", "get_client_status",
      "get_cte_types"};
  router.RegisterMchipDirect(MakeTestRoute("cte", cte_tools));

  // 2 CAE tools
  router.RegisterMchipDirect(MakeTestRoute("cae", {"assimilate", "list_formats"}));

  // 3 Cluster tools
  router.RegisterMchipDirect(
      MakeTestRoute("cluster", {"cluster_status", "list_pools", "pool_stats"}));

  // 2 Demo tools
  router.RegisterMchipDirect(MakeTestRoute("demo", {"echo", "add"}));

  REQUIRE(router.NumMchips() == 4);
  REQUIRE(router.ListAllTools().size() == 19);

  // Verify routing for each MChiP
  std::string tool_name;

  auto* cte = router.Route("cte__put_blob", tool_name);
  REQUIRE(cte != nullptr);
  REQUIRE(tool_name == "put_blob");

  auto* cae = router.Route("cae__assimilate", tool_name);
  REQUIRE(cae != nullptr);
  REQUIRE(tool_name == "assimilate");

  auto* cluster = router.Route("cluster__pool_stats", tool_name);
  REQUIRE(cluster != nullptr);
  REQUIRE(tool_name == "pool_stats");

  auto* demo = router.Route("demo__add", tool_name);
  REQUIRE(demo != nullptr);
  REQUIRE(tool_name == "add");
}

SIMPLE_TEST_MAIN()
