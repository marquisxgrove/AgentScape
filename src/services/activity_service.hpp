#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "app/event_bus.hpp"
#include "core/result.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "domain/chat_log.hpp"

namespace as::services {

enum class ActivityKind { QuestLogCreated, LevelUp, AgentApproved, NoteCreated };

struct ActivityEvent {
  std::int64_t id = 0;
  ActivityKind kind = ActivityKind::QuestLogCreated;
  std::string text;  // pre-rendered game-voice text
  std::chrono::sys_seconds at;
};

// Writes and reads the Tavern activity feed. Publishes ActivityPosted on the EventBus AFTER commit,
// so the Chatbox updates without a reload (game-shell Req 5.3/5.4).
class ActivityService {
 public:
  ActivityService(db::Database& db, const core::Clock& clock, app::EventBus& bus)
      : db_(db), clock_(clock), bus_(bus) {}

  // Records an event and returns it. Publishes ActivityPosted after the transaction commits.
  Result<ActivityEvent> post(ActivityKind kind, const std::string& text);
  // For services that already hold a transaction: writes the row but neither commits nor publishes. Call
  // announce() with the result AFTER the surrounding transaction commits.
  Result<ActivityEvent> record(ActivityKind kind, const std::string& text);
  void announce(const ActivityEvent& event);

  // The most recent `limit` events, oldest-first (ready to feed the Chatbox).
  Result<std::vector<ActivityEvent>> latest(int limit = 20);

 private:
  db::Database& db_;
  const core::Clock& clock_;
  app::EventBus& bus_;
};

// Shared chat-message helpers used by the Chatbox.
domain::ChatMessage toChatMessage(const ActivityEvent& event);
std::string_view activityKindToDb(ActivityKind kind);

}  // namespace as::services
