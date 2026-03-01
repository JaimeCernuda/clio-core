/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mcp_gateway/session_manager.h"

#include <mutex>
#include <random>
#include <sstream>
#include <iomanip>

namespace mchips::mcp_gateway {

SessionManager::SessionManager(std::chrono::seconds timeout)
    : timeout_(timeout) {}

/// Generate a random UUID-like session ID.
std::string SessionManager::GenerateSessionId() {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<uint64_t> dist;

  uint64_t a = dist(rng);
  uint64_t b = dist(rng);

  // Format as UUID-like string: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  oss << std::setw(8) << ((a >> 32) & 0xFFFFFFFF) << "-";
  oss << std::setw(4) << ((a >> 16) & 0xFFFF) << "-";
  oss << std::setw(4) << (a & 0xFFFF) << "-";
  oss << std::setw(4) << ((b >> 48) & 0xFFFF) << "-";
  oss << std::setw(12) << (b & 0xFFFFFFFFFFFFULL);
  return oss.str();
}

/// Create a new session and return its ID.
std::string SessionManager::CreateSession(
    const std::string& protocol_version) {
  auto session_id = GenerateSessionId();
  auto now = std::chrono::steady_clock::now();

  McpSessionState state;
  state.session_id = session_id;
  state.protocol_version = protocol_version;
  state.created_at = now;
  state.last_active = now;

  std::unique_lock lock(mutex_);
  sessions_[session_id] = std::move(state);
  return session_id;
}

/// Validate a session: check it exists and hasn't expired.
bool SessionManager::ValidateSession(const std::string& session_id) {
  std::shared_lock lock(mutex_);
  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return false;
  }

  auto now = std::chrono::steady_clock::now();
  if ((now - it->second.last_active) > timeout_) {
    return false;
  }

  // Update last_active (need exclusive lock)
  lock.unlock();
  std::unique_lock write_lock(mutex_);
  auto it2 = sessions_.find(session_id);
  if (it2 != sessions_.end()) {
    it2->second.last_active = now;
  }
  return true;
}

/// Destroy a session immediately.
void SessionManager::DestroySession(const std::string& session_id) {
  std::unique_lock lock(mutex_);
  sessions_.erase(session_id);
}

/// Remove all expired sessions.
void SessionManager::CleanupExpired() {
  auto now = std::chrono::steady_clock::now();
  std::unique_lock lock(mutex_);
  for (auto it = sessions_.begin(); it != sessions_.end();) {
    if ((now - it->second.last_active) > timeout_) {
      it = sessions_.erase(it);
    } else {
      ++it;
    }
  }
}

/// Return the number of active sessions.
size_t SessionManager::Count() const {
  std::shared_lock lock(mutex_);
  return sessions_.size();
}

}  // namespace mchips::mcp_gateway
