#include "services/activity_service.hpp"
#include <algorithm>

#include "core/civil_time.hpp"

namespace as::services {

namespace {
ActivityKind kindFromDb(const std::string& s) {
  if (s == "LEVEL_UP") return ActivityKind::LevelUp;
  if (s == "AGENT_APPROVED") return ActivityKind::AgentApproved;
  if (s == "NOTE_CREATED") return ActivityKind::NoteCreated;
  return ActivityKind::QuestLogCreated;
}
}  // namespace

std::string_view activityKindToDb(ActivityKind kind) {
  switch (kind) {
    case ActivityKind::LevelUp: return "LEVEL_UP";
    case ActivityKind::AgentApproved: return "AGENT_APPROVED";
    case ActivityKind::NoteCreated: return "NOTE_CREATED";
    case ActivityKind::QuestLogCreated: return "QUEST_LOG_CREATED";
  }
  return "QUEST_LOG_CREATED";
}

domain::ChatMessage toChatMessage(const ActivityEvent& event) {
  // Activity rows are the community feed, shown on the Tavern channel. The id is prefixed so it
  // never collides with a GameMessage id.
  return {"act-" + std::to_string(event.id), event.at, domain::Channel::Tavern, event.text};
}

void ActivityService::announce(const ActivityEvent& event) { bus_.publish(app::ActivityPosted{toChatMessage(event)}); }

Result<ActivityEvent> ActivityService::record(ActivityKind kind, const std::string& text) {
  if (!db_.inTransaction()) {
    return makeError(GameErrorCode::Internal, "Something went wrong. Nothing was saved.", "record() needs the caller's transaction");
  }
  return db::guarded([&]() -> ActivityEvent {
    const auto now = clock_.now();
    db_.prepare("INSERT INTO activity_event(kind, text, created_at) VALUES (?, ?, ?)")
        .bind(1, activityKindToDb(kind))
        .bind(2, text)
        .bind(3, core::formatIso8601(now))
        .run();
    return ActivityEvent{db_.lastInsertRowId(), kind, text, now};
  });
}

Result<ActivityEvent> ActivityService::post(ActivityKind kind, const std::string& text) {
  auto result = db::guarded([&]() -> ActivityEvent {
    const auto now = clock_.now();
    const std::string stamp = core::formatIso8601(now);
    db::Transaction tx{db_};
    db_.prepare("INSERT INTO activity_event(kind, text, created_at) VALUES (?, ?, ?)")
        .bind(1, activityKindToDb(kind))
        .bind(2, text)
        .bind(3, stamp)
        .run();
    const auto id = db_.lastInsertRowId();
    tx.commit();
    return ActivityEvent{id, kind, text, now};
  });
  if (!result) return result;
  bus_.publish(app::ActivityPosted{toChatMessage(*result)});  // after commit
  return result;
}

Result<std::vector<ActivityEvent>> ActivityService::latest(int limit) {
  return db::guarded([&] {
    std::vector<ActivityEvent> events;
    // Newest `limit` by id, then reversed so the caller gets oldest-first.
    auto st = db_.prepare("SELECT id, kind, text, created_at FROM activity_event ORDER BY id DESC LIMIT ?");
    st.bind(1, limit);
    while (st.step()) {
      ActivityEvent e;
      e.id = st.columnInt(0);
      e.kind = kindFromDb(st.columnText(1));
      e.text = st.columnText(2);
      e.at = core::parseIso8601(st.columnText(3)).value_or(std::chrono::sys_seconds{});
      events.push_back(std::move(e));
    }
    std::reverse(events.begin(), events.end());
    return events;
  });
}

}  // namespace as::services
