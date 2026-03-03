#include "dt_provenance/tracker/tracker_runtime.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace dt_provenance::tracker {

using json = nlohmann::ordered_json;

std::string Runtime::FormatBlobName(uint64_t seq_id) {
  // Zero-padded 10-digit blob name for lexicographic ordering
  char buf[16];
  snprintf(buf, sizeof(buf), "%010lu", static_cast<unsigned long>(seq_id));
  return std::string(buf);
}

std::string Runtime::BuildTagName(const std::string& session_id) {
  return "Agentic_session_" + session_id;
}

chi::TaskResume Runtime::Create(hipc::FullPtr<CreateTask> task,
                                chi::RunContext& rctx) {
  HLOG(kInfo, "Conversation Tracker ChiMod created");
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

  // 6. Store in memory (Phase 5 replaces with CTE)
  {
    std::lock_guard<std::mutex> lock(store_mutex_);
    store_[tag_name][blob_name] = interaction.dump();
  }

  HLOG(kDebug, "Stored interaction seq={} tag={} blob={}", seq_id, tag_name,
       blob_name);

  task->sequence_id_ = seq_id;
  co_return;
}

chi::TaskResume Runtime::QuerySession(hipc::FullPtr<QuerySessionTask> task,
                                      chi::RunContext& rctx) {
  std::string session_id(task->session_id_.str());
  std::string tag_name = BuildTagName(session_id);

  json result = json::array();
  {
    std::lock_guard<std::mutex> lock(store_mutex_);
    auto it = store_.find(tag_name);
    if (it != store_.end()) {
      // std::map is already ordered by blob_name (lexicographic = chronological)
      for (const auto& [blob_name, data] : it->second) {
        try {
          result.push_back(json::parse(data));
        } catch (const json::parse_error&) {}
      }
    }
  }

  task->interactions_json_ = result.dump();
  co_return;
}

chi::TaskResume Runtime::ListSessions(hipc::FullPtr<ListSessionsTask> task,
                                      chi::RunContext& rctx) {
  json result = json::array();
  {
    std::lock_guard<std::mutex> lock(store_mutex_);
    for (const auto& [tag_name, blobs] : store_) {
      // Strip "Agentic_session_" prefix
      std::string session_id = tag_name;
      const std::string prefix = "Agentic_session_";
      if (session_id.starts_with(prefix)) {
        session_id = session_id.substr(prefix.size());
      }
      result.push_back(json{
          {"session_id", session_id},
          {"count", blobs.size()},
          {"tag_name", tag_name}});
    }
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

  {
    std::lock_guard<std::mutex> lock(store_mutex_);
    auto tag_it = store_.find(tag_name);
    if (tag_it != store_.end()) {
      auto blob_it = tag_it->second.find(blob_name);
      if (blob_it != tag_it->second.end()) {
        task->interaction_json_ = blob_it->second;
        co_return;
      }
    }
  }

  task->interaction_json_ = "{}";
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
  HLOG(kInfo, "Conversation Tracker ChiMod destroyed");
  (void)task;
  (void)rctx;
  co_return;
}

chi::u64 Runtime::GetWorkRemaining() const { return 0; }

}  // namespace dt_provenance::tracker
