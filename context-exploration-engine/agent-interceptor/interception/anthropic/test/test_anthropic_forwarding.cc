#include <catch2/catch_test_macros.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::ordered_json;

/**
 * Test forwarding by starting a mock Anthropic server
 * and verifying the request is forwarded correctly.
 *
 * This tests the HTTP-level forwarding without Chimaera —
 * it verifies the mock server receives requests correctly.
 */

TEST_CASE("Mock Anthropic server receives forwarded request",
          "[anthropic][forwarding]") {
  // Start a mock server that mimics Anthropic API
  httplib::Server mock_server;
  std::string captured_body;
  std::string captured_path;
  bool request_received = false;

  mock_server.Post("/v1/messages", [&](const httplib::Request& req,
                                        httplib::Response& res) {
    request_received = true;
    captured_body = req.body;
    captured_path = req.path;

    // Return a realistic Anthropic response
    json response = {
        {"id", "msg_test"},
        {"type", "message"},
        {"role", "assistant"},
        {"model", "claude-sonnet-4-6"},
        {"content", json::array({json{{"type", "text"}, {"text", "Test response"}}})},
        {"stop_reason", "end_turn"},
        {"usage", {{"input_tokens", 10}, {"output_tokens", 5}}}};
    res.set_content(response.dump(), "application/json");
  });

  // Start mock on background thread
  std::thread server_thread([&]() { mock_server.listen("127.0.0.1", 19094); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Send a request to the mock (simulating what the interception ChiMod does)
  httplib::Client client("127.0.0.1", 19094);
  client.set_connection_timeout(5);
  client.set_read_timeout(5);

  json request_body = {
      {"model", "claude-sonnet-4-6"},
      {"max_tokens", 100},
      {"messages",
       json::array({json{{"role", "user"}, {"content", "Hello"}}})}};

  httplib::Headers headers = {{"anthropic-version", "2023-06-01"},
                               {"content-type", "application/json"}};

  auto res = client.Post("/v1/messages", headers, request_body.dump(),
                          "application/json");

  REQUIRE(res);
  REQUIRE(res->status == 200);
  REQUIRE(request_received);
  REQUIRE(captured_path == "/v1/messages");

  // Verify the mock received the correct request body
  auto received = json::parse(captured_body);
  REQUIRE(received["model"] == "claude-sonnet-4-6");
  REQUIRE(received["messages"].size() == 1);

  // Verify we got a valid Anthropic response back
  auto resp_body = json::parse(res->body);
  REQUIRE(resp_body["type"] == "message");
  REQUIRE(resp_body["content"][0]["text"] == "Test response");
  REQUIRE(resp_body["usage"]["input_tokens"] == 10);

  mock_server.stop();
  server_thread.join();
}

TEST_CASE("Mock Anthropic SSE streaming response", "[anthropic][forwarding]") {
  httplib::Server mock_server;

  mock_server.Post("/v1/messages", [&](const httplib::Request& req,
                                        httplib::Response& res) {
    // Check if streaming was requested
    auto body = json::parse(req.body);
    bool stream = body.value("stream", false);

    if (stream) {
      std::string sse_body =
          "event: message_start\n"
          "data: {\"type\":\"message_start\",\"message\":{\"model\":\"claude-sonnet-4-6\",\"usage\":{\"input_tokens\":5}}}\n\n"
          "event: content_block_delta\n"
          "data: {\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\",\"text\":\"Hi!\"}}\n\n"
          "event: message_delta\n"
          "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":2}}\n\n";
      res.set_content(sse_body, "text/event-stream");
    } else {
      json response = {
          {"type", "message"},
          {"content", json::array({json{{"type", "text"}, {"text", "Hi!"}}})},
          {"stop_reason", "end_turn"},
          {"usage", {{"input_tokens", 5}, {"output_tokens", 2}}}};
      res.set_content(response.dump(), "application/json");
    }
  });

  std::thread server_thread([&]() { mock_server.listen("127.0.0.1", 19095); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  httplib::Client client("127.0.0.1", 19095);
  json request_body = {{"model", "claude-sonnet-4-6"},
                        {"stream", true},
                        {"messages", json::array({json{{"role", "user"}, {"content", "Hi"}}})}};

  auto res = client.Post("/v1/messages", request_body.dump(), "application/json");
  REQUIRE(res);
  REQUIRE(res->status == 200);

  // Verify SSE content
  REQUIRE(res->body.find("event: message_start") != std::string::npos);
  REQUIRE(res->body.find("text_delta") != std::string::npos);
  REQUIRE(res->body.find("Hi!") != std::string::npos);

  mock_server.stop();
  server_thread.join();
}
