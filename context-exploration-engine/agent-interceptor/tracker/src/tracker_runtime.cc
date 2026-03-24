#include "dt_provenance/tracker/tracker_runtime.h"

#include <wrp_cte/core/core_client.h>
#include <hermes_shm/serialize/msgpack_wrapper.h>
#include <nlohmann/json.hpp>
#include <cstring>

#include "dt_provenance/ctx_untangler/ctx_untangler_client.h"
#include "dt_provenance/tracker/failure_detector.h"

namespace dt_provenance::tracker {

using json = nlohmann::ordered_json;

Runtime::~Runtime() = default;

chi::TaskStat Runtime::GetTaskStats(chi::u32 method_id) const {
  // StoreInteraction performs CTE PutBlob I/O — report io_size >= 4096
  // so the DefaultScheduler routes it to an I/O worker instead of worker 0.
  if (method_id == Method::kStoreInteraction) {
    chi::TaskStat stat;
    stat.io_size_ = 8192;
    return stat;
  }
  return chi::TaskStat();
}

std::string Runtime::FormatBlobName(uint64_t seq_id) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%010lu", static_cast<unsigned long>(seq_id));
  return std::string(buf);
}

std::string Runtime::BuildTagName(const std::string& session_id) {
  return "Agentic_session_" + session_id;
}

chi::TaskResume Runtime::Create(hipc::FullPtr<CreateTask> task,
                                chi::RunContext& rctx) {
  // Initialize the CTE client with the correct pool ID if needed.
  {
    auto *cte = WRP_CTE_CLIENT;
    if (cte->pool_id_.IsNull()) {
      chi::PoolId cte_pool = CHI_POOL_MANAGER->FindPoolByName("cte_main");
      if (!cte_pool.IsNull()) {
        cte->Init(cte_pool);
        HLOG(kInfo, "Tracker: initialized CTE client with pool_id={}", cte_pool);
      }
    }
  }

  // Recover sequence_counter_ from existing CTE tags
  try {
    auto tags = WRP_CTE_CLIENT->AsyncTagQuery("Agentic_session_.*");
    co_await tags;
    auto tag_names = tags->results_;
    uint64_t max_seq = 0;
    for (const auto& tname : tag_names) {
      auto tag_future = WRP_CTE_CLIENT->AsyncGetOrCreateTag(tname);
      co_await tag_future;
      auto tag_id = tag_future->tag_id_;
      auto blobs_future = WRP_CTE_CLIENT->AsyncGetContainedBlobs(tag_id);
      co_await blobs_future;
      auto& blobs = blobs_future->blob_names_;
      for (const auto& bname : blobs) {
        try {
          uint64_t seq = std::stoull(bname);
          if (seq > max_seq) max_seq = seq;
        } catch (...) {}
      }
    }
    if (max_seq > 0) {
      sequence_counter_.store(max_seq);
      HLOG(kInfo, "Recovered sequence_counter_ to {}", max_seq);
    }
  } catch (...) {
    HLOG(kWarning, "CTE tag scan failed during Create — starting fresh");
  }

  HLOG(kInfo, "Conversation Tracker ChiMod created (CTE-backed)");
  co_return;
}

