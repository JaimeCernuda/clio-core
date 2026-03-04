#include "dt_provenance/proxy/proxy_runtime.h"

#include <chrono>

#include <chimaera/pool_manager.h>

#include "dt_provenance/protocol/provider.h"

namespace dt_provenance::proxy {

chi::TaskResume Runtime::Create(hipc::FullPtr<CreateTask> task,
                                chi::RunContext& rctx) {
  // Extract create params
  CreateParams params = task->GetParams();
  uint16_t port = params.port_;
  int num_threads = params.num_threads_;

  HLOG(kInfo, "DTProvenance proxy starting on port {} with {} threads", port,
       num_threads);

  // Start HTTP server with a callback that dispatches to interception ChiMods
  http_server_.Start(
      "0.0.0.0", port, num_threads,
      [this](const std::string& session_id, const std::string& provider_name,
             const std::string& method, const std::string& path,
             const std::string& headers_json, const std::string& body,
             uint64_t stream_buffer_id,
             int& resp_status, std::string& resp_headers,
             std::string& resp_body) {
        // Lazy-init: discover interception pools on first request
        // (pools may not exist yet during Create() due to compose ordering)
        if (!clients_initialized_) {
          auto* pool_mgr = CHI_POOL_MANAGER;
          chi::PoolId anthropic_pool =
              pool_mgr->FindPoolByName("dt_intercept_anthropic_pool");
          chi::PoolId openai_pool =
              pool_mgr->FindPoolByName("dt_intercept_openai_pool");
          chi::PoolId ollama_pool =
              pool_mgr->FindPoolByName("dt_intercept_ollama_pool");

          if (!anthropic_pool.IsNull()) {
            anthropic_client_.Init(anthropic_pool);
          }
          if (!openai_pool.IsNull()) {
            openai_client_.Init(openai_pool);
          }
          if (!ollama_pool.IsNull()) {
            ollama_client_.Init(ollama_pool);
          }
          clients_initialized_ = true;
        }

        // Repurpose request_time_ns_ as StreamBuffer ID.
        // When stream_buffer_id > 0, the ChiMod streams via StreamBuffer.
        // When 0, use a real timestamp for the buffered path.
        chi::u64 now_ns = stream_buffer_id;
        if (now_ns == 0) {
          now_ns = static_cast<chi::u64>(
              std::chrono::duration_cast<std::chrono::nanoseconds>(
                  std::chrono::steady_clock::now().time_since_epoch()).count());
        }

        chi::PoolQuery local_query = chi::PoolQuery::Local();

        if (provider_name == "anthropic") {
          auto future = anthropic_client_.AsyncInterceptAndForward(
              local_query, session_id, path, headers_json, body, now_ns);
          future.Wait();
          if (stream_buffer_id == 0) {
            resp_status = future->response_status_;
            resp_headers = std::string(future->response_headers_json_.str());
            resp_body = std::string(future->response_body_.str());
          }
        } else if (provider_name == "openai") {
          auto future = openai_client_.AsyncInterceptAndForward(
              local_query, session_id, path, headers_json, body, now_ns);
          future.Wait();
          if (stream_buffer_id == 0) {
            resp_status = future->response_status_;
            resp_headers = std::string(future->response_headers_json_.str());
            resp_body = std::string(future->response_body_.str());
          }
        } else if (provider_name == "ollama") {
          auto future = ollama_client_.AsyncInterceptAndForward(
              local_query, session_id, path, headers_json, body, now_ns);
          future.Wait();
          if (stream_buffer_id == 0) {
            resp_status = future->response_status_;
            resp_headers = std::string(future->response_headers_json_.str());
            resp_body = std::string(future->response_body_.str());
          }
        } else {
          resp_status = 400;
          resp_headers = "{}";
          resp_body = R"({"error":"unknown provider: )" + provider_name + R"("})";
        }
      });

  HLOG(kInfo, "DTProvenance proxy started successfully on port {}", port);
  co_return;
}

chi::TaskResume Runtime::Monitor(hipc::FullPtr<MonitorTask> task,
                                 chi::RunContext& rctx) {
  // Return basic stats
  (void)task;
  (void)rctx;
  co_return;
}

chi::TaskResume Runtime::Destroy(hipc::FullPtr<DestroyTask> task,
                                 chi::RunContext& rctx) {
  HLOG(kInfo, "DTProvenance proxy shutting down");
  http_server_.Stop();
  (void)task;
  (void)rctx;
  co_return;
}

chi::u64 Runtime::GetWorkRemaining() const {
  return http_server_.IsRunning() ? 1 : 0;
}

}  // namespace dt_provenance::proxy

CHI_TASK_CC(dt_provenance::proxy::Runtime)
