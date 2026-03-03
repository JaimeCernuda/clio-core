#include <catch2/catch_test_macros.hpp>

#include "dt_provenance/protocol/cost_estimator.h"
#include "dt_provenance/protocol/interaction.h"
#include "dt_provenance/protocol/ollama_parser.h"
#include "dt_provenance/protocol/stream_reassembly.h"

using namespace dt_provenance::protocol;

// Realistic NDJSON streaming response from Ollama /api/chat
static const char* kOllamaChatNDJSON =
    R"({"model":"llama3.2","message":{"role":"assistant","content":"Hello"},"done":false}
{"model":"llama3.2","message":{"role":"assistant","content":", how"},"done":false}
{"model":"llama3.2","message":{"role":"assistant","content":" are you?"},"done":false}
{"model":"llama3.2","message":{"role":"assistant","content":""},"done":true,"done_reason":"stop","prompt_eval_count":15,"eval_count":8}
)";

// Realistic NDJSON streaming response from Ollama /api/generate
static const char* kOllamaGenerateNDJSON =
    R"({"model":"llama3.2","response":"The ","done":false}
{"model":"llama3.2","response":"answer ","done":false}
{"model":"llama3.2","response":"is 42.","done":false}
{"model":"llama3.2","response":"","done":true,"done_reason":"stop","prompt_eval_count":10,"eval_count":6}
)";

TEST_CASE("Full Ollama /api/chat streaming pipeline: NDJSON -> parse -> record",
          "[ollama][integration]") {
  // 1. Parse request
  json request_body = {
      {"model", "llama3.2"},
      {"stream", true},
      {"messages",
       json::array({json{{"role", "system"}, {"content", "Be helpful."}},
                    json{{"role", "user"}, {"content", "Hello"}}})}};

  InteractionRecord record;
  record.session_id = "test-session";
  record.provider = Provider::kOllama;
  record.request.method = "POST";
  record.request.path = "/api/chat";

  OllamaParser::ParseRequest(request_body, "/api/chat", record);

  REQUIRE(record.model == "llama3.2");
  REQUIRE(record.response.is_streaming == true);
  REQUIRE(record.context_metrics.message_count == 2);
  REQUIRE(record.context_metrics.user_turn_count == 1);

  // 2. Reassemble NDJSON stream
  auto chunks = ReassembleNDJSON(kOllamaChatNDJSON);
  REQUIRE(chunks.size() == 4);

  // 3. Parse each chunk
  for (const auto& chunk : chunks) {
    OllamaParser::ParseStreamChunk(chunk, "/api/chat", record);
  }

  // 4. Verify extracted fields
  REQUIRE(record.response.text == "Hello, how are you?");
  REQUIRE(record.response.stop_reason == "stop");
  REQUIRE(record.metrics.input_tokens == 15);
  REQUIRE(record.metrics.output_tokens == 8);

  // 5. Cost should be zero (local model)
  TokenUsage usage;
  usage.input_tokens = record.metrics.input_tokens;
  usage.output_tokens = record.metrics.output_tokens;
  auto cost = CostEstimator::Estimate(Provider::kOllama, record.model, usage);

  REQUIRE(cost.total_cost == 0.0);

  // 6. Round-trip through JSON
  json serialized = record.ToJson();
  auto restored = InteractionRecord::FromJson(serialized);

  REQUIRE(restored.response.text == "Hello, how are you?");
  REQUIRE(restored.session_id == "test-session");
  REQUIRE(restored.metrics.input_tokens == 15);
}