chi::TaskResume Runtime::StoreInteraction(
    hipc::FullPtr<StoreInteractionTask> task, chi::RunContext& rctx) {
  // 1. Atomic increment for monotonic sequence ID
  uint64_t seq_id = sequence_counter_.fetch_add(1) + 1;

  // 2. Parse interaction JSON
  std::string interaction_str(task->interaction_json_.str());
  json interaction;
  try {
    interaction = json::parse(interaction_str);
  } catch (const json::parse_error& e) {
    HLOG(kError, "Failed to parse interaction JSON: {}", e.what());
    task->sequence_id_ = 0;
    co_return;
  }

  // 3. Set sequence_id in the interaction
  interaction["sequence_id"] = seq_id;

  // 4. Extract session_id
  std::string session_id = interaction.value("session_id", "default");
  std::string tag_name = BuildTagName(session_id);
  std::string blob_name = FormatBlobName(seq_id);

  // 5. Resolve conversation threading
  auto record = dt_provenance::protocol::InteractionRecord::FromJson(interaction);
  record.sequence_id = seq_id;
  threader_.ResolveThreading(record);
  interaction["conversation"] = record.conversation.ToJson();

  // 6. Store in CTE (co_await instead of blocking Tag)
  std::string data = interaction.dump();
  try {
    auto tag_future = WRP_CTE_CLIENT->AsyncGetOrCreateTag(tag_name);
    co_await tag_future;
    auto tag_id = tag_future->tag_id_;

    auto *ipc = CHI_IPC;
    auto shm = ipc->AllocateBuffer(data.size());
    if (shm.IsNull()) {
      HLOG(kError, "StoreInteraction: AllocateBuffer({}) failed", data.size());
      task->sequence_id_ = 0;
      co_return;
    }
    memcpy(shm.ptr_, data.c_str(), data.size());
    auto put_future = WRP_CTE_CLIENT->AsyncPutBlob(
        tag_id, blob_name, 0, data.size(), hipc::ShmPtr<>(shm.shm_));
    co_await put_future;
    ipc->FreeBuffer(shm);
  } catch (const std::exception& e) {
    HLOG(kError, "CTE PutBlob failed: {}", e.what());
    task->sequence_id_ = 0;
    co_return;
  }

  HLOG(kDebug, "Stored interaction seq={} tag={} blob={}", seq_id, tag_name,
       blob_name);

  // 7. Auto-generate recovery events for detected failures (best-effort)
  {
    auto failure_events = FailureDetector::Detect(interaction);
    for (auto& event : failure_events) {
      std::string recovery_tag = "Recovery_" + session_id;
      std::string event_id = event.value("event_id", "");
      std::string event_data = event.dump();
      try {
        auto rtag_future = WRP_CTE_CLIENT->AsyncGetOrCreateTag(recovery_tag);
        co_await rtag_future;
        auto rtag_id = rtag_future->tag_id_;

        auto *ipc = CHI_IPC;
        auto shm = ipc->AllocateBuffer(event_data.size());
        if (!shm.IsNull()) {
          memcpy(shm.ptr_, event_data.c_str(), event_data.size());
          auto put_future = WRP_CTE_CLIENT->AsyncPutBlob(
              rtag_id, event_id, 0, event_data.size(), hipc::ShmPtr<>(shm.shm_));
          co_await put_future;
          ipc->FreeBuffer(shm);
          HLOG(kInfo, "Auto recovery event: session={} type={} event_id={}",
               session_id, event["payload"].value("error_type", ""), event_id);
        }
      } catch (const std::exception& e) {
        HLOG(kWarning, "Failed to store auto recovery event: {}", e.what());
      }
    }
  }

  // 9. Dispatch to Ctx Untangler for diff computation
  if (!untangler_initialized_) {
    chi::PoolId pool = CHI_POOL_MANAGER->FindPoolByName("dt_ctx_untangler_pool");
    if (!pool.IsNull()) {
      untangler_client_ = std::make_unique<dt_provenance::ctx_untangler::Client>(pool);
      untangler_initialized_ = true;
    }
  }
  if (untangler_initialized_) {
    auto f = untangler_client_->AsyncComputeDiff(
        chi::PoolQuery::Local(), session_id, seq_id);
    co_await f;
  }

  task->sequence_id_ = seq_id;
  co_return;
}

chi::TaskResume Runtime::QuerySession(hipc::FullPtr<QuerySessionTask> task,
                                      chi::RunContext& rctx) {
  std::string session_id(task->session_id_.str());
  std::string tag_name = BuildTagName(session_id);

  json result = json::array();
  try {
    auto tag_future = WRP_CTE_CLIENT->AsyncGetOrCreateTag(tag_name);
    co_await tag_future;
    auto tag_id = tag_future->tag_id_;

    auto blobs_future = WRP_CTE_CLIENT->AsyncGetContainedBlobs(tag_id);
    co_await blobs_future;
    auto& blobs = blobs_future->blob_names_;

    auto *ipc = CHI_IPC;
    for (const auto& bname : blobs) {
      auto size_future = WRP_CTE_CLIENT->AsyncGetBlobSize(tag_id, bname);
      co_await size_future;
      auto size = size_future->size_;
      if (size == 0) continue;

      auto shm = ipc->AllocateBuffer(size);
      if (shm.IsNull()) continue;
      auto get_future = WRP_CTE_CLIENT->AsyncGetBlob(
          tag_id, bname, 0, size, 0, hipc::ShmPtr<>(shm.shm_));
      co_await get_future;
      try {
        result.push_back(json::parse(
            std::string(reinterpret_cast<char*>(shm.ptr_), size)));
      } catch (const json::parse_error&) {}
      ipc->FreeBuffer(shm);
    }
  } catch (...) {
    // Tag doesn't exist yet — return empty array
  }

  task->interactions_json_ = result.dump();
  co_return;
}

