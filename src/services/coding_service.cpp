#include "services/coding_service.hpp"

#include <vector>

#include "core/civil_time.hpp"
#include "core/ids.hpp"
#include "domain/coding_session.hpp"

namespace as::services {

namespace {
using std::chrono::seconds;
using std::chrono::sys_days;
using std::chrono::sys_seconds;

constexpr const char* SESSION_SELECT =
    "SELECT s.id, s.user_id, COALESCE(s.agent_id, ''), COALESCE(a.name, ''), COALESCE(a.slug, ''), s.started_at, "
    "s.settled_until, s.ended_at FROM coding_session s LEFT JOIN agent a ON a.id = s.agent_id ";

sys_seconds timeFrom(const std::string& s) { return core::parseIso8601(s).value_or(sys_seconds{}); }

CodingSession readSession(const db::Statement& st) {
  CodingSession s;
  s.id = st.columnText(0);
  s.userId = st.columnText(1);
  s.agentId = st.columnText(2);
  s.agentName = st.columnText(3);
  s.agentSlug = st.columnText(4);
  s.startedAt = timeFrom(st.columnText(5));
  s.settledUntil = timeFrom(st.columnText(6));
  if (auto ended = st.columnOptText(7)) s.endedAt = timeFrom(*ended);
  return s;
}

std::optional<CodingSession> findRunning(db::Database& db, const std::string& userId) {
  auto st = db.prepare(std::string(SESSION_SELECT) + "WHERE s.user_id = ? AND s.ended_at IS NULL");
  st.bind(1, userId);
  if (!st.step()) return std::nullopt;
  return readSession(st);
}

seconds usedOn(db::Database& db, const std::string& userId, sys_days day) {
  auto st = db.prepare("SELECT COALESCE(SUM(seconds), 0) FROM coding_session_day WHERE user_id = ? AND day = ?");
  st.bind(1, userId).bind(2, domain::formatUtcDay(day));
  st.step();
  return seconds{st.columnInt(0)};
}

int sessionXpIn(db::Database& db, const std::string& sessionId) {
  auto st = db.prepare("SELECT COALESCE(SUM(amount), 0) FROM xp_entry WHERE reason = 'CODING' AND substr(ref_id, 1, ?) = ?");
  const std::string prefix = sessionId + ":";
  st.bind(1, static_cast<std::int64_t>(prefix.size())).bind(2, prefix);
  st.step();
  return static_cast<int>(st.columnInt(0));
}

// Credits [settledUntil, countedUntil(now)) of the running session inside the caller's transaction: per-day seconds,
// the growing XP entries, and the new settled_until. Ends the session at its 4-hour limit.
struct SettleWork {
  CodingSettle result;
  std::vector<XpGrant> grants;
};

Result<SettleWork> settleIn(db::Database& db, XpService& xp, const std::string& userId, sys_seconds now) {
  SettleWork work;
  auto running = findRunning(db, userId);
  if (!running) return work;
  CodingSession& s = *running;
  const sys_seconds until = domain::codingCountedUntil(s.startedAt, now);
  work.result.from = s.settledUntil;
  work.result.to = s.settledUntil;
  if (until > s.settledUntil) {
    // What each touched day has already counted, across all of the user's sessions.
    domain::CodingDays used;
    for (sys_days day = std::chrono::floor<std::chrono::days>(s.settledUntil);
         sys_seconds{day} < until; day += std::chrono::days{1}) {
      if (const auto u = usedOn(db, userId, day); u > seconds{0}) used[day] = u;
    }
    const auto added = domain::creditCodingInterval(used, s.settledUntil, until);
    for (const auto& [day, secs] : added) {
      const std::string dayText = domain::formatUtcDay(day);
      db.prepare("INSERT INTO coding_session_day(session_id, user_id, day, seconds) VALUES (?, ?, ?, ?) "
                 "ON CONFLICT(session_id, day) DO UPDATE SET seconds = seconds + excluded.seconds")
          .bind(1, s.id)
          .bind(2, userId)
          .bind(3, dayText)
          .bind(4, static_cast<std::int64_t>(secs.count()))
          .run();
      auto total = db.prepare("SELECT seconds FROM coding_session_day WHERE session_id = ? AND day = ?");
      total.bind(1, s.id).bind(2, dayText);
      total.step();
      const int amount = domain::codingXpFor(seconds{total.columnInt(0)});
      if (amount <= 0) continue;
      auto grant = xp.raiseTo(userId, domain::XpReason::Coding, s.id + ":" + dayText, amount);
      if (!grant) return tl::unexpected(grant.error());
      work.result.xp += grant->amount;
      work.grants.push_back(std::move(*grant));
    }
    db.prepare("UPDATE coding_session SET settled_until = ? WHERE id = ?")
        .bind(1, core::formatIso8601(until))
        .bind(2, s.id)
        .run();
    work.result.newlyCovered = until - s.settledUntil;
    work.result.to = until;
    s.settledUntil = until;
  }
  if (until >= s.startedAt + domain::CODING_SESSION_MAX) {   // the 4-hour limit (Requirement 1.5)
    db.prepare("UPDATE coding_session SET ended_at = ? WHERE id = ?").bind(1, core::formatIso8601(until)).bind(2, s.id).run();
    s.endedAt = until;
    work.result.ended = true;
  }
  work.result.session = s;
  return work;
}
}  // namespace

Result<CodingSession> CodingService::start(const std::string& userId, const std::string& agentSlug) {
  auto r = db::guarded([&]() -> Result<CodingSession> {
    const auto now = clock_.now();
    db::Transaction tx{db_};
    {
      auto user = db_.prepare("SELECT 1 FROM user WHERE id = ? AND kind != 'NPC'");
      user.bind(1, userId);
      if (!user.step()) return makeError(GameErrorCode::NotFound, "You can't do that right now.");
    }
    // A session that ran out of time while nobody looked is closed first, so it never blocks a new one.
    auto settled = settleIn(db_, xp_, userId, now);
    if (!settled) return tl::unexpected(settled.error());
    if (findRunning(db_, userId)) return makeError(GameErrorCode::Forbidden, "You're already coding. Stop that session first.");

    CodingSession s;
    s.id = core::newUlid(now, rng_);
    s.userId = userId;
    if (!agentSlug.empty()) {
      auto agent = db_.prepare("SELECT id, name, slug FROM agent WHERE slug = ? AND status = 'APPROVED'");
      agent.bind(1, agentSlug);
      if (!agent.step()) return makeError(GameErrorCode::NotFound, "That familiar isn't in the Codex.");
      s.agentId = agent.columnText(0);
      s.agentName = agent.columnText(1);
      s.agentSlug = agent.columnText(2);
    }
    s.startedAt = now;
    s.settledUntil = now;
    db_.prepare("INSERT INTO coding_session(id, user_id, agent_id, started_at, settled_until) VALUES (?, ?, ?, ?, ?)")
        .bind(1, s.id)
        .bind(2, userId)
        .bind(3, s.agentId.empty() ? std::optional<std::string>{} : std::optional<std::string>{s.agentId})
        .bind(4, core::formatIso8601(now))
        .bind(5, core::formatIso8601(now))
        .run();
    tx.commit();
    for (const auto& g : settled->grants) publishGrant(bus_, activity_, userId, g);
    return s;
  });
  if (!r) return tl::unexpected(r.error());
  return *r;
}

Result<CodingSettle> CodingService::settle(const std::string& userId) {
  auto r = db::guarded([&]() -> Result<CodingSettle> {
    db::Transaction tx{db_};
    auto work = settleIn(db_, xp_, userId, clock_.now());
    if (!work) return tl::unexpected(work.error());
    tx.commit();
    for (const auto& g : work->grants) publishGrant(bus_, activity_, userId, g);
    return work->result;
  });
  if (!r) return tl::unexpected(r.error());
  return *r;
}

Result<CodingSummary> CodingService::stop(const std::string& userId) {
  auto r = db::guarded([&]() -> Result<CodingSummary> {
    const auto now = clock_.now();
    db::Transaction tx{db_};
    auto work = settleIn(db_, xp_, userId, now);
    if (!work) return tl::unexpected(work.error());
    if (!work->result.session) return makeError(GameErrorCode::NotFound, "You aren't coding right now.");
    CodingSession s = *work->result.session;
    if (!s.endedAt) {
      const sys_seconds end = domain::codingCountedUntil(s.startedAt, now);
      db_.prepare("UPDATE coding_session SET ended_at = ? WHERE id = ?").bind(1, core::formatIso8601(end)).bind(2, s.id).run();
      s.endedAt = end;
    }
    CodingSummary summary;
    summary.session = s;
    summary.duration = *s.endedAt - s.startedAt;
    summary.xp = sessionXpIn(db_, s.id);
    tx.commit();
    for (const auto& g : work->grants) publishGrant(bus_, activity_, userId, g);
    return summary;
  });
  if (!r) return tl::unexpected(r.error());
  return *r;
}

Result<std::optional<CodingSession>> CodingService::running(const std::string& userId) {
  return db::guarded([&] { return findRunning(db_, userId); });
}

Result<int> CodingService::sessionXp(const std::string& sessionId) {
  return db::guarded([&] { return sessionXpIn(db_, sessionId); });
}

Result<int> CodingService::todayXp(const std::string& userId) {
  return db::guarded([&]() -> int {
    // Per session-day entries are whole minutes of that session's seconds, so sum those rather than the seconds.
    auto st = db_.prepare("SELECT seconds FROM coding_session_day WHERE user_id = ? AND day = ?");
    st.bind(1, userId).bind(2, domain::formatUtcDay(std::chrono::floor<std::chrono::days>(clock_.now())));
    int xp = 0;
    while (st.step()) xp += domain::codingXpFor(seconds{st.columnInt(0)});
    return xp;
  });
}

}  // namespace as::services
