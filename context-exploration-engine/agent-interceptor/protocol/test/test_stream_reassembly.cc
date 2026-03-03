#include <catch2/catch_test_macros.hpp>

#include "dt_provenance/protocol/stream_reassembly.h"

using namespace dt_provenance::protocol;

TEST_CASE("ReassembleSSE parses complete SSE body", "[stream]") {
  std::string sse_body =
      "event: message_start\n"
      "data: {\"type\":\"message_start\",\"message\":{\"model\":\"claude-sonnet-4-6\"}}\n"
      "\n"
      "event: content_block_delta\n"
      "data: {\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\",\"text\":\"Hello\"}}\n"
      "\n"
      "event: content_block_delta\n"
      "data: {\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\",\"text\":\" world\"}}\n"
      "\n"
      "event: message_delta\n"
      "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":5}}\n"
      "\n";

  auto chunks = ReassembleSSE(sse_body);
  REQUIRE(chunks.size() == 4);
  REQUIRE(chunks[0]["type"] == "message_start");
  REQUIRE(chunks[1]["delta"]["text"] == "Hello");
  REQUIRE(chunks[2]["delta"]["text"] == " world");
  REQUIRE(chunks[3]["delta"]["stop_reason"] == "end_turn");
}

TEST_CASE("ReassembleSSE handles [DONE] marker", "[stream]") {
  std::string sse_body =
      "data: {\"choices\":[{\"delta\":{\"content\":\"hi\"}}]}\n\n"
      "data: [DONE]\n\n";

  auto chunks = ReassembleSSE(sse_body);
  REQUIRE(chunks.size() == 1);
  REQUIRE(chunks[0]["choices"][0]["delta"]["content"] == "hi");
}

TEST_CASE("ReassembleSSE skips non-data lines", "[stream]") {
  std::string sse_body =
      "event: ping\n"
      "id: 123\n"
      "retry: 5000\n"
      "data: {\"type\":\"test\"}\n"
      "\n";

  auto chunks = ReassembleSSE(sse_body);
  REQUIRE(chunks.size() == 1);
  REQUIRE(chunks[0]["type"] == "test");
}

TEST_CASE("ReassembleNDJSON parses complete NDJSON body", "[stream]") {
  std::string ndjson_body =
      "{\"model\":\"llama3\",\"message\":{\"content\":\"Hello\"},\"done\":false}\n"
      "{\"model\":\"llama3\",\"message\":{\"content\":\" world\"},\"done\":false}\n"
      "{\"model\":\"llama3\",\"done\":true,\"eval_count\":10,\"prompt_eval_count\":5}\n";

  auto chunks = ReassembleNDJSON(ndjson_body);
  REQUIRE(chunks.size() == 3);
  REQUIRE(chunks[0]["message"]["content"] == "Hello");
  REQUIRE(chunks[1]["message"]["content"] == " world");
  REQUIRE(chunks[2]["done"] == true);
  REQUIRE(chunks[2]["eval_count"] == 10);
}

TEST_CASE("StreamReassembler handles partial chunks via Feed", "[stream]") {
  StreamReassembler reassembler(Provider::kAnthropic);
  std::vector<json> collected;
  auto cb = [&](const json& chunk) { collected.push_back(chunk); };

  // Feed partial data
  reassembler.Feed("data: {\"type\":", cb);
  REQUIRE(collected.empty());  // Not yet complete

  reassembler.Feed("\"test\"}\n\n", cb);
  REQUIRE(collected.size() == 1);
  REQUIRE(collected[0]["type"] == "test");
}

TEST_CASE("StreamReassembler handles NDJSON partial chunks", "[stream]") {
  StreamReassembler reassembler(Provider::kOllama);
  std::vector<json> collected;
  auto cb = [&](const json& chunk) { collected.push_back(chunk); };

  // Feed partial NDJSON line
  reassembler.Feed("{\"model\":\"llama3\"", cb);
  REQUIRE(collected.empty());

  reassembler.Feed(",\"done\":true}\n", cb);
  REQUIRE(collected.size() == 1);
  REQUIRE(collected[0]["done"] == true);
}

TEST_CASE("StreamReassembler counts chunks correctly", "[stream]") {
  StreamReassembler reassembler(Provider::kAnthropic);
  auto cb = [](const json&) {};

  std::string data =
      "data: {\"a\":1}\n\ndata: {\"b\":2}\n\ndata: {\"c\":3}\n\n";
  reassembler.Feed(data, cb);

  REQUIRE(reassembler.ChunkCount() == 3);
}

TEST_CASE("ReassembleSSE handles empty body", "[stream]") {
  auto chunks = ReassembleSSE("");
  REQUIRE(chunks.empty());
}

TEST_CASE("ReassembleNDJSON handles empty body", "[stream]") {
  auto chunks = ReassembleNDJSON("");
  REQUIRE(chunks.empty());
}

TEST_CASE("ReassembleSSE handles malformed JSON gracefully", "[stream]") {
  std::string sse_body =
      "data: {invalid json}\n\n"
      "data: {\"valid\":true}\n\n";

  auto chunks = ReassembleSSE(sse_body);
  REQUIRE(chunks.size() == 1);
  REQUIRE(chunks[0]["valid"] == true);
}
