#include <catch2/catch_test_macros.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>

#include "dt_provenance/proxy/http_proxy_server.h"

using namespace dt_provenance::proxy;
using json = nlohmann::ordered_json;

TEST_CASE("HttpProxyServer starts and stops cleanly", "[proxy]") {
  HttpProxyServer server;
  bool called = false;

  server.Start("127.0.0.1", 0, 2,
               [&](const std::string&, const std::string&,
                   const std::string&, const std::string&,
                   const std::string&, const std::string&, int& status,
                   std::string& headers, std::string& body) {
                 called = true;
                 status = 200;
                 headers = "{}";
                 body = R"({"ok":true})";
               });

  // Give the server a moment to start
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  REQUIRE(server.IsRunning());

  server.Stop();
  REQUIRE_FALSE(server.IsRunning());
}

TEST_CASE("HttpProxyServer routes request with session to callback",
          "[proxy]") {
  HttpProxyServer server;
  std::string captured_session;
  std::string captured_provider;
  std::string captured_path;

  server.Start("127.0.0.1", 19091, 2,
               [&](const std::string& session_id, const std::string& provider,
                   const std::string& method, const std::string& path,
                   const std::string& headers_json, const std::string& body,
                   int& resp_status, std::string& resp_headers,
                   std::string& resp_body) {
                 captured_session = session_id;
                 captured_provider = provider;
                 captured_path = path;
                 resp_status = 200;
                 resp_headers = R"({"content-type":"application/json"})";
                 resp_body = R"({"result":"ok"})";
               });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Send request with session prefix
  httplib::Client client("127.0.0.1", 19091);
  auto res = client.Post("/_session/test-agent/v1/messages",
                          R"({"model":"claude-sonnet-4-6"})",
                          "application/json");

  REQUIRE(res);
  REQUIRE(res->status == 200);
  REQUIRE(captured_session == "test-agent");
  REQUIRE(captured_provider == "anthropic");
  REQUIRE(captured_path == "/v1/messages");

  server.Stop();
}

TEST_CASE("HttpProxyServer returns rejection for missing session", "[proxy]") {
  HttpProxyServer server;
  bool callback_called = false;

  server.Start("127.0.0.1", 19092, 2,
               [&](const std::string&, const std::string&,
                   const std::string&, const std::string&,
                   const std::string&, const std::string&, int&,
                   std::string&, std::string&) {
                 callback_called = true;
               });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Send request without session prefix
  httplib::Client client("127.0.0.1", 19092);
  auto res = client.Post("/v1/messages",
                          R"({"model":"claude-sonnet-4-6"})",
                          "application/json");

  REQUIRE(res);
  REQUIRE(res->status == 200);
  REQUIRE_FALSE(callback_called);  // Should NOT reach the forward callback

  // Verify the rejection response is valid Anthropic format
  auto body = json::parse(res->body);
  REQUIRE(body.contains("content"));
  REQUIRE(body["type"] == "message");

  server.Stop();
}

TEST_CASE("HttpProxyServer increments request counter", "[proxy]") {
  HttpProxyServer server;

  server.Start("127.0.0.1", 19093, 2,
               [](const std::string&, const std::string&,
                  const std::string&, const std::string&,
                  const std::string&, const std::string&, int& status,
                  std::string& headers, std::string& body) {
                 status = 200;
                 headers = "{}";
                 body = "{}";
               });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  httplib::Client client("127.0.0.1", 19093);
  client.Post("/_session/s1/v1/messages", "{}", "application/json");
  client.Post("/_session/s2/v1/messages", "{}", "application/json");
  // Also counts rejections
  client.Post("/v1/messages", "{}", "application/json");

  REQUIRE(server.TotalRequests() == 3);

  server.Stop();
}