TEST_CASE("Ollama /api/generate streaming pipeline", "[ollama][integration]") {
  json request_body = {
      {"model", "llama3.2"},
      {"prompt", "What is the meaning of life?"},
      {"stream", true}};

  InteractionRecord record;
  record.provider = Provider::kOllama;

  OllamaParser::ParseRequest(request_body, "/api/generate", record);

  REQUIRE(record.model == "llama3.2");
  REQUIRE(record.context_metrics.message_count == 1);
  REQUIRE(record.context_metrics.user_turn_count == 1);

  auto chunks = ReassembleNDJSON(kOllamaGenerateNDJSON);
  REQUIRE(chunks.size() == 4);

  for (const auto& chunk : chunks) {
    OllamaParser::ParseStreamChunk(chunk, "/api/generate", record);
  }

  REQUIRE(record.response.text == "The answer is 42.");
  REQUIRE(record.response.stop_reason == "stop");
  REQUIRE(record.metrics.input_tokens == 10);
  REQUIRE(record.metrics.output_tokens == 6);
}

TEST_CASE("Ollama non-streaming /api/chat response", "[ollama]") {
  json response_body = {
      {"model", "llama3.2"},
      {"message",
       {{"role", "assistant"}, {"content", "Hello! How can I help?"}}},
      {"done", true},
      {"done_reason", "stop"},
      {"prompt_eval_count", 20},
      {"eval_count", 12}};

  InteractionRecord record;
  OllamaParser::ParseResponse(response_body, "/api/chat", record);

  REQUIRE(record.response.text == "Hello! How can I help?");
  REQUIRE(record.response.stop_reason == "stop");
  REQUIRE(record.metrics.input_tokens == 20);
  REQUIRE(record.metrics.output_tokens == 12);
}

TEST_CASE("Ollama non-streaming /api/generate response", "[ollama]") {
  json response_body = {{"model", "codellama"},
                         {"response", "def hello():\n    print('Hello!')"},
                         {"done", true},
                         {"done_reason", "stop"},
                         {"prompt_eval_count", 8},
                         {"eval_count", 15}};

  InteractionRecord record;
  OllamaParser::ParseResponse(response_body, "/api/generate", record);

  REQUIRE(record.response.text == "def hello():\n    print('Hello!')");
  REQUIRE(record.response.stop_reason == "stop");
  REQUIRE(record.metrics.input_tokens == 8);
  REQUIRE(record.metrics.output_tokens == 15);
}

TEST_CASE("Ollama request with system prompt as top-level field", "[ollama]") {
  json request_body = {
      {"model", "llama3.2"},
      {"system", "You are a coding assistant."},
      {"messages",
       json::array({json{{"role", "user"}, {"content", "Write a function"}}})}};

  InteractionRecord record;
  OllamaParser::ParseRequest(request_body, "/api/chat", record);

  REQUIRE(record.model == "llama3.2");
  REQUIRE(record.context_metrics.message_count == 1);
  REQUIRE(record.context_metrics.user_turn_count == 1);
  // Top-level system prompt sets system_prompt_length
  REQUIRE(record.context_metrics.system_prompt_length > 0);
}

TEST_CASE("Ollama /api/chat with tool_calls in response", "[ollama]") {
  json response_body = {
      {"model", "llama3.2"},
      {"message",
       {{"role", "assistant"},
        {"content", ""},
        {"tool_calls",
         json::array(
             {json{{"function",
                    {{"name", "get_weather"},
                     {"arguments", {{"location", "NYC"}}}}}}})}}}  ,
      {"done", true},
      {"done_reason", "stop"},
      {"prompt_eval_count", 30},
      {"eval_count", 20}};

  InteractionRecord record;
  OllamaParser::ParseResponse(response_body, "/api/chat", record);

  REQUIRE(record.response.tool_calls.size() == 1);
  REQUIRE(record.response.tool_calls[0].name == "get_weather");
  REQUIRE(record.response.tool_calls[0].input["location"] == "NYC");
}

TEST_CASE("Ollama cost estimation is always zero", "[ollama]") {
  TokenUsage usage;
  usage.input_tokens = 1'000'000;
  usage.output_tokens = 1'000'000;

  auto cost = OllamaParser::EstimateCost("llama3.2", usage);
  REQUIRE(cost.total_cost == 0.0);
  REQUIRE(cost.input_cost == 0.0);
  REQUIRE(cost.output_cost == 0.0);
  REQUIRE(!cost.note.empty());
}
