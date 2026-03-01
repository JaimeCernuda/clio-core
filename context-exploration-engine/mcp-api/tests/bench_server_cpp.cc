/*
 * Standalone C++ MCP benchmark server.
 *
 * Implements echo(message) and add(a,b) tools using cpp-httplib,
 * without Chimaera dependencies. Used as baseline for scalability
 * comparison against the IOWarp MChiPs gateway.
 *
 * Usage:
 *   ./bench_server_cpp [--port 9098] [--host 0.0.0.0] [--threads 8]
 */

#include <mchips/protocol/json_rpc.h>
#include <mchips/protocol/mcp_error.h>
#include <mchips/protocol/mcp_message.h>
#include <mchips/protocol/mcp_types.h>
#include <mchips/protocol/schema_generator.h>

#include <httplib.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <vector>

using namespace mchips::protocol;

static std::atomic<bool> g_running{true};

static void SignalHandler(int /*sig*/) { g_running = false; }

/// Standalone MCP server with echo and add tools.
class BenchMcpServer {
 public:
  void Configure(const std::string& host, int port, int threads) {
    host_ = host;
    port_ = port;

    server_.new_task_queue = [threads] {
      return new httplib::ThreadPool(threads);
    };

    // Register tools
    tools_.push_back(
        ToolBuilder("echo")
            .Description("Echo the input message back")
            .AddParam("message", SchemaType::String, "Message to echo", true)
            .Build());

    tools_.push_back(
        ToolBuilder("add")
            .Description("Add two numbers")
            .AddParam("a", SchemaType::Number, "First number", true)
            .AddParam("b", SchemaType::Number, "Second number", true)
            .Build());

    server_.Post("/mcp", [this](const httplib::Request& req,
                                 httplib::Response& res) {
      HandlePost(req, res);
    });

    server_.Delete("/mcp", [](const httplib::Request& /*req*/,
                               httplib::Response& res) {
      res.status = 200;
      res.set_content("{}", "application/json");
    });
  }

  void Run() {
    std::cout << "C++ MCP benchmark server listening on "
              << host_ << ":" << port_ << "/mcp\n";
    std::cout << "Tools: echo, add\n";
    server_.listen(host_, port_);
  }

  void Stop() { server_.stop(); }

 private:
  void HandlePost(const httplib::Request& req, httplib::Response& res) {
    json id = nullptr;
    try {
      auto j = json::parse(req.body);
      auto msg = ParseMessage(j);

      // Handle notifications silently
      if (std::holds_alternative<JsonRpcNotification>(msg)) {
        res.status = 200;
        res.set_content("{}", "application/json");
        return;
      }

      if (!std::holds_alternative<JsonRpcRequest>(msg)) {
        auto err = MakeError(McpErrorCode::InvalidRequest, "Expected request");
        res.set_content(
            JsonRpcResponse::Error(nullptr, err).ToJson().dump(),
            "application/json");
        res.status = 400;
        return;
      }

      const auto& request = std::get<JsonRpcRequest>(msg);
      id = request.id;

      if (request.method == methods::kInitialize) {
        HandleInitialize(request, res);
      } else if (request.method == methods::kToolsList) {
        HandleToolsList(request, res);
      } else if (request.method == methods::kToolsCall) {
        HandleToolsCall(request, res);
      } else if (request.method == methods::kPing) {
        res.set_content(
            JsonRpcResponse::Success(id, json::object()).ToJson().dump(),
            "application/json");
      } else {
        auto err = MakeError(McpErrorCode::MethodNotFound,
                              "Method not found: " + request.method);
        res.set_content(
            JsonRpcResponse::Error(id, err).ToJson().dump(),
            "application/json");
        res.status = 404;
      }

    } catch (const json::parse_error& e) {
      auto err = MakeError(McpErrorCode::ParseError,
                            std::string("Parse error: ") + e.what());
      res.set_content(
          JsonRpcResponse::Error(id, err).ToJson().dump(),
          "application/json");
      res.status = 400;
    } catch (const std::exception& e) {
      auto err = MakeError(McpErrorCode::InternalError,
                            std::string("Internal error: ") + e.what());
      res.set_content(
          JsonRpcResponse::Error(id, err).ToJson().dump(),
          "application/json");
      res.status = 500;
    }
  }

  void HandleInitialize(const JsonRpcRequest& req, httplib::Response& res) {
    InitializeResult result;
    ServerCapabilities::ToolsCapability tc;
    tc.listChanged = true;
    result.capabilities.tools = tc;

    session_id_ = "bench-session-" + std::to_string(++session_counter_);

    auto body = JsonRpcResponse::Success(req.id, result.ToJson()).ToJson().dump();
    res.set_content(body, "application/json");
    res.set_header("Mcp-Session-Id", session_id_);
    res.status = 200;
  }

  void HandleToolsList(const JsonRpcRequest& req, httplib::Response& res) {
    json tools_arr = json::array();
    for (const auto& tool : tools_) {
      tools_arr.push_back(tool.ToJson());
    }
    json result;
    result["tools"] = tools_arr;

    res.set_content(
        JsonRpcResponse::Success(req.id, result).ToJson().dump(),
        "application/json");
    res.status = 200;
  }

  void HandleToolsCall(const JsonRpcRequest& req, httplib::Response& res) {
    if (!req.params.has_value() || !req.params->contains("name")) {
      auto err = MakeError(McpErrorCode::InvalidParams,
                            "Missing 'name' in tools/call");
      res.set_content(
          JsonRpcResponse::Error(req.id, err).ToJson().dump(),
          "application/json");
      res.status = 400;
      return;
    }

    auto name = (*req.params)["name"].get<std::string>();
    json args = req.params->value("arguments", json::object());

    CallToolResult tool_result;
    ContentItem item;
    item.type = "text";

    if (name == "echo") {
      auto message = args.value("message", "");
      item.text = message;
      tool_result.isError = false;
    } else if (name == "add") {
      auto a = args.value("a", 0.0);
      auto b = args.value("b", 0.0);
      item.text = std::to_string(a + b);
      tool_result.isError = false;
    } else {
      item.text = "Unknown tool: " + name;
      tool_result.isError = true;
    }

    tool_result.content.push_back(item);
    res.set_content(
        JsonRpcResponse::Success(req.id, tool_result.ToJson()).ToJson().dump(),
        "application/json");
    res.status = 200;
  }

  httplib::Server server_;
  std::string host_;
  int port_ = 9098;
  std::vector<ToolDefinition> tools_;
  std::string session_id_;
  std::atomic<int> session_counter_{0};
};

static BenchMcpServer* g_server = nullptr;

int main(int argc, char** argv) {
  std::string host = "0.0.0.0";
  int port = 9098;
  int threads = 8;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--port" && i + 1 < argc) port = std::atoi(argv[++i]);
    else if (arg == "--host" && i + 1 < argc) host = argv[++i];
    else if (arg == "--threads" && i + 1 < argc) threads = std::atoi(argv[++i]);
    else if (arg == "--help") {
      std::cout << "Usage: " << argv[0]
                << " [--port 9098] [--host 0.0.0.0] [--threads 8]\n";
      return 0;
    }
  }

  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  BenchMcpServer server;
  g_server = &server;
  server.Configure(host, port, threads);

  std::cout << "Starting C++ standalone MCP benchmark server...\n";
  server.Run();

  return 0;
}
