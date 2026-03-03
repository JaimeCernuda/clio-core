#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dt_provenance/protocol/openai_parser.h"

using namespace dt_provenance::protocol;

// Realistic OpenAI request fixture
static const char* kOpenAIRequest = R"({
  "model": "gpt-4o",
  "messages": [
    {"role": "system", "content": "You are a helpful assistant."},
    {"role": "user", "content": "What is 2+2?"},
    {"role": "assistant", "content": "2+2 equals 4."},
    {"role": "user", "content": "What about 3+3?"}
  ],
  "stream": false,
  "tools": [
    {"type": "function", "function": {"name": "calculator", "parameters": {}}}
  ]
})";

// Realistic OpenAI non-streaming response fixture
static const char* kOpenAIResponse = R"({
  "id": "chatcmpl-abc123",
  "object": "chat.completion",
  "model": "gpt-4o-2024-11-20",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "3+3 equals 6.",
        "tool_calls": [
          {
            "id": "call_abc",
            "type": "function",
            "function": {
              "name": "calculator",
              "arguments": "{\"expression\": \"3+3\"}"
            }
          }
        ]
      },
      "finish_reason": "tool_calls"
    }
  ],
  "usage": {
    "prompt_tokens": 150,
    "completion_tokens": 25,
    "total_tokens": 175
  }
})";

TEST_CASE("OpenAIParser::ParseRequest extracts model and stream flag",
          "[openai]") {
  auto body = json::parse(kOpenAIRequest);
  InteractionRecord record;
  OpenAIParser::ParseRequest(body, record);

  REQUIRE(record.model == "gpt-4o");
  REQUIRE(record.response.is_streaming == false);
}

TEST_CASE("OpenAIParser::ParseRequest counts messages correctly", "[openai]") {
  auto body = json::parse(kOpenAIRequest);
  InteractionRecord record;
  OpenAIParser::ParseRequest(body, record);

  REQUIRE(record.context_metrics.message_count == 4);
  REQUIRE(record.context_metrics.user_turn_count == 2);
  REQUIRE(record.context_metrics.assistant_turn_count == 1);
  // System message doesn't count as user/assistant
  REQUIRE(record.context_metrics.system_prompt_length > 0);
}

TEST_CASE("OpenAIParser::ParseResponse extracts text and tool_calls",
          "[openai]") {
  auto body = json::parse(kOpenAIResponse);
  InteractionRecord record;
  OpenAIParser::ParseResponse(body, record);

  REQUIRE(record.response.text == "3+3 equals 6.");
  REQUIRE(record.response.stop_reason == "tool_calls");
  REQUIRE(record.response.tool_calls.size() == 1);
  REQUIRE(record.response.tool_calls[0].name == "calculator");
  REQUIRE(record.response.tool_calls[0].id == "call_abc");
  REQUIRE(record.response.tool_calls[0].input["expression"] == "3+3");
}

TEST_CASE("OpenAIParser::ParseResponse extracts token usage", "[openai]") {
  auto body = json::parse(kOpenAIResponse);
  InteractionRecord record;
  OpenAIParser::ParseResponse(body, record);

  REQUIRE(record.metrics.input_tokens == 150);
  REQUIRE(record.metrics.output_tokens == 25);
}

TEST_CASE("OpenAIParser::ParseStreamChunk accumulates text deltas",
          "[openai]") {
  InteractionRecord record;

  json chunk1 = {{"choices",
                   json::array({json{{"delta", {{"content", "Hello "}}},
                                     {"finish_reason", nullptr}}})}};
  json chunk2 = {{"choices",
                   json::array({json{{"delta", {{"content", "world!"}}},
                                     {"finish_reason", "stop"}}})}};

  OpenAIParser::ParseStreamChunk(chunk1, record);
  OpenAIParser::ParseStreamChunk(chunk2, record);

  REQUIRE(record.response.text == "Hello world!");
  REQUIRE(record.response.stop_reason == "stop");
}

TEST_CASE("OpenAIParser::ParseStreamChunk handles tool call deltas",
          "[openai]") {
  InteractionRecord record;

  json chunk1 = json::parse(R"({
    "choices": [{
      "delta": {
        "tool_calls": [{
          "index": 0, "id": "call_xyz",
          "function": {"name": "read", "arguments": "{\"path\":"}
        }]
      },
      "finish_reason": null
    }]
  })");
  json chunk2 = json::parse(R"({
    "choices": [{
      "delta": {
        "tool_calls": [{
          "index": 0,
          "function": {"arguments": " \"test.txt\"}"}
        }]
      },
      "finish_reason": null
    }]
  })");

  OpenAIParser::ParseStreamChunk(chunk1, record);
  OpenAIParser::ParseStreamChunk(chunk2, record);

  REQUIRE(record.response.tool_calls.size() == 1);
  REQUIRE(record.response.tool_calls[0].id == "call_xyz");
  REQUIRE(record.response.tool_calls[0].name == "read");
}

TEST_CASE("OpenAIParser::EstimateCost returns note for unknown model",
          "[openai]") {
  TokenUsage usage;
  usage.input_tokens = 100;
  auto cost = OpenAIParser::EstimateCost("unknown-model-xyz", usage);
  REQUIRE_FALSE(cost.note.empty());
}
