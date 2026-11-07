#include "services/xp_service.hpp"

#include "core/civil_time.hpp"
#include "core/ids.hpp"

namespace as::services {

namespace {
Result<void> needTransaction(db::Database& db) {
  if (db.inTransaction()) return {};
  return makeError(GameErrorCode::Internal, "Something went wrong. Nothing was saved.", "XpService needs the caller's transaction");
}

std::int64_t readTotal(db::Database& db, const std::string& userId) {
  auto st = db.prepare("SELECT total_xp FROM user WHERE id = ?");
  st.bind(1, userId);
  return st.step() ? st.columnInt(0) : 0;
}

// The UTC day containing `now`, as [start, end) in the ISO form stored in created_at.
std::pair<std::string, std::string> utcDay(std::chrono::sys_seconds now) {
  const auto start = std::chrono::floor<std::chrono::days>(now);
  return {core::formatIso8601(start), core::formatIso8601(start + std::chrono::days{1})};
}
}  // namespace

Result<XpGrant> XpService::award(const std::string& userId, domain::XpReason reason, const std::string& refId, int amount) {
  if (auto ok = needTransaction(db_); !ok) return tl::unexpected(ok.error());
  if (amount <= 0) return makeError(GameErrorCode::InvalidInput, "You can't do that right now.", "XP amount must be positive");
  return db::guarded([&]() -> Result<XpGrant> {
    const auto now = clock_.now();
    auto user = db_.prepare("SELECT name FROM user WHERE id = ?");
    user.bind(1, userId);
    if (!user.step()) return makeError(GameErrorCode::NotFound, "That is no longer there.");
    const std::string userName = user.columnText(0);

    db_.prepare("INSERT INTO xp_entry(id, user_id, reason, ref_id, amount, created_at) VALUES (?, ?, ?, ?, ?, ?) "
                "ON CONFLICT(user_id, reason, ref_id) DO NOTHING")
        .bind(1, core::newUlid(now, rng_))
        .bind(2, userId)
        .bind(3, domain::xpReasonToDb(reason))
        .bind(4, refId)
        .bind(5, amount)
        .bind(6, core::formatIso8601(now))
        .run();
    if (db_.changes() == 0) {   // already awarded: a retry pays nothing
      XpGrant grant;
      grant.totalXp = readTotal(db_, userId);
      grant.levelBefore = grant.levelAfter = domain::levelForXp(grant.totalXp);
      return grant;
    }
    return applyGain(userId, userName, amount);
  }).and_then([](Result<XpGrant> r) { return r; });
}

Result<XpGrant> XpService::raiseTo(const std::string& userId, domain::XpReason reason, const std::string& refId, int amount) {
  if (auto ok = needTransaction(db_); !ok) return tl::unexpected(ok.error());
  return db::guarded([&]() -> Result<XpGrant> {
    const auto now = clock_.now();
    auto user = db_.prepare("SELECT name FROM user WHERE id = ?");
    user.bind(1, userId);
    if (!user.step()) return makeError(GameErrorCode::NotFound, "That is no longer there.");
    const std::string userName = user.columnText(0);

    auto existing = db_.prepare("SELECT amount FROM xp_entry WHERE user_id = ? AND reason = ? AND ref_id = ?");
    existing.bind(1, userId).bind(2, domain::xpReasonToDb(reason)).bind(3, refId);
    const bool found = existing.step();
    const std::int64_t have = found ? existing.columnInt(0) : 0;
    if (amount <= have) {   // nothing new: never lowers an entry
      XpGrant grant;
      grant.totalXp = readTotal(db_, userId);
      grant.levelBefore = grant.levelAfter = domain::levelForXp(grant.totalXp);
      return grant;
    }
    if (found) {
      db_.prepare("UPDATE xp_entry SET amount = ? WHERE user_id = ? AND reason = ? AND ref_id = ?")
          .bind(1, amount)
          .bind(2, userId)
          .bind(3, domain::xpReasonToDb(reason))
          .bind(4, refId)
          .run();
    } else {
      db_.prepare("INSERT INTO xp_entry(id, user_id, reason, ref_id, amount, created_at) VALUES (?, ?, ?, ?, ?, ?)")
          .bind(1, core::newUlid(now, rng_))
          .bind(2, userId)
          .bind(3, domain::xpReasonToDb(reason))
          .bind(4, refId)
          .bind(5, amount)
          .bind(6, core::formatIso8601(now))
          .run();
    }
    return applyGain(userId, userName, static_cast<int>(amount - have));
  }).and_then([](Result<XpGrant> r) { return r; });
}

// Adds `gain` (already written to the ledger) to total_xp, and on a level change grants the unlocks and records the
// milestones. Runs inside the caller's transaction and db::guarded scope.
Result<XpGrant> XpService::applyGain(const std::string& userId, const std::string& userName, int gain) {
  const auto now = clock_.now();
  XpGrant grant;
  const std::int64_t before = readTotal(db_, userId);
  grant.levelBefore = domain::levelForXp(before);
  db_.prepare("UPDATE user SET total_xp = total_xp + ? WHERE id = ?").bind(1, gain).bind(2, userId).run();
  grant.awarded = true;
  grant.amount = gain;
  grant.totalXp = before + gain;
  grant.levelAfter = domain::levelForXp(grant.totalXp);
  if (grant.levelAfter > grant.levelBefore) {
    // Unlocks: everything the new level reaches that the user does not own yet. Idempotent by construction.
    domain::OwnedSet owned = domain::defaultOwnedSet();
    auto have = db_.prepare("SELECT item_id FROM item_unlock WHERE user_id = ?");
    have.bind(1, userId);
    while (have.step()) owned.insert(have.columnText(0));
    for (const auto& id : domain::newUnlocks(owned, grant.levelAfter)) {
      db_.prepare("INSERT INTO item_unlock(user_id, item_id, source, unlocked_at) VALUES (?, ?, 'LEVEL', ?) "
                  "ON CONFLICT(user_id, item_id) DO NOTHING")
          .bind(1, userId)
          .bind(2, id)
          .bind(3, core::formatIso8601(now))
          .run();
      if (db_.changes() == 1) grant.unlocked.push_back(id);
    }
    // Announce every milestone crossed (a multiple of 10, or 99), in order.
    for (int level = grant.levelBefore + 1; level <= grant.levelAfter; ++level) {
      if (level % 10 != 0 && level != domain::MAX_QUESTING_LEVEL) continue;
      auto event = activity_.record(ActivityKind::LevelUp, userName + " reached Questing level " + std::to_string(level) + "!");
      if (!event) return tl::unexpected(event.error());
      grant.activity.push_back(std::move(*event));
    }
  }
  return grant;
}

Result<std::vector<std::string>> XpService::revokeForQuestLog(const std::string& questLogId) {
  if (auto ok = needTransaction(db_); !ok) return tl::unexpected(ok.error());
  return db::guarded([&]() -> std::vector<std::string> {
    // QUEST_LOG rows reference the id itself; HELPFUL_RECEIVED rows reference '<id>:<voter>'.
    std::vector<std::string> users;
    auto find = db_.prepare("SELECT DISTINCT user_id FROM xp_entry WHERE (reason = 'QUEST_LOG' AND ref_id = ?) "
                            "OR (reason = 'HELPFUL_RECEIVED' AND substr(ref_id, 1, ?) = ?)");
    const std::string prefix = questLogId + ":";
    find.bind(1, questLogId).bind(2, static_cast<std::int64_t>(prefix.size())).bind(3, prefix);
    while (find.step()) users.push_back(find.columnText(0));

    auto del = db_.prepare("DELETE FROM xp_entry WHERE (reason = 'QUEST_LOG' AND ref_id = ?) "
                           "OR (reason = 'HELPFUL_RECEIVED' AND substr(ref_id, 1, ?) = ?)");
    del.bind(1, questLogId).bind(2, static_cast<std::int64_t>(prefix.size())).bind(3, prefix);
    del.run();
    for (const auto& u : users) {
      db_.prepare("UPDATE user SET total_xp = (SELECT COALESCE(SUM(amount), 0) FROM xp_entry WHERE user_id = ?) WHERE id = ?")
          .bind(1, u)
          .bind(2, u)
          .run();
    }
    return users;
  });
}

Result<bool> XpService::revoke(const std::string& userId, domain::XpReason reason, const std::string& refId) {
  if (auto ok = needTransaction(db_); !ok) return tl::unexpected(ok.error());
  return db::guarded([&]() -> bool {
    db_.prepare("DELETE FROM xp_entry WHERE user_id = ? AND reason = ? AND ref_id = ?")
        .bind(1, userId)
        .bind(2, domain::xpReasonToDb(reason))
        .bind(3, refId)
        .run();
    const bool removed = db_.changes() == 1;
    if (removed) {
      db_.prepare("UPDATE user SET total_xp = (SELECT COALESCE(SUM(amount), 0) FROM xp_entry WHERE user_id = ?) WHERE id = ?")
          .bind(1, userId)
          .bind(2, userId)
          .run();
    }
    return removed;
  });
}

Result<int> XpService::todaysXp(const std::string& userId, domain::XpReason reason) {
  return db::guarded([&]() -> int {
    const auto [start, end] = utcDay(clock_.now());
    auto st = db_.prepare("SELECT COALESCE(SUM(amount), 0) FROM xp_entry WHERE user_id = ? AND reason = ? "
                          "AND created_at >= ? AND created_at < ?");
    st.bind(1, userId).bind(2, domain::xpReasonToDb(reason)).bind(3, start).bind(4, end);
    st.step();
    return static_cast<int>(st.columnInt(0));
  });
}

Result<std::int64_t> XpService::totalXp(const std::string& userId) {
  return db::guarded([&]() -> std::int64_t { return readTotal(db_, userId); });
}

Result<int> XpService::repairTotals() {
  return db::guarded([&]() -> int {
    db::Transaction tx{db_};
    auto wrong = db_.prepare("SELECT COUNT(*) FROM user u WHERE u.total_xp <> (SELECT COALESCE(SUM(amount), 0) FROM xp_entry WHERE user_id = u.id)");
    wrong.step();
    const int count = static_cast<int>(wrong.columnInt(0));
    db_.prepare("UPDATE user SET total_xp = (SELECT COALESCE(SUM(amount), 0) FROM xp_entry WHERE user_id = user.id)").run();
    tx.commit();
    return count;
  });
}

void publishGrant(app::EventBus& bus, ActivityService& activity, const std::string& userId, const XpGrant& grant) {
  if (!grant.awarded) return;
  bus.publish(app::XpChanged{userId, grant.totalXp, grant.levelBefore, grant.levelAfter});
  for (const auto& item : grant.unlocked) bus.publish(app::ItemUnlocked{userId, item});
  for (const auto& e : grant.activity) activity.announce(e);
}

}  // namespace as::services
