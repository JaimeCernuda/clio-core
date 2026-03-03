#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "dt_provenance/proxy/session_guard.h"

using namespace dt_provenance::proxy;
using namespace dt_provenance::protocol;
using json = nlohmann::ordered_json;

TEST_CASE("BuildSessionRejection returns Anthropic message format",
          "[session_guard]") {
  auto [ct, body] = BuildSessionRejection(Provider::kAnthropic);
  REQUIRE(ct == "application/json");

  auto j = json::parse(body);
  REQUIRE(j["type"] == "message");
  REQUIRE(j["role"] == "assistant");
  REQUIRE(j.contains("content"));
  REQUIRE(j["content"].is_array());
  REQUIRE(j["content"][0]["type"] == "text");
  // Should contain session requirement message
  REQUIRE(j["content"][0]["text"].get<std::string>().find("session") !=
          std::string::npos);
}

TEST_CASE("BuildSessionRejection returns OpenAI chat completion format",
          "[session_guard]") {
  auto [ct, body] = BuildSessionRejection(Provider::kOpenAI);
  auto j = json::parse(body);
  REQUIRE(j["object"] == "chat.completion");
  REQUIRE(j["choices"].is_array());
  REQUIRE(j["choices"][0]["message"]["role"] == "assistant");
}

TEST_CASE("BuildSessionRejection returns Ollama format", "[session_guard]") {
  auto [ct, body] = BuildSessionRejection(Provider::kOllama);
  auto j = json::parse(body);
  REQUIRE(j.contains("message"));
  REQUIRE(j["message"]["role"] == "assistant");
  REQUIRE(j["done"] == true);
}

TEST_CASE("BuildSessionRejection returns generic error for Unknown",
          "[session_guard]") {
  auto [ct, body] = BuildSessionRejection(Provider::kUnknown);
  auto j = json::parse(body);
  REQUIRE(j.contains("error"));
  REQUIRE(j["error"]["code"] == -32000);
}
