#include "dt_provenance/interception/anthropic/anthropic_runtime.h"

#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

#include <chimaera/pool_manager.h>

#include "dt_provenance/protocol/anthropic_parser.h"
#include "dt_provenance/protocol/cost_estimator.h"
#include "dt_provenance/protocol/interaction.h"
#include "dt_provenance/protocol/stream_buffer.h"
#include "dt_provenance/protocol/stream_reassembly.h"

namespace dt_provenance::interception::anthropic {

using json = nlohmann::ordered_json;
using namespace dt_provenance::protocol;

/**
 * Parse a URL into host, port, and ssl flag
 */
static void ParseUrl(const std::string& url, std::string& host, int& port,
                     bool& ssl) {
  if (url.starts_with("https://")) {
    ssl = true;
    host = url.substr(8);
    port = 443;
  } else if (url.starts_with("http://")) {
    ssl = false;
    host = url.substr(7);
    port = 80;
  } else {
    host = url;
    port = 443;
    ssl = true;
  }

  // Check for port in host
  auto colon = host.find(':');
  if (colon != std::string::npos) {
    port = std::stoi(host.substr(colon + 1));
    host = host.substr(0, colon);
  }

  // Strip trailing slash
  if (!host.empty() && host.back() == '/') {
    host.pop_back();
  }
}

chi::TaskResume Runtime::Create(hipc::FullPtr<CreateTask> task,
                                chi::RunContext& rctx) {
  CreateParams params = task->GetParams();
  std::string url(params.upstream_base_url_.str());

  ParseUrl(url, upstream_host_, upstream_port_, upstream_ssl_);

  HLOG(kInfo, "Anthropic interception ChiMod created: upstream={}:{} ssl={}",
       upstream_host_, upstream_port_, upstream_ssl_);
  co_return;
}

chi::TaskResume Runtime::InterceptAndForward(
    hipc::FullPtr<InterceptAndForwardTask> task, chi::RunContext& rctx) {
  // Lazy-init tracker client (tracker pool may not exist during Create())
  if (!tracker_initialized_) {
    auto* pool_mgr = CHI_POOL_MANAGER;
    chi::PoolId tracker_pool = pool_mgr->FindPoolByName("dt_tracker_pool");
    if (!tracker_pool.IsNull()) {
      tracker_client_.Init(tracker_pool);
      tracker_initialized_ = true;
    }
  }

  active_requests_.fetch_add(1);
  auto start = std::chrono::steady_clock::now();

  // Extract task fields (local copies — safe after co_return)
  std::string session_id(task->session_id_.str());
  std::string path(task->path_.str());
  std::string headers_json_str(task->headers_json_.str());
  std::string request_body(task->request_body_.str());

  // Parse headers
  json request_headers;
  try {
    request_headers = json::parse(headers_json_str);
  } catch (const json::parse_error&) {
    request_headers = json::object();
  }

  // 1. Create httplib headers
  httplib::Headers hdr;
  for (auto& [k, v] : request_headers.items()) {
    if (v.is_string()) {
      hdr.emplace(k, v.get<std::string>());
    }
  }

  // Check for streaming via StreamBuffer registry
  using dt_provenance::protocol::StreamBufferRegistry;
  auto stream_buf = StreamBufferRegistry::Instance().Get(task->request_time_ns_);
  bool is_streaming = (stream_buf != nullptr);

  if (is_streaming) {
    // Offload the blocking HTTP call to a detached thread so the Chimaera
    // cooperative worker returns immediately and can process other tasks.
    std::thread([this,
                 session_id = std::move(session_id),
                 path = std::move(path),
                 request_headers = std::move(request_headers),
                 request_body = std::move(request_body),
                 hdr = std::move(hdr),
                 stream_buf = std::move(stream_buf),
                 start]() {
      std::string response_body;
      int response_status = 502;
      httplib::Headers response_headers;
      double ttft_ms = 0;

      httplib::Request http_req;
      http_req.method = "POST";
      http_req.path = path;
      http_req.headers = hdr;
      http_req.body = request_body;
      http_req.headers.emplace("Content-Type", "application/json");

      bool first_chunk = true;

      http_req.response_handler = [&](const httplib::Response& resp) -> bool {
        response_status = resp.status;
        response_headers = resp.headers;
        json rh = json::object();
        for (const auto& [k, v] : resp.headers) rh[k] = v;
        stream_buf->SetResponseHeaders(resp.status, rh.dump());
        return true;
      };

      http_req.content_receiver =
          [&](const char* data, size_t len, uint64_t, uint64_t) -> bool {
        if (first_chunk) {
          ttft_ms = std::chrono::duration<double, std::milli>(
              std::chrono::steady_clock::now() - start).count();
          first_chunk = false;
        }
        stream_buf->PushChunk(std::string(data, len));
        return true;
      };

      if (upstream_ssl_) {
        httplib::SSLClient cli(upstream_host_, upstream_port_);
        cli.set_connection_timeout(30);
        cli.set_read_timeout(300);
        cli.enable_server_certificate_verification(false);
        auto result = cli.send(http_req);
        if (!result)
          stream_buf->SetError(502, R"({"error":"upstream connection failed"})");
      } else {
        httplib::Client cli(upstream_host_, upstream_port_);
        cli.set_connection_timeout(30);
        cli.set_read_timeout(300);
        auto result = cli.send(http_req);
        if (!result)
          stream_buf->SetError(502, R"({"error":"upstream connection failed"})");
      }

      stream_buf->Complete();
      response_body = stream_buf->GetCollectedBody();

      auto end = std::chrono::steady_clock::now();
      double latency_ms =
          std::chrono::duration<double, std::milli>(end - start).count();

      // Parse interaction and dispatch to tracker
      if (response_status >= 200 && response_status < 300) {
        InteractionRecord record;
        record.session_id = session_id;
        record.provider = Provider::kAnthropic;
        record.request.method = "POST";
        record.request.path = path;
        record.request.headers = request_headers;

        try {
          auto req_body = json::parse(request_body);
          AnthropicParser::ParseRequest(req_body, record);
        } catch (const json::parse_error&) {}

        json resp_hdrs_json = json::object();
        for (const auto& [k, v] : response_headers) {
          resp_hdrs_json[k] = v;
        }

        bool is_sse = false;
        if (resp_hdrs_json.contains("content-type")) {
          std::string ct = resp_hdrs_json["content-type"].get<std::string>();
          is_sse = ct.find("text/event-stream") != std::string::npos;
        }

        if (is_sse) {
          record.response.is_streaming = true;
          auto chunks = ReassembleSSE(response_body);
          for (const auto& chunk : chunks) {
            AnthropicParser::ParseStreamChunk(chunk, record);
          }
        } else {
          record.response.is_streaming = false;
          try {
            auto resp_body = json::parse(response_body);
            AnthropicParser::ParseResponse(resp_body, record);
          } catch (const json::parse_error&) {}
        }

        TokenUsage usage;
        usage.input_tokens = record.metrics.input_tokens;
        usage.output_tokens = record.metrics.output_tokens;
        usage.cache_creation_tokens = record.metrics.cache_creation_tokens;
        usage.cache_read_tokens = record.metrics.cache_read_tokens;
        auto cost = CostEstimator::Estimate(Provider::kAnthropic,
                                            record.model, usage);
        record.metrics.cost_usd = cost.total_cost;
        record.metrics.total_latency_ms = latency_ms;
        record.metrics.time_to_first_token_ms = ttft_ms;

        record.response.status_code = response_status;

        json interaction_json = record.ToJson();
        std::string interaction_str = interaction_json.dump();
        HLOG(kInfo,
             "Dispatching to tracker: session={} model={} in={} out={} "
             "cost=${} latency={}ms",
             session_id, record.model, record.metrics.input_tokens,
             record.metrics.output_tokens, record.metrics.cost_usd,
             latency_ms);

        if (tracker_initialized_) {
          auto tracker_future = tracker_client_.AsyncStoreInteraction(
              chi::PoolQuery::Local(), interaction_str);
          tracker_future.Wait();
        }
      }

      active_requests_.fetch_sub(1);
    }).detach();
    co_return;
  }

  // --- NON-STREAMING PATH (existing buffered approach) ---
  std::string response_body;
  int response_status = 502;
  httplib::Headers response_headers;

  if (upstream_ssl_) {
    httplib::SSLClient cli(upstream_host_, upstream_port_);
    cli.set_connection_timeout(30);
    cli.set_read_timeout(300);
    cli.enable_server_certificate_verification(false);

    auto res = cli.Post(path, hdr, request_body, "application/json");
    if (res) {
      response_status = res->status;
      response_body = res->body;
      response_headers = res->headers;
    }
  } else {
    httplib::Client cli(upstream_host_, upstream_port_);
    cli.set_connection_timeout(30);
    cli.set_read_timeout(300);

    auto res = cli.Post(path, hdr, request_body, "application/json");
    if (res) {
      response_status = res->status;
      response_body = res->body;
      response_headers = res->headers;
    }
  }

  auto end = std::chrono::steady_clock::now();
  double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();

  // 2. Set OUT fields
  task->response_status_ = response_status;
  task->latency_ms_ = latency_ms;
  task->ttft_ms_ = latency_ms;

  // Serialize response headers
  json resp_hdrs_json = json::object();
  for (const auto& [k, v] : response_headers) {
    resp_hdrs_json[k] = v;
  }
  task->response_headers_json_ = resp_hdrs_json.dump();
  task->response_body_ = response_body;

  // 3. Parse interaction
  if (response_status >= 200 && response_status < 300) {
    InteractionRecord record;
    record.session_id = session_id;
    record.provider = Provider::kAnthropic;
    record.request.method = "POST";
    record.request.path = path;
    record.request.headers = request_headers;

    // Parse request
    try {
      auto req_body = json::parse(request_body);
      AnthropicParser::ParseRequest(req_body, record);
    } catch (const json::parse_error&) {}

    // Detect streaming response by content-type
    bool is_sse = false;
    if (resp_hdrs_json.contains("content-type")) {
      std::string ct = resp_hdrs_json["content-type"].get<std::string>();
      is_sse = ct.find("text/event-stream") != std::string::npos;
    }

    // Parse response
    if (is_sse) {
      record.response.is_streaming = true;
      auto chunks = ReassembleSSE(response_body);
      for (const auto& chunk : chunks) {
        AnthropicParser::ParseStreamChunk(chunk, record);
      }
    } else {
      record.response.is_streaming = false;
      try {
        auto resp_body = json::parse(response_body);
        AnthropicParser::ParseResponse(resp_body, record);
      } catch (const json::parse_error&) {}
    }

    // Estimate cost
    TokenUsage usage;
    usage.input_tokens = record.metrics.input_tokens;
    usage.output_tokens = record.metrics.output_tokens;
    usage.cache_creation_tokens = record.metrics.cache_creation_tokens;
    usage.cache_read_tokens = record.metrics.cache_read_tokens;
    auto cost = CostEstimator::Estimate(Provider::kAnthropic, record.model, usage);
    record.metrics.cost_usd = cost.total_cost;
    record.metrics.total_latency_ms = latency_ms;
    record.metrics.time_to_first_token_ms = latency_ms;

    record.response.status_code = response_status;

    // 4. Dispatch to Tracker
    json interaction_json = record.ToJson();
    std::string interaction_str = interaction_json.dump();
    HLOG(kInfo,
         "Dispatching to tracker: session={} model={} in={} out={} "
         "cost=${} latency={}ms",
         session_id, record.model, record.metrics.input_tokens,
         record.metrics.output_tokens, record.metrics.cost_usd, latency_ms);

    if (tracker_initialized_) {
      auto tracker_future = tracker_client_.AsyncStoreInteraction(
          chi::PoolQuery::Local(), interaction_str);
      tracker_future.Wait();
    }
  }

  active_requests_.fetch_sub(1);
  co_return;
}

chi::TaskResume Runtime::Monitor(hipc::FullPtr<MonitorTask> task,
                                 chi::RunContext& rctx) {
  (void)task;
  (void)rctx;
  co_return;
}

chi::TaskResume Runtime::Destroy(hipc::FullPtr<DestroyTask> task,
                                 chi::RunContext& rctx) {
  HLOG(kInfo, "Anthropic interception ChiMod destroyed");
  (void)task;
  (void)rctx;
  co_return;
}

chi::u64 Runtime::GetWorkRemaining() const {
  return active_requests_.load();
}

}  // namespace dt_provenance::interception::anthropic

CHI_TASK_CC(dt_provenance::interception::anthropic::Runtime)
