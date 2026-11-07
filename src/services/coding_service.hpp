#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "app/event_bus.hpp"
#include "core/result.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "services/activity_service.hpp"
#include "services/xp_service.hpp"

namespace as::services {

// One stretch of coding (coding-sessions Requirement 1). agentId is empty when coding solo.
struct CodingSession {
  std::string id;
  std::string userId;
  std::string agentId;
  std::string agentName;
  std::string agentSlug;
  std::chrono::sys_seconds startedAt;
  std::chrono::sys_seconds settledUntil;    // counted and paid up to here
  std::optional<std::chrono::sys_seconds> endedAt;

  bool solo() const { return agentId.empty(); }
};

// What one settle did.
struct CodingSettle {
  int xp = 0;                               // paid by this settle
  std::chrono::seconds newlyCovered{0};     // session time this settle moved past (counted or over the daily cap)
  std::chrono::sys_seconds from{};          // the settled stretch, [from, to)
  std::chrono::sys_seconds to{};
  bool ended = false;                       // the session reached its 4-hour limit and was ended
  std::optional<CodingSession> session;     // the session settled, if any
};

// A finished session, for the summary.
struct CodingSummary {
  CodingSession session;
  std::chrono::seconds duration{0};         // started -> ended (at most the 4-hour limit)
  int xp = 0;                               // everything the session earned
};

// Passive XP for time spent coding (coding-sessions Requirement 2). Every method is one transaction; XP events are
// published after it commits. Self-contained, like QuestService, so it can live in the App's service bundle.
class CodingService {
 public:
  CodingService(db::Database& db, const core::Clock& clock, core::Rng& rng, app::EventBus& bus)
      : db_(db), clock_(clock), rng_(rng), bus_(bus), activity_(db, clock, bus), xp_(db, clock, rng, activity_) {}
  // xp_ refers to activity_, a sibling member, so a defaulted move would leave it pointing into the moved-from
  // object (the App builds its service bundle as a temporary and moves it). Rebuild both instead.
  CodingService(CodingService&& other) noexcept : CodingService(other.db_, other.clock_, other.rng_, other.bus_) {}
  CodingService(const CodingService&) = delete;
  CodingService& operator=(const CodingService&) = delete;
  CodingService& operator=(CodingService&&) = delete;

  // Starts a session, solo (empty slug) or with an approved familiar. Refused while another session runs.
  Result<CodingSession> start(const std::string& userId, const std::string& agentSlug);

  // Credits the running session up to now (Requirement 2.3). With no running session it does nothing. Reaching the
  // 4-hour limit ends the session (Requirement 1.5).
  Result<CodingSettle> settle(const std::string& userId);

  // Settles and ends the running session. Refused if none runs.
  Result<CodingSummary> stop(const std::string& userId);

  Result<std::optional<CodingSession>> running(const std::string& userId);

  // XP the session has earned so far (all of its days).
  Result<int> sessionXp(const std::string& sessionId);

  // Coding XP counted for the user's current UTC day, across sessions (for "Today: N / 480 XP").
  Result<int> todayXp(const std::string& userId);

 private:
  db::Database& db_;
  const core::Clock& clock_;
  core::Rng& rng_;
  app::EventBus& bus_;
  ActivityService activity_;
  XpService xp_;
};

}  // namespace as::services
