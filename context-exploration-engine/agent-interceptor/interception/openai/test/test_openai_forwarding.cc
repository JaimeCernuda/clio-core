#include <catch2/catch_test_macros.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::ordered_json;

/**
 * Test forwarding by starting a mock OpenAI server
 * and verifying the request is forwarded correctly.
 *
 * This tests the HTTP-level forwarding without Chimaera —
 * it verifies the mock server receives requests correctly.
 */

TEST_CASE("Mock OpenAI server receives forwarded request",
          "[openai][forwarding]") {
  // Start a mock server that mimics OpenAI API
  httplib::Server mock_server;
  std::string captured_body;
  std::string captured_path;
  bool request_received = false;

  mock_server.Post("/v1/chat/completions",
                   [&](const httplib::Request& req, httplib::Response& res) {
                     request_received = true;
                     captured_body = req.body;
                     captured_path = req.path;

                     // Return a realistic OpenAI response
                     json response = {
                         {"id", "chatcmpl-test"},
                         {"object", "chat.completion"},
                         {"created", 1709000000},
                         {"model", "gpt-4o"},
                         {"choices",
                          json::array({json{
                              {"index", 0},
                              {"message",
                               {{"role", "assistant"},
                                {"content", "Test response"}}},
                              {"finish_reason", "stop"}}})},
                         {"usage",
                          {{"prompt_tokens", 10},
                           {"completion_tokens", 5},
                           {"total_tokens", 15}}}};
                     res.set_content(response.dump(), "application/json");
                   });

  // Start mock on background thread
  std::thread server_thread(
      [&]() { mock_server.listen("127.0.0.1", 19096); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Send a request to the mock
  httplib::Client client("127.0.0.1", 19096);
  client.set_connection_timeout(5);
  client.set_read_timeout(5);

  json request_body = {
      {"model", "gpt-4o"},
      {"max_tokens", 100},
      {"messages",
       json::array({json{{"role", "user"}, {"content", "Hello"}}})}};

  httplib::Headers headers = {{"authorization", "Bearer sk-test"},
                               {"content-type", "application/json"}};

  auto res = client.Post("/v1/chat/completions", headers, request_body.dump(),
                          "application/json");

  REQUIRE(res);
  REQUIRE(res->status == 200);
  REQUIRE(request_received);
  REQUIRE(captured_path == "/v1/chat/completions");

  // Verify the mock received the correct request body
  auto received = json::parse(captured_body);
  REQUIRE(received["model"] == "gpt-4o");
  REQUIRE(received["messages"].size() == 1);

  // Verify we got a valid OpenAI response back
  auto resp_body = json::parse(res->body);
  REQUIRE(resp_body["object"] == "chat.completion");
  REQUIRE(resp_body["choices"][0]["message"]["content"] == "Test response");
  REQUIRE(resp_body["usage"]["prompt_tokens"] == 10);

  mock_server.stop();
  server_thread.join();
}

TEST_CASE("Mock OpenAI SSE streaming response", "[openai][forwarding]") {
  httplib::Server mock_server;

  mock_server.Post("/v1/chat/completions",
                   [&](const httplib::Request& req, httplib::Response& res) {
                     auto body = json::parse(req.body);
                     bool stream = body.value("stream", false);

                     if (stream) {
                       std::string sse_body =
                           "data: {\"id\":\"chatcmpl-test\",\"model\":\"gpt-4o\","
                           "\"choices\":[{\"index\":0,\"delta\":{\"role\":"
                           "\"assistant\",\"content\":\"\"},\"finish_reason\":"
                           "null}]}\n\n"
                           "data: {\"id\":\"chatcmpl-test\",\"model\":\"gpt-4o\","
                           "\"choices\":[{\"index\":0,\"delta\":{\"content\":"
                           "\"Hi!\"},\"finish_reason\":null}]}\n\n"
                           "data: {\"id\":\"chatcmpl-test\",\"model\":\"gpt-4o\","
                           "\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":"
                           "\"stop\"}]}\n\n"
                           "data: [DONE]\n\n";
                       res.set_content(sse_body, "text/event-stream");
                     } else {
                       json response = {
                           {"object", "chat.completion"},
                           {"model", "gpt-4o"},
                           {"choices",
                            json::array({json{
                                {"index", 0},
                                {"message",
                                 {{"role", "assistant"}, {"content", "Hi!"}}},
                                {"finish_reason", "stop"}}})},
                           {"usage",
                            {{"prompt_tokens", 5},
                             {"completion_tokens", 2}}}};
                       res.set_content(response.dump(), "application/json");
                     }
                   });

  std::thread server_thread(
      [&]() { mock_server.listen("127.0.0.1", 19097); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  httplib::Client client("127.0.0.1", 19097);
  json request_body = {
      {"model", "gpt-4o"},
      {"stream", true},
      {"messages",
       json::array({json{{"role", "user"}, {"content", "Hi"}}})}};

  auto res = client.Post("/v1/chat/completions", request_body.dump(),
                          "application/json");
  REQUIRE(res);
  REQUIRE(res->status == 200);

  // Verify SSE content
  REQUIRE(res->body.find("data: ") != std::string::npos);
  REQUIRE(res->body.find("Hi!") != std::string::npos);
  REQUIRE(res->body.find("[DONE]") != std::string::npos);

  mock_server.stop();
  server_thread.join();
}