chi::TaskResume Runtime::ListSessions(hipc::FullPtr<ListSessionsTask> task,
                                      chi::RunContext& rctx) {
  json result = json::array();
  try {
    auto future = WRP_CTE_CLIENT->AsyncTagQuery("Agentic_session_.*");
    co_await future;
    auto tag_names = future->results_;
    const std::string prefix = "Agentic_session_";
    for (const auto& tag_name : tag_names) {
      std::string session_id = tag_name;
      if (session_id.starts_with(prefix)) {
        session_id = session_id.substr(prefix.size());
      }
      auto tag_future = WRP_CTE_CLIENT->AsyncGetOrCreateTag(tag_name);
      co_await tag_future;
      auto tag_id = tag_future->tag_id_;

      auto blobs_future = WRP_CTE_CLIENT->AsyncGetContainedBlobs(tag_id);
      co_await blobs_future;
      auto& blobs = blobs_future->blob_names_;

      result.push_back(json{
          {"session_id", session_id},
          {"count", blobs.size()},
          {"tag_name", tag_name}});
    }
  } catch (...) {
    // CTE not available — return empty
  }

  task->sessions_json_ = result.dump();
  co_return;
}

chi::TaskResume Runtime::GetInteraction(
    hipc::FullPtr<GetInteractionTask> task, chi::RunContext& rctx) {
  std::string session_id(task->session_id_.str());
  uint64_t seq_id = task->sequence_id_;
  std::string tag_name = BuildTagName(session_id);
  std::string blob_name = FormatBlobName(seq_id);

  try {
    auto tag_future = WRP_CTE_CLIENT->AsyncGetOrCreateTag(tag_name);
    co_await tag_future;
    auto tag_id = tag_future->tag_id_;

    auto size_future = WRP_CTE_CLIENT->AsyncGetBlobSize(tag_id, blob_name);
    co_await size_future;
    auto size = size_future->size_;

    if (size > 0) {
      auto *ipc = CHI_IPC;
      auto shm = ipc->AllocateBuffer(size);
      if (!shm.IsNull()) {
      auto get_future = WRP_CTE_CLIENT->AsyncGetBlob(
          tag_id, blob_name, 0, size, 0, hipc::ShmPtr<>(shm.shm_));
      co_await get_future;
      task->interaction_json_ = std::string(
          reinterpret_cast<char*>(shm.ptr_), size);
      ipc->FreeBuffer(shm);
      co_return;
      }
    }
  } catch (...) {}

  task->interaction_json_ = "{}";
  co_return;
}

