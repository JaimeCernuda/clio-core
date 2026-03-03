#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dt_provenance/protocol/anthropic_parser.h"
#include "dt_provenance/protocol/cost_estimator.h"
#include "dt_provenance/protocol/interaction.h"
#include "dt_provenance/protocol/stream_reassembly.h"

using namespace dt_provenance::protocol;
using Catch::Matchers::WithinAbs;

// Realistic streaming SSE response from Anthropic
static const char* kAnthropicSSEResponse = R"(event: message_start
data: {"type":"message_start","message":{"id":"msg_abc","type":"message","role":"assistant","model":"claude-sonnet-4-6","content":[],"stop_reason":null,"usage":{"input_tokens":500,"cache_read_input_tokens":100}}}

event: content_block_start
data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hello"}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":", I can help"}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":" with that!"}}

event: content_block_stop
data: {"type":"content_block_stop","index":0}

event: message_delta
data: {"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":10}}

event: message_stop
data: {"type":"message_stop"}

)";

TEST_CASE("Full Anthropic streaming pipeline: SSE → parse → record",
          "[anthropic][integration]") {
  // 1. Parse request
  json request_body = {
      {"model", "claude-sonnet-4-6"},
      {"max_tokens", 4096},
      {"stream", true},
      {"system", "Be helpful."},
      {"messages",
       json::array({json{{"role", "user"}, {"content", "Help me"}}})}};

  InteractionRecord record;
  record.session_id = "test-session";
  record.provider = Provider::kAnthropic;
  record.request.method = "POST";
  record.request.path = "/v1/messages";

  AnthropicParser::ParseRequest(request_body, record);

  REQUIRE(record.model == "claude-sonnet-4-6");
  REQUIRE(record.response.is_streaming == true);
  REQUIRE(record.context_metrics.message_count == 1);
  REQUIRE(record.context_metrics.user_turn_count == 1);

  // 2. Reassemble SSE stream
  auto chunks = ReassembleSSE(kAnthropicSSEResponse);
  REQUIRE(chunks.size() >= 5);

  // 3. Parse each chunk
  for (const auto& chunk : chunks) {
    AnthropicParser::ParseStreamChunk(chunk, record);
  }

  // 4. Verify extracted fields
  REQUIRE(record.response.text == "Hello, I can help with that!");
  REQUIRE(record.response.stop_reason == "end_turn");
  REQUIRE(record.metrics.input_tokens == 500);
  REQUIRE(record.metrics.cache_read_tokens == 100);
  REQUIRE(record.metrics.output_tokens == 10);

  // 5. Estimate cost
  TokenUsage usage;
  usage.input_tokens = record.metrics.input_tokens;
  usage.output_tokens = record.metrics.output_tokens;
  usage.cache_read_tokens = record.metrics.cache_read_tokens;
  auto cost = CostEstimator::Estimate(Provider::kAnthropic, record.model, usage);

  REQUIRE(cost.total_cost > 0.0);
  REQUIRE(cost.model == "claude-sonnet-4-6");

  // 6. Round-trip through JSON
  json serialized = record.ToJson();
  auto restored = InteractionRecord::FromJson(serialized);

  REQUIRE(restored.response.text == "Hello, I can help with that!");
  REQUIRE(restored.session_id == "test-session");
  REQUIRE(restored.metrics.input_tokens == 500);
}

TEST_CASE("Anthropic streaming with tool_use blocks",
          "[anthropic][integration]") {
  InteractionRecord record;

  // message_start
  json start = {
      {"type", "message_start"},
      {"message",
       {{"model", "claude-sonnet-4-6"},
        {"usage", {{"input_tokens", 1000}}}}}};
  AnthropicParser::ParseStreamChunk(start, record);

  // content_block_start (tool_use)
  json tool_start = {
      {"type", "content_block_start"},
      {"index", 0},
      {"content_block",
       {{"type", "tool_use"}, {"id", "toolu_abc"}, {"name", "Read"}}}};
  AnthropicParser::ParseStreamChunk(tool_start, record);

  REQUIRE(record.response.tool_calls.size() == 1);
  REQUIRE(record.response.tool_calls[0].name == "Read");

  // input_json_delta chunks
  json json_delta1 = {{"type", "content_block_delta"},
                      {"delta",
                       {{"type", "input_json_delta"},
                        {"partial_json", R"({"file_)"}}}};
  json json_delta2 = {{"type", "content_block_delta"},
                      {"delta",
                       {{"type", "input_json_delta"},
                        {"partial_json", R"(path": "main.cc"})"}}}};
  AnthropicParser::ParseStreamChunk(json_delta1, record);
  AnthropicParser::ParseStreamChunk(json_delta2, record);

  // content_block_stop (finalizes JSON)
  json block_stop = {{"type", "content_block_stop"}, {"index", 0}};
  AnthropicParser::ParseStreamChunk(block_stop, record);

  REQUIRE(record.response.tool_calls[0].input.is_object());
  REQUIRE(record.response.tool_calls[0].input["file_path"] == "main.cc");

  // message_delta
  json msg_delta = {{"type", "message_delta"},
                    {"delta", {{"stop_reason", "tool_use"}}},
                    {"usage", {{"output_tokens", 50}}}};
  AnthropicParser::ParseStreamChunk(msg_delta, record);

  REQUIRE(record.response.stop_reason == "tool_use");
  REQUIRE(record.metrics.output_tokens == 50);
}
