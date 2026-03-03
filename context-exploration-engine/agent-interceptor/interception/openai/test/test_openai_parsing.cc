#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dt_provenance/protocol/cost_estimator.h"
#include "dt_provenance/protocol/interaction.h"
#include "dt_provenance/protocol/openai_parser.h"
#include "dt_provenance/protocol/stream_reassembly.h"

using namespace dt_provenance::protocol;
using Catch::Matchers::WithinAbs;

// Realistic streaming SSE response from OpenAI
static const char* kOpenAISSEResponse = R"(data: {"id":"chatcmpl-abc","object":"chat.completion.chunk","created":1709000000,"model":"gpt-4o","choices":[{"index":0,"delta":{"role":"assistant","content":""},"finish_reason":null}]}

data: {"id":"chatcmpl-abc","object":"chat.completion.chunk","created":1709000000,"model":"gpt-4o","choices":[{"index":0,"delta":{"content":"Hello"},"finish_reason":null}]}

data: {"id":"chatcmpl-abc","object":"chat.completion.chunk","created":1709000000,"model":"gpt-4o","choices":[{"index":0,"delta":{"content":", how"},"finish_reason":null}]}

data: {"id":"chatcmpl-abc","object":"chat.completion.chunk","created":1709000000,"model":"gpt-4o","choices":[{"index":0,"delta":{"content":" can I help?"},"finish_reason":null}]}

data: {"id":"chatcmpl-abc","object":"chat.completion.chunk","created":1709000000,"model":"gpt-4o","choices":[{"index":0,"delta":{},"finish_reason":"stop"}]}

data: {"id":"chatcmpl-abc","object":"chat.completion.chunk","created":1709000000,"model":"gpt-4o","choices":[],"usage":{"prompt_tokens":25,"completion_tokens":8,"total_tokens":33}}

data: [DONE]

)";

TEST_CASE("Full OpenAI streaming pipeline: SSE -> parse -> record",
          "[openai][integration]") {
  // 1. Parse request
  json request_body = {
      {"model", "gpt-4o"},
      {"max_tokens", 4096},
      {"stream", true},
      {"stream_options", {{"include_usage", true}}},
      {"messages",
       json::array({json{{"role", "system"}, {"content", "Be helpful."}},
                    json{{"role", "user"}, {"content", "Hello"}}})}};

  InteractionRecord record;
  record.session_id = "test-session";
  record.provider = Provider::kOpenAI;
  record.request.method = "POST";
  record.request.path = "/v1/chat/completions";

  OpenAIParser::ParseRequest(request_body, record);

  REQUIRE(record.model == "gpt-4o");
  REQUIRE(record.response.is_streaming == true);
  REQUIRE(record.context_metrics.message_count == 2);
  REQUIRE(record.context_metrics.user_turn_count == 1);

  // 2. Reassemble SSE stream
  auto chunks = ReassembleSSE(kOpenAISSEResponse);
  REQUIRE(chunks.size() >= 5);

  // 3. Parse each chunk
  for (const auto& chunk : chunks) {
    OpenAIParser::ParseStreamChunk(chunk, record);
  }

  // 4. Verify extracted fields
  REQUIRE(record.response.text == "Hello, how can I help?");
  REQUIRE(record.response.stop_reason == "stop");
  REQUIRE(record.metrics.input_tokens == 25);
  REQUIRE(record.metrics.output_tokens == 8);

  // 5. Estimate cost
  TokenUsage usage;
  usage.input_tokens = record.metrics.input_tokens;
  usage.output_tokens = record.metrics.output_tokens;
  auto cost = CostEstimator::Estimate(Provider::kOpenAI, record.model, usage);

  REQUIRE(cost.total_cost > 0.0);
  REQUIRE(cost.model == "gpt-4o");

  // 6. Round-trip through JSON
  json serialized = record.ToJson();
  auto restored = InteractionRecord::FromJson(serialized);

  REQUIRE(restored.response.text == "Hello, how can I help?");
  REQUIRE(restored.session_id == "test-session");
  REQUIRE(restored.metrics.input_tokens == 25);
}

TEST_CASE("OpenAI non-streaming response parsing", "[openai]") {
  json response_body = {
      {"id", "chatcmpl-xyz"},
      {"object", "chat.completion"},
      {"created", 1709000000},
      {"model", "gpt-4o-2024-11-20"},
      {"choices",
       json::array({json{
           {"index", 0},
           {"message",
            {{"role", "assistant"},
             {"content", "The answer is 42."}}},
           {"finish_reason", "stop"}}})},
      {"usage",
       {{"prompt_tokens", 15}, {"completion_tokens", 6}, {"total_tokens", 21}}}};

  InteractionRecord record;
  OpenAIParser::ParseResponse(response_body, record);

  REQUIRE(record.model == "gpt-4o-2024-11-20");
  REQUIRE(record.response.text == "The answer is 42.");
  REQUIRE(record.response.stop_reason == "stop");
  REQUIRE(record.metrics.input_tokens == 15);
  REQUIRE(record.metrics.output_tokens == 6);
}

