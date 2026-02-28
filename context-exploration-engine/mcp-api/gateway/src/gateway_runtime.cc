/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mcp_gateway/gateway_runtime.h"
#include "mchips/mcp_gateway/gateway_tasks.h"

#include <mchips/protocol/json_rpc.h>
#include <mchips/protocol/mcp_message.h>
#include <mchips/protocol/mcp_types.h>

#include <stdexcept>

namespace mchips::mcp_gateway {

namespace {

/// Build a JSON-RPC error response (as a JSON string).
std::string MakeErrorResponse(const protocol::json& id, int code,
                               const std::string& message) {
  protocol::JsonRpcError err;
  err.code = code;
  err.message = message;
  return protocol::JsonRpcResponse::Error(id, err).ToJson().dump();
}

/// Build a JSON-RPC success response (as a JSON string).
std::string MakeSuccessResponse(const protocol::json& id,
                                 const protocol::json& result) {
  return protocol::JsonRpcResponse::Success(id, result).ToJson().dump();
}

}  // namespace

//=============================================================================
// Create — gateway startup
//=============================================================================

chi::TaskResume Runtime::Create(hipc::FullPtr<CreateTask> task,
                                 chi::RunContext& /*rctx*/) {
  // Register the three standard MChiP pool IDs
  // Pool IDs are fixed in the compose config (wrp_conf.yaml):
  //   700 = gateway (us), 701 = cte, 702 = cae, 703 = cluster
  router_.RegisterMchip("cte",     chi::PoolId(701, 0));
  router_.RegisterMchip("cae",     chi::PoolId(702, 0));
  router_.RegisterMchip("cluster", chi::PoolId(703, 0));

  // Start HTTP server via the StartHttpServer task mechanism
  const auto& params = task->GetCreateParams();
  auto host = "0.0.0.0";
  auto port = static_cast<int>(params.http_port_);
  auto threads = static_cast<int>(params.http_threads_);

  http_server_.SetRequestHandler(
      [this](const std::string& body,
             const std::string& session_id) -> HttpResponse {
        return HandleHttpRequestSync(body, session_id);
      });

  http_server_.SetDeleteHandler(
      [this](const std::string& session_id) -> HttpResponse {
        return HandleDeleteSync(session_id);
      });

  http_server_.Start(host, port, threads);

  task->return_code_ = 0;
  co_return;
}

//=============================================================================
// Destroy
//=============================================================================

chi::TaskResume Runtime::Destroy(
    hipc::FullPtr<chi::admin::DestroyPoolTask> task,
    chi::RunContext& /*rctx*/) {
  http_server_.Stop();
  task->return_code_ = 0;
  co_return;
}

//=============================================================================
// HTTP synchronous dispatch (called from httplib thread pool)
//=============================================================================

/// Handle a POST /mcp request synchronously.
///
/// This is called from the httplib thread pool — NOT a coroutine.
/// For Chimaera-integrated mode, this would submit a task to a worker
/// and block on Future::Wait(). For Stage 1 (standalone), we dispatch inline.
HttpResponse Runtime::HandleHttpRequestSync(const std::string& body,
                                             const std::string& session_id) {
  protocol::json id = nullptr;
  try {
    auto j = protocol::json::parse(body);
    auto msg = protocol::ParseMessage(j);

    // Only handle requests (has id); notifications get empty 200
    if (std::holds_alternative<protocol::JsonRpcNotification>(msg)) {
      return HttpResponse{200, "{}", "application/json"};
    }
    if (!std::holds_alternative<protocol::JsonRpcRequest>(msg)) {
      return HttpResponse{400,
                          MakeErrorResponse(nullptr, -32600, "Invalid request"),
                          "application/json"};
    }

    const auto& req = std::get<protocol::JsonRpcRequest>(msg);
    id = req.id;
    const auto& method = req.method;

    // Validate session for all methods except initialize
    if (method != protocol::methods::kInitialize) {
      if (!session_id.empty() && !session_manager_.ValidateSession(session_id)) {
        return HttpResponse{
            401,
            MakeErrorResponse(id, -32000, "Session not found or expired"),
            "application/json"};
      }
    }

    // Dispatch by method
    if (method == protocol::methods::kInitialize) {
      return HandleInitializeSync(req);
    } else if (method == protocol::methods::kToolsList) {
      return HandleToolsListSync(req);
    } else if (method == protocol::methods::kToolsCall) {
      return HandleToolsCallSync(req);
    } else if (method == protocol::methods::kPing) {
      auto resp = MakeSuccessResponse(id, protocol::json::object());
      return HttpResponse{200, resp, "application/json"};
    } else {
      return HttpResponse{
          404,
          MakeErrorResponse(id, -32601, "Method not found: " + method),
          "application/json"};
    }

  } catch (const protocol::json::parse_error& e) {
    return HttpResponse{
        400,
        MakeErrorResponse(id, -32700, std::string("Parse error: ") + e.what()),
        "application/json"};
  } catch (const std::exception& e) {
    return HttpResponse{
        500,
        MakeErrorResponse(id, -32603, std::string("Internal error: ") + e.what()),
        "application/json"};
  }
}

/// Handle the MCP initialize handshake.
HttpResponse Runtime::HandleInitializeSync(
    const protocol::JsonRpcRequest& req) {
  std::string protocol_version = protocol::kMcpProtocolVersion;
  if (req.params.has_value() && req.params->contains("protocolVersion")) {
    protocol_version = (*req.params)["protocolVersion"].get<std::string>();
  }

  auto session_id = session_manager_.CreateSession(protocol_version);

  protocol::InitializeResult result;
  protocol::ServerCapabilities::ToolsCapability tools_cap;
  tools_cap.listChanged = true;
  result.capabilities.tools = tools_cap;

  auto response_json = MakeSuccessResponse(req.id, result.ToJson());

  // Return with session ID header embedded in body for simplicity
  // (clients must also check MCP-Session-Id response header)
  protocol::json resp_with_session = protocol::json::parse(response_json);
  resp_with_session["sessionId"] = session_id;

  return HttpResponse{200, resp_with_session.dump(), "application/json"};
}

/// Handle tools/list — aggregate from all registered MChiPs.
HttpResponse Runtime::HandleToolsListSync(
    const protocol::JsonRpcRequest& req) {
  auto all_tools = router_.ListAllTools();

  protocol::json tools_arr = protocol::json::array();
  for (const auto& tool : all_tools) {
    tools_arr.push_back(tool.ToJson());
  }

  protocol::json result;
  result["tools"] = tools_arr;

  return HttpResponse{200, MakeSuccessResponse(req.id, result),
                      "application/json"};
}

/// Handle tools/call — route to the appropriate MChiP.
///
/// For Stage 1 (no Chimaera): returns "runtime not available" error.
/// For Stage 2 (Chimaera integrated): submits CallMcpToolTask.
HttpResponse Runtime::HandleToolsCallSync(
    const protocol::JsonRpcRequest& req) {
  if (!req.params.has_value()) {
    return HttpResponse{
        400,
        MakeErrorResponse(req.id, -32602, "Missing params for tools/call"),
        "application/json"};
  }

  const auto& params = *req.params;
  if (!params.contains("name")) {
    return HttpResponse{
        400,
        MakeErrorResponse(req.id, -32602, "Missing 'name' in tools/call"),
        "application/json"};
  }

  auto qualified_name = params["name"].get<std::string>();
  std::string tool_name;
  auto* route = router_.Route(qualified_name, tool_name);

  if (!route) {
    return HttpResponse{
        404,
        MakeErrorResponse(req.id, -32601,
                          "Tool not found: " + qualified_name),
        "application/json"};
  }

  protocol::json args = protocol::json::object();
  if (params.contains("arguments") && params["arguments"].is_object()) {
    args = params["arguments"];
  }

  // Stage 1: Return honest error if Chimaera runtime is not available
  // Stage 2: co_await route->client.AsyncCallMcpTool(...)
  // For now, indicate the ChiMod must be running
  (void)route;
  (void)tool_name;
  (void)args;

  // This path requires Chimaera to dispatch to the MChiP.
  // The HTTP handler submits a HandleHttpRequestTask to the Chimaera
  // worker queue and blocks. For standalone mode, return an informative error.
  protocol::json error_result;
  error_result["content"] = protocol::json::array();
  error_result["content"].push_back({
      {"type", "text"},
      {"text", "Tool call routing requires the Chimaera runtime. "
               "Start mchips_demo_server to enable MChiP routing."}
  });
  error_result["isError"] = true;

  return HttpResponse{200, MakeSuccessResponse(req.id, error_result),
                      "application/json"};
}

/// Handle HTTP DELETE (session close).
HttpResponse Runtime::HandleDeleteSync(const std::string& session_id) {
  if (!session_id.empty()) {
    session_manager_.DestroySession(session_id);
  }
  return HttpResponse{200, "{}", "application/json"};
}

//=============================================================================
// Task-based handlers (Chimaera coroutine integration)
//=============================================================================

chi::TaskResume Runtime::HandleHttpRequest(
    hipc::FullPtr<HandleHttpRequestTask> task,
    chi::RunContext& /*rctx*/) {
  std::string request_body(task->request_body_.str());
  std::string session_id(task->session_id_.str());

  auto response = HandleHttpRequestSync(request_body, session_id);

  task->response_body_ = chi::priv::string(HSHM_MALLOC, response.body);
  task->http_status_ = static_cast<chi::u32>(response.status_code);
  task->return_code_ = 0;
  co_return;
}

chi::TaskResume Runtime::InitializeSession(
    hipc::FullPtr<InitializeSessionTask> task,
    chi::RunContext& /*rctx*/) {
  std::string request_json(task->request_json_.str());

  std::string protocol_version = protocol::kMcpProtocolVersion;
  try {
    auto j = protocol::json::parse(request_json);
    if (j.contains("protocolVersion")) {
      protocol_version = j["protocolVersion"].get<std::string>();
    }
  } catch (...) {}

  auto session_id = session_manager_.CreateSession(protocol_version);

  protocol::InitializeResult result;
  protocol::ServerCapabilities::ToolsCapability tc;
  tc.listChanged = true;
  result.capabilities.tools = tc;

  task->response_json_ = chi::priv::string(HSHM_MALLOC, result.ToJson().dump());
  task->session_id_ = chi::priv::string(HSHM_MALLOC, session_id);
  task->return_code_ = 0;
  co_return;
}

chi::TaskResume Runtime::CloseSession(
    hipc::FullPtr<CloseSessionTask> task,
    chi::RunContext& /*rctx*/) {
  std::string session_id(task->session_id_.str());
  session_manager_.DestroySession(session_id);
  task->success_ = 1;
  task->return_code_ = 0;
  co_return;
}

chi::TaskResume Runtime::StartHttpServer(
    hipc::FullPtr<StartHttpServerTask> task,
    chi::RunContext& /*rctx*/) {
  std::string host(task->host_.str());
  http_server_.Start(host, static_cast<int>(task->port_),
                     static_cast<int>(task->num_threads_));
  task->success_ = 1;
  task->return_code_ = 0;
  co_return;
}

chi::TaskResume Runtime::StopHttpServer(
    hipc::FullPtr<StopHttpServerTask> task,
    chi::RunContext& /*rctx*/) {
  http_server_.Stop();
  task->success_ = 1;
  task->return_code_ = 0;
  co_return;
}

}  // namespace mchips::mcp_gateway
