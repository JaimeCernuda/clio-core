#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dt_provenance/protocol/anthropic_parser.h"

using namespace dt_provenance::protocol;
using Catch::Matchers::WithinAbs;

// Realistic Anthropic request fixture
static const char* kAnthropicRequest = R"({
  "model": "claude-sonnet-4-6",
  "max_tokens": 4096,
  "stream": true,
  "system": "You are a helpful coding assistant.",
  "messages": [
    {"role": "user", "content": "Write a hello world function in Python"},
    {"role": "assistant", "content": "Here is a simple hello world function:\n\n```python\ndef hello():\n    print(\"Hello, World!\")\n```"},
    {"role": "user", "content": "Now add type hints"}
  ],
  "tools": [
    {"name": "Read", "description": "Read a file", "input_schema": {"type": "object"}}
  ]
})";

// Realistic Anthropic non-streaming response fixture
static const char* kAnthropicResponse = R"({
  "id": "msg_01XAbC",
  "type": "message",
  "role": "assistant",
  "model": "claude-sonnet-4-6",
  "content": [
    {"type": "text", "text": "Here is the updated function with type hints:"},
    {"type": "tool_use", "id": "toolu_01ABC", "name": "Read", "input": {"file_path": "main.py"}}
  ],
  "stop_reason": "tool_use",
  "usage": {
    "input_tokens": 1234,
    "output_tokens": 567,
    "cache_creation_input_tokens": 100,
    "cache_read_input_tokens": 800
  }
})";

TEST_CASE("AnthropicParser::ParseRequest extracts model and stream flag",
          "[anthropic]") {
  auto body = json::parse(kAnthropicRequest);
  InteractionRecord record;
  AnthropicParser::ParseRequest(body, record);

  REQUIRE(record.model == "claude-sonnet-4-6");
  REQUIRE(record.response.is_streaming == true);
}

TEST_CASE("AnthropicParser::ParseRequest counts messages correctly",
          "[anthropic]") {
  auto body = json::parse(kAnthropicRequest);
  InteractionRecord record;
  AnthropicParser::ParseRequest(body, record);

  REQUIRE(record.context_metrics.message_count == 3);
  REQUIRE(record.context_metrics.user_turn_count == 2);
  REQUIRE(record.context_metrics.assistant_turn_count == 1);
  REQUIRE(record.context_metrics.tool_result_count == 0);
}

TEST_CASE("AnthropicParser::ParseRequest captures system prompt length",
          "[anthropic]") {
  auto body = json::parse(kAnthropicRequest);
  InteractionRecord record;
  AnthropicParser::ParseRequest(body, record);

  // "You are a helpful coding assistant." = 36 chars
  REQUIRE(record.context_metrics.system_prompt_length == 36);
}

TEST_CASE("AnthropicParser::ParseRequest handles array system prompt",
          "[anthropic]") {
  json body = {
      {"model", "claude-sonnet-4-6"},
      {"system",
       json::array(
           {json{{"type", "text"}, {"text", "System instruction one."}},
            json{{"type", "text"}, {"text", "System instruction two."}}})},
      {"messages", json::array()}};
  InteractionRecord record;
  AnthropicParser::ParseRequest(body, record);

  // "System instruction one." (23) + "System instruction two." (23) = 46
  REQUIRE(record.context_metrics.system_prompt_length == 46);
}

TEST_CASE("AnthropicParser::ParseResponse extracts text and tool_calls",
          "[anthropic]") {
  auto body = json::parse(kAnthropicResponse);
  InteractionRecord record;
  AnthropicParser::ParseResponse(body, record);

  REQUIRE(record.response.text == "Here is the updated function with type hints:");
  REQUIRE(record.response.tool_calls.size() == 1);
  REQUIRE(record.response.tool_calls[0].name == "Read");
  REQUIRE(record.response.tool_calls[0].id == "toolu_01ABC");
  REQUIRE(record.response.tool_calls[0].input["file_path"] == "main.py");
  REQUIRE(record.response.stop_reason == "tool_use");
}

TEST_CASE("AnthropicParser::ParseResponse extracts token usage",
          "[anthropic]") {
  auto body = json::parse(kAnthropicResponse);
  InteractionRecord record;
  AnthropicParser::ParseResponse(body, record);

  REQUIRE(record.metrics.input_tokens == 1234);
  REQUIRE(record.metrics.output_tokens == 567);
  REQUIRE(record.metrics.cache_creation_tokens == 100);
  REQUIRE(record.metrics.cache_read_tokens == 800);
}

TEST_CASE("AnthropicParser::ParseStreamChunk handles message_start",
          "[anthropic]") {
  json chunk = {{"type", "message_start"},
                {"message",
                 {{"model", "claude-sonnet-4-6"},
                  {"usage",
                   {{"input_tokens", 500},
                    {"cache_read_input_tokens", 200}}}}}};
  InteractionRecord record;
  AnthropicParser::ParseStreamChunk(chunk, record);

  REQUIRE(record.model == "claude-sonnet-4-6");
  REQUIRE(record.metrics.input_tokens == 500);
  REQUIRE(record.metrics.cache_read_tokens == 200);
}

TEST_CASE("AnthropicParser::ParseStreamChunk accumulates text deltas",
          "[anthropic]") {
  InteractionRecord record;

  json delta1 = {{"type", "content_block_delta"},
                 {"delta", {{"type", "text_delta"}, {"text", "Hello "}}}};
  json delta2 = {{"type", "content_block_delta"},
                 {"delta", {{"type", "text_delta"}, {"text", "world!"}}}};

  AnthropicParser::ParseStreamChunk(delta1, record);
  AnthropicParser::ParseStreamChunk(delta2, record);

  REQUIRE(record.response.text == "Hello world!");
}

TEST_CASE("AnthropicParser::ParseStreamChunk handles message_delta with stop",
          "[anthropic]") {
  json chunk = {{"type", "message_delta"},
                {"delta", {{"stop_reason", "end_turn"}}},
                {"usage", {{"output_tokens", 42}}}};
  InteractionRecord record;
  AnthropicParser::ParseStreamChunk(chunk, record);

  REQUIRE(record.response.stop_reason == "end_turn");
  REQUIRE(record.metrics.output_tokens == 42);
}

TEST_CASE("AnthropicParser::EstimateCost calculates correctly for claude-sonnet-4-6",
          "[anthropic]") {
  TokenUsage usage;
  usage.input_tokens = 1000;
  usage.output_tokens = 500;
  usage.cache_read_tokens = 200;
  usage.cache_creation_tokens = 0;

  auto cost = AnthropicParser::EstimateCost("claude-sonnet-4-6", usage);

  // Input: (1000 + 0) * 3.0/1M + 200 * 0.1 * 3.0/1M = 3060e-6
  // Output: 500 * 15.0/1M = 7500e-6
  REQUIRE_THAT(cost.input_cost, WithinAbs(0.00306, 0.00001));
  REQUIRE_THAT(cost.output_cost, WithinAbs(0.0075, 0.00001));
  REQUIRE_THAT(cost.total_cost, WithinAbs(0.01056, 0.00001));
}

TEST_CASE("AnthropicParser::EstimateCost returns note for unknown model",
          "[anthropic]") {
  TokenUsage usage;
  usage.input_tokens = 100;
  auto cost = AnthropicParser::EstimateCost("totally-unknown-model", usage);
  REQUIRE_FALSE(cost.note.empty());
  REQUIRE(cost.total_cost == 0.0);
}
