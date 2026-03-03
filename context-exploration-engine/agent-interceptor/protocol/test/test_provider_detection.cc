#include <catch2/catch_test_macros.hpp>
#include <unordered_map>

#include "dt_provenance/protocol/provider.h"

using namespace dt_provenance::protocol;
using Headers = std::unordered_map<std::string, std::string>;

TEST_CASE("DetectProvider identifies Anthropic from /v1/messages",
          "[provider]") {
  Headers headers;
  auto info = DetectProvider("/v1/messages", headers);
  REQUIRE(info.provider == Provider::kAnthropic);
  REQUIRE(info.upstream_base_url == "https://api.anthropic.com");
}

TEST_CASE("DetectProvider identifies Anthropic from anthropic-version header",
          "[provider]") {
  Headers headers = {{"anthropic-version", "2023-06-01"}};
  auto info = DetectProvider("/v1/complete", headers);
  REQUIRE(info.provider == Provider::kAnthropic);
}

TEST_CASE("DetectProvider identifies OpenAI from /v1/chat/completions",
          "[provider]") {
  Headers headers;
  auto info = DetectProvider("/v1/chat/completions", headers);
  REQUIRE(info.provider == Provider::kOpenAI);
  REQUIRE(info.upstream_base_url == "https://api.openai.com");
}

TEST_CASE("DetectProvider identifies OpenAI as fallback for /v1/*",
          "[provider]") {
  Headers headers;
  auto info = DetectProvider("/v1/embeddings", headers);
  REQUIRE(info.provider == Provider::kOpenAI);
}

TEST_CASE("DetectProvider identifies Ollama from /api/chat", "[provider]") {
  Headers headers;
  auto info = DetectProvider("/api/chat", headers);
  REQUIRE(info.provider == Provider::kOllama);
  REQUIRE(info.upstream_base_url == "http://localhost:11434");
}

TEST_CASE("DetectProvider identifies Ollama from /api/generate", "[provider]") {
  Headers headers;
  auto info = DetectProvider("/api/generate", headers);
  REQUIRE(info.provider == Provider::kOllama);
}

TEST_CASE("DetectProvider returns Unknown for internal routes", "[provider]") {
  Headers headers;
  auto info = DetectProvider("/_interceptor/status", headers);
  REQUIRE(info.provider == Provider::kUnknown);
}

TEST_CASE("DetectProvider returns Unknown for unrecognized paths",
          "[provider]") {
  Headers headers;
  auto info = DetectProvider("/health", headers);
  REQUIRE(info.provider == Provider::kUnknown);
}

TEST_CASE("ProviderToString round-trips correctly", "[provider]") {
  REQUIRE(ProviderToString(Provider::kAnthropic) == "anthropic");
  REQUIRE(ProviderToString(Provider::kOpenAI) == "openai");
  REQUIRE(ProviderToString(Provider::kOllama) == "ollama");
  REQUIRE(ProviderToString(Provider::kUnknown) == "unknown");
}

TEST_CASE("ProviderFromString is case-insensitive", "[provider]") {
  REQUIRE(ProviderFromString("Anthropic") == Provider::kAnthropic);
  REQUIRE(ProviderFromString("OPENAI") == Provider::kOpenAI);
  REQUIRE(ProviderFromString("ollama") == Provider::kOllama);
  REQUIRE(ProviderFromString("garbage") == Provider::kUnknown);
}

TEST_CASE("DetectProvider handles /v1/messages with query params",
          "[provider]") {
  Headers headers;
  auto info = DetectProvider("/v1/messages?beta=true", headers);
  REQUIRE(info.provider == Provider::kAnthropic);
}