chi::TaskResume Runtime::Monitor(hipc::FullPtr<MonitorTask> task,
                                 chi::RunContext& rctx) {
  const std::string& query = task->query_;
  HLOG(kInfo, "Tracker Monitor: query='{}'", query);

  if (query == "list_sessions") {
    // Return array of {session_id, count, tag_name}
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> pk(sbuf);

    try {
      HLOG(kInfo, "Tracker Monitor: calling AsyncTagQuery");
      auto future = WRP_CTE_CLIENT->AsyncTagQuery("Agentic_session_.*");
      HLOG(kInfo, "Tracker Monitor: co_await tag query...");
      co_await future;
      HLOG(kInfo, "Tracker Monitor: tag query returned");
      auto tag_names = future->results_;
      const std::string prefix = "Agentic_session_";

      pk.pack_array(static_cast<uint32_t>(tag_names.size()));
      for (const auto& tag_name : tag_names) {
        std::string session_id = tag_name;
        if (session_id.starts_with(prefix)) {
          session_id = session_id.substr(prefix.size());
        }
        auto tag_future = WRP_CTE_CLIENT->AsyncGetOrCreateTag(tag_name);
        co_await tag_future;
        auto tag_id = tag_future->tag_id_;

        auto blobs_future = WRP_CTE_CLIENT->AsyncGetContainedBlobs(tag_id);
        co_await blobs_future;
        auto& blobs = blobs_future->blob_names_;

        pk.pack_map(3);
        pk.pack("session_id"); pk.pack(session_id);
        pk.pack("count"); pk.pack(static_cast<uint64_t>(blobs.size()));
        pk.pack("tag_name"); pk.pack(tag_name);
      }
    } catch (...) {
      pk.pack_array(0);
    }

    task->results_[container_id_] = std::string(sbuf.data(), sbuf.size());

  } else if (query.rfind("query_session://", 0) == 0) {
    // Return all interaction JSONs for a session
    std::string session_id = query.substr(16);
    std::string tag_name = BuildTagName(session_id);

    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> pk(sbuf);

    try {
      auto tag_future = WRP_CTE_CLIENT->AsyncGetOrCreateTag(tag_name);
      co_await tag_future;
      auto tag_id = tag_future->tag_id_;

      auto blobs_future = WRP_CTE_CLIENT->AsyncGetContainedBlobs(tag_id);
      co_await blobs_future;
      auto& blobs = blobs_future->blob_names_;

      auto *ipc = CHI_IPC;
      pk.pack_array(static_cast<uint32_t>(blobs.size()));
      for (const auto& bname : blobs) {
        auto size_future = WRP_CTE_CLIENT->AsyncGetBlobSize(tag_id, bname);
        co_await size_future;
        auto size = size_future->size_;
        if (size == 0) { pk.pack("{}"); continue; }

        auto shm = ipc->AllocateBuffer(size);
        if (shm.IsNull()) { pk.pack("{}"); continue; }
        auto get_future = WRP_CTE_CLIENT->AsyncGetBlob(
            tag_id, bname, 0, size, 0, hipc::ShmPtr<>(shm.shm_));
        co_await get_future;
        pk.pack(std::string(reinterpret_cast<char*>(shm.ptr_), size));
        ipc->FreeBuffer(shm);
      }
    } catch (...) {
      pk.pack_array(0);
    }

    task->results_[container_id_] = std::string(sbuf.data(), sbuf.size());

  } else if (query.rfind("get_interaction://", 0) == 0) {
    // get_interaction://session_id/seq_id
    std::string body = query.substr(18);
    auto slash = body.find('/');
    if (slash != std::string::npos) {
      std::string session_id = body.substr(0, slash);
      uint64_t seq_id = std::stoull(body.substr(slash + 1));
      std::string tag_name = BuildTagName(session_id);
      std::string blob_name = FormatBlobName(seq_id);

      msgpack::sbuffer sbuf;
      msgpack::packer<msgpack::sbuffer> pk(sbuf);

      try {
        auto tag_future = WRP_CTE_CLIENT->AsyncGetOrCreateTag(tag_name);
        co_await tag_future;
        auto tag_id = tag_future->tag_id_;

        auto size_future = WRP_CTE_CLIENT->AsyncGetBlobSize(tag_id, blob_name);
        co_await size_future;
        auto size = size_future->size_;

        if (size > 0) {
          auto *ipc = CHI_IPC;
          auto shm = ipc->AllocateBuffer(size);
          if (shm.IsNull()) { pk.pack("{}"); } else {
          auto get_future = WRP_CTE_CLIENT->AsyncGetBlob(
              tag_id, blob_name, 0, size, 0, hipc::ShmPtr<>(shm.shm_));
          co_await get_future;
          pk.pack(std::string(reinterpret_cast<char*>(shm.ptr_), size));
          ipc->FreeBuffer(shm); }
        } else {
          pk.pack("{}");
        }
      } catch (...) {
        pk.pack("{}");
      }

      task->results_[container_id_] = std::string(sbuf.data(), sbuf.size());
    }
  }

  (void)rctx;
  co_return;
}

chi::TaskResume Runtime::Destroy(hipc::FullPtr<DestroyTask> task,
                                 chi::RunContext& rctx) {
  HLOG(kInfo, "Conversation Tracker ChiMod destroyed");
  untangler_client_.reset();
  (void)task;
  (void)rctx;
  co_return;
}

chi::u64 Runtime::GetWorkRemaining() const { return 0; }

}  // namespace dt_provenance::tracker

CHI_TASK_CC(dt_provenance::tracker::Runtime)
