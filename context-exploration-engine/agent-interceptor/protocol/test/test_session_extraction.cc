#include <catch2/catch_test_macros.hpp>

#include "dt_provenance/protocol/session.h"

using namespace dt_provenance::protocol;

TEST_CASE("ExtractSession parses valid session with trailing path",
          "[session]") {
  auto result = ExtractSession("/_session/agent-0/v1/messages");
  REQUIRE(result.has_value());
  REQUIRE(result->session_id == "agent-0");
  REQUIRE(result->stripped_path == "/v1/messages");
}

TEST_CASE("ExtractSession parses session with no trailing path", "[session]") {
  auto result = ExtractSession("/_session/my-session");
  REQUIRE(result.has_value());
  REQUIRE(result->session_id == "my-session");
  REQUIRE(result->stripped_path == "/");
}

TEST_CASE("ExtractSession parses session with deep trailing path",
          "[session]") {
  auto result = ExtractSession("/_session/test123/api/chat/completions");
  REQUIRE(result.has_value());
  REQUIRE(result->session_id == "test123");
  REQUIRE(result->stripped_path == "/api/chat/completions");
}

TEST_CASE("ExtractSession returns nullopt for non-session paths",
          "[session]") {
  REQUIRE_FALSE(ExtractSession("/v1/messages").has_value());
  REQUIRE_FALSE(ExtractSession("/api/chat").has_value());
  REQUIRE_FALSE(ExtractSession("/health").has_value());
  REQUIRE_FALSE(ExtractSession("/").has_value());
}

TEST_CASE("ExtractSession returns nullopt for empty session ID", "[session]") {
  REQUIRE_FALSE(ExtractSession("/_session/").has_value());
}

TEST_CASE("ExtractSession returns nullopt for incomplete prefix", "[session]") {
  REQUIRE_FALSE(ExtractSession("/_session").has_value());
  REQUIRE_FALSE(ExtractSession("/_sess/foo/bar").has_value());
}

TEST_CASE("ExtractSession handles session IDs with special characters",
          "[session]") {
  auto result = ExtractSession("/_session/agent-a_b.c/v1/messages");
  REQUIRE(result.has_value());
  REQUIRE(result->session_id == "agent-a_b.c");
  REQUIRE(result->stripped_path == "/v1/messages");
}

TEST_CASE("ExtractSession handles numeric session IDs", "[session]") {
  auto result = ExtractSession("/_session/12345/v1/messages");
  REQUIRE(result.has_value());
  REQUIRE(result->session_id == "12345");
}
