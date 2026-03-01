/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Unit tests for SessionManager (no Chimaera dependency).
 */

#include "simple_test.h"
#include <mchips/mcp_gateway/session_manager.h>

#include <set>
#include <thread>

using namespace mchips::mcp_gateway;

TEST_CASE("SessionManager - Create and validate", "[session][unit]") {
  SessionManager sm;
  auto id = sm.CreateSession("2025-11-25");

  REQUIRE(!id.empty());
  REQUIRE(sm.ValidateSession(id));
  REQUIRE(sm.Count() == 1);
}

TEST_CASE("SessionManager - UUID uniqueness", "[session][unit]") {
  SessionManager sm;
  std::set<std::string> ids;

  for (int i = 0; i < 100; ++i) {
    auto id = sm.CreateSession("2025-11-25");
    REQUIRE(ids.find(id) == ids.end());
    ids.insert(id);
  }

  REQUIRE(sm.Count() == 100);
}

TEST_CASE("SessionManager - Destroy session", "[session][unit]") {
  SessionManager sm;
  auto id = sm.CreateSession("2025-11-25");

  REQUIRE(sm.ValidateSession(id));
  sm.DestroySession(id);
  REQUIRE_FALSE(sm.ValidateSession(id));
  REQUIRE(sm.Count() == 0);
}

TEST_CASE("SessionManager - Invalid session returns false", "[session][unit]") {
  SessionManager sm;
  REQUIRE_FALSE(sm.ValidateSession("nonexistent-session-id"));
}

TEST_CASE("SessionManager - Expiry", "[session][unit]") {
  // Create with 1-second timeout
  SessionManager sm(std::chrono::seconds(1));
  auto id = sm.CreateSession("2025-11-25");

  REQUIRE(sm.ValidateSession(id));

  // Wait for expiry
  std::this_thread::sleep_for(std::chrono::seconds(2));

  REQUIRE_FALSE(sm.ValidateSession(id));
}

TEST_CASE("SessionManager - CleanupExpired", "[session][unit]") {
  SessionManager sm(std::chrono::seconds(1));
  sm.CreateSession("2025-11-25");
  sm.CreateSession("2025-11-25");

  REQUIRE(sm.Count() == 2);

  std::this_thread::sleep_for(std::chrono::seconds(2));
  sm.CleanupExpired();

  REQUIRE(sm.Count() == 0);
}

TEST_CASE("SessionManager - Multiple sessions", "[session][unit]") {
  SessionManager sm;
  auto id1 = sm.CreateSession("2025-11-25");
  auto id2 = sm.CreateSession("2025-11-25");
  auto id3 = sm.CreateSession("2025-11-25");

  REQUIRE(sm.Count() == 3);
  REQUIRE(sm.ValidateSession(id1));
  REQUIRE(sm.ValidateSession(id2));
  REQUIRE(sm.ValidateSession(id3));

  sm.DestroySession(id2);
  REQUIRE(sm.Count() == 2);
  REQUIRE(sm.ValidateSession(id1));
  REQUIRE_FALSE(sm.ValidateSession(id2));
  REQUIRE(sm.ValidateSession(id3));
}

SIMPLE_TEST_MAIN()
