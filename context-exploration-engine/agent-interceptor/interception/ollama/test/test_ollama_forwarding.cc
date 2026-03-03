#include <catch2/catch_test_macros.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::ordered_json;

/**
 * Test forwarding by starting a mock Ollama server
 * and verifying the request is forwarded correctly.
 *
 * This tests the HTTP-level forwarding without Chimaera —
 * it verifies the mock server receives requests correctly.
 */

TEST_CASE("Mock Ollama server receives /api/chat request",
          "[ollama][forwarding]") {
  httplib::Server mock_server;
  std::string captured_body;
  std::string captured_path;
  bool request_received = false;

  mock_server.Post("/api/chat",
                   [&](const httplib::Request& req, httplib::Response& res) {
                     request_received = true;
                     captured_body = req.body;
                     captured_path = req.path;

                     // Return a realistic non-streaming Ollama response
                     json response = {
                         {"model", "llama3.2"},
                         {"message",
                          {{"role", "assistant"},
                           {"content", "Test response"}}},
                         {"done", true},
                         {"done_reason", "stop"},
                         {"prompt_eval_count", 10},
                         {"eval_count", 5}};
                     res.set_content(response.dump(), "application/json");
                   });

  std::thread server_thread(
      [&]() { mock_server.listen("127.0.0.1", 19098); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  httplib::Client client("127.0.0.1", 19098);
  client.set_connection_timeout(5);
  client.set_read_timeout(5);

  json request_body = {
      {"model", "llama3.2"},
      {"stream", false},
      {"messages",
       json::array({json{{"role", "user"}, {"content", "Hello"}}})}};

  auto res =
      client.Post("/api/chat", request_body.dump(), "application/json");

  REQUIRE(res);
  REQUIRE(res->status == 200);
  REQUIRE(request_received);
  REQUIRE(captured_path == "/api/chat");

  auto received = json::parse(captured_body);
  REQUIRE(received["model"] == "llama3.2");
  REQUIRE(received["messages"].size() == 1);

  auto resp_body = json::parse(res->body);
  REQUIRE(resp_body["done"] == true);
  REQUIRE(resp_body["message"]["content"] == "Test response");
  REQUIRE(resp_body["prompt_eval_count"] == 10);

  mock_server.stop();
  server_thread.join();
}

TEST_CASE("Mock Ollama server receives /api/generate request",
          "[ollama][forwarding]") {
  httplib::Server mock_server;
  bool request_received = false;

  mock_server.Post("/api/generate",
                   [&](const httplib::Request& req, httplib::Response& res) {
                     request_received = true;

                     json response = {{"model", "codellama"},
                                       {"response", "Hello World!"},
                                       {"done", true},
                                       {"done_reason", "stop"},
                                       {"prompt_eval_count", 5},
                                       {"eval_count", 3}};
                     res.set_content(response.dump(), "application/json");
                   });

  std::thread server_thread(
      [&]() { mock_server.listen("127.0.0.1", 19099); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  httplib::Client client("127.0.0.1", 19099);
  json request_body = {
      {"model", "codellama"},
      {"prompt", "Say hello"},
      {"stream", false}};

  auto res =
      client.Post("/api/generate", request_body.dump(), "application/json");

  REQUIRE(res);
  REQUIRE(res->status == 200);
  REQUIRE(request_received);

  auto resp_body = json::parse(res->body);
  REQUIRE(resp_body["response"] == "Hello World!");

  mock_server.stop();
  server_thread.join();
}

TEST_CASE("Mock Ollama NDJSON streaming response", "[ollama][forwarding]") {
  httplib::Server mock_server;

  mock_server.Post("/api/chat",
                   [&](const httplib::Request& req, httplib::Response& res) {
                     auto body = json::parse(req.body);
                     bool stream = body.value("stream", true);

                     if (stream) {
                       std::string ndjson_body =
                           "{\"model\":\"llama3.2\",\"message\":{\"role\":"
                           "\"assistant\",\"content\":\"Hi\"},\"done\":false}\n"
                           "{\"model\":\"llama3.2\",\"message\":{\"role\":"
                           "\"assistant\",\"content\":\"!\"},\"done\":false}\n"
                           "{\"model\":\"llama3.2\",\"message\":{\"role\":"
                           "\"assistant\",\"content\":\"\"},\"done\":true,"
                           "\"done_reason\":\"stop\",\"prompt_eval_count\":5,"
                           "\"eval_count\":2}\n";
                       res.set_content(ndjson_body, "application/x-ndjson");
                     } else {
                       json response = {
                           {"model", "llama3.2"},
                           {"message",
                            {{"role", "assistant"}, {"content", "Hi!"}}},
                           {"done", true},
                           {"done_reason", "stop"},
                           {"prompt_eval_count", 5},
                           {"eval_count", 2}};
                       res.set_content(response.dump(), "application/json");
                     }
                   });

  std::thread server_thread(
      [&]() { mock_server.listen("127.0.0.1", 19100); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  httplib::Client client("127.0.0.1", 19100);
  json request_body = {
      {"model", "llama3.2"},
      {"stream", true},
      {"messages",
       json::array({json{{"role", "user"}, {"content", "Hi"}}})}};

  auto res =
      client.Post("/api/chat", request_body.dump(), "application/json");
  REQUIRE(res);
  REQUIRE(res->status == 200);

  // Verify NDJSON content has multiple lines
  REQUIRE(res->body.find("\"done\":false") != std::string::npos);
  REQUIRE(res->body.find("\"done\":true") != std::string::npos);
  REQUIRE(res->body.find("llama3.2") != std::string::npos);

  mock_server.stop();
  server_thread.join();
}
