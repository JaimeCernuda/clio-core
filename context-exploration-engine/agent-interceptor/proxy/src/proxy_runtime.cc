#include "dt_provenance/proxy/proxy_runtime.h"

#include "dt_provenance/protocol/provider.h"

namespace dt_provenance::proxy {

chi::TaskResume Runtime::Create(hipc::FullPtr<CreateTask> task,
                                chi::RunContext& rctx) {
  // Extract create params
  auto& params = task->GetNewContainerParams<CreateParams>();
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
             int& resp_status, std::string& resp_headers,
             std::string& resp_body) {
        // This runs on httplib worker threads, NOT Chimaera coroutines.
        // For Phase 2 we just return 502 — Phase 3 wires up the
        // interception ChiMod dispatch.
        (void)this;
        (void)session_id;
        (void)provider_name;
        (void)method;
        (void)path;
        (void)headers_json;
        (void)body;
        resp_status = 502;
        resp_headers = "{}";
        resp_body = R"({"error":"interception not yet wired"})";
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