TEST_CASE("OpenAI streaming with tool_calls", "[openai][integration]") {
  InteractionRecord record;

  // First chunk with tool call start
  json chunk1 = {
      {"model", "gpt-4o"},
      {"choices",
       json::array({json{
           {"index", 0},
           {"delta",
            {{"tool_calls",
              json::array({json{{"index", 0},
                                {"id", "call_abc"},
                                {"type", "function"},
                                {"function",
                                 {{"name", "get_weather"},
                                  {"arguments", "{\"loc"}}}}}})}}},
           {"finish_reason", nullptr}}})}};
  OpenAIParser::ParseStreamChunk(chunk1, record);

  REQUIRE(record.response.tool_calls.size() == 1);
  REQUIRE(record.response.tool_calls[0].name == "get_weather");
  REQUIRE(record.response.tool_calls[0].id == "call_abc");

  // Second chunk with argument continuation
  json chunk2 = {
      {"model", "gpt-4o"},
      {"choices",
       json::array({json{
           {"index", 0},
           {"delta",
            {{"tool_calls",
              json::array({json{{"index", 0},
                                {"function",
                                 {{"arguments", "ation\": \"NYC\"}"}}}}})}}},
           {"finish_reason", nullptr}}})}};
  OpenAIParser::ParseStreamChunk(chunk2, record);

  // Finalize tool call arguments
  for (auto& tc : record.response.tool_calls) {
    if (tc.input.is_string()) {
      std::string args_str = tc.input.get<std::string>();
      if (!args_str.empty()) {
        try {
          tc.input = json::parse(args_str);
        } catch (const json::parse_error&) {
        }
      }
    }
  }

  REQUIRE(record.response.tool_calls[0].input.is_object());
  REQUIRE(record.response.tool_calls[0].input["location"] == "NYC");

  // Final chunk with finish_reason and usage
  json chunk3 = {
      {"model", "gpt-4o"},
      {"choices",
       json::array({json{{"index", 0},
                          {"delta", json::object()},
                          {"finish_reason", "tool_calls"}}})},
      {"usage", {{"prompt_tokens", 50}, {"completion_tokens", 20}}}};
  OpenAIParser::ParseStreamChunk(chunk3, record);

  REQUIRE(record.response.stop_reason == "tool_calls");
  REQUIRE(record.metrics.input_tokens == 50);
  REQUIRE(record.metrics.output_tokens == 20);
}

TEST_CASE("OpenAI request with system message context metrics", "[openai]") {
  json request_body = {
      {"model", "gpt-4o-mini"},
      {"messages",
       json::array(
           {json{{"role", "system"}, {"content", "You are a helpful assistant."}},
            json{{"role", "user"}, {"content", "Hello"}},
            json{{"role", "assistant"}, {"content", "Hi there!"}},
            json{{"role", "user"}, {"content", "How are you?"}},
            json{{"role", "tool"},
                 {"content", "weather: sunny"},
                 {"tool_call_id", "call_1"}}})}};

  InteractionRecord record;
  OpenAIParser::ParseRequest(request_body, record);

  REQUIRE(record.model == "gpt-4o-mini");
  REQUIRE(record.context_metrics.message_count == 5);
  REQUIRE(record.context_metrics.user_turn_count == 2);
  REQUIRE(record.context_metrics.assistant_turn_count == 1);
  REQUIRE(record.context_metrics.tool_result_count == 1);
  REQUIRE(record.context_metrics.context_depth_chars > 0);
}

TEST_CASE("OpenAI cost estimation for known models", "[openai]") {
  TokenUsage usage;
  usage.input_tokens = 1'000'000;
  usage.output_tokens = 1'000'000;

  auto cost_4o = OpenAIParser::EstimateCost("gpt-4o", usage);
  REQUIRE_THAT(cost_4o.input_cost, WithinAbs(2.50, 0.01));
  REQUIRE_THAT(cost_4o.output_cost, WithinAbs(10.0, 0.01));

  auto cost_mini = OpenAIParser::EstimateCost("gpt-4o-mini", usage);
  REQUIRE_THAT(cost_mini.input_cost, WithinAbs(0.15, 0.01));
  REQUIRE_THAT(cost_mini.output_cost, WithinAbs(0.60, 0.01));

  auto cost_unknown = OpenAIParser::EstimateCost("unknown-model", usage);
  REQUIRE(cost_unknown.total_cost == 0.0);
  REQUIRE(!cost_unknown.note.empty());
}
