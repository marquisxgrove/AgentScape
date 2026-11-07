#include "services/quest_service.hpp"

#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>

#include "core/civil_time.hpp"
#include "core/ids.hpp"
#include "domain/appearance_json.hpp"
#include "domain/loadout_json.hpp"
#include "domain/rate_limit.hpp"
#include "domain/snapshot.hpp"
#include "services/codex_store.hpp"

namespace as::services {

namespace {
constexpr const char* MSG_GONE = "That is no longer there.";
constexpr const char* MSG_NOT_ALLOWED = "You can't do that right now.";
constexpr const char* MSG_SAVE_FAILED = "Something went wrong. Your quest scroll was not saved.";

QuestError fail(GameErrorCode code, std::string message, std::string detail = {}) {
  return QuestError{GameError{code, std::move(message), std::move(detail)}, {}, std::nullopt};
}

// A failure from a lower layer. Storage failures get the quest-scroll wording from the design's error table.
QuestError fromGame(GameError e) {
  if (e.code == GameErrorCode::StorageFailed) e.message = MSG_SAVE_FAILED;
  return QuestError{std::move(e), {}, std::nullopt};
}

// Everything a command publishes once its transaction has committed.
struct Pending {
  std::vector<std::string> ratingAgents;
  std::vector<std::pair<std::string, XpGrant>> grants;   // user id, grant
  std::vector<ActivityEvent> activity;
  std::vector<std::pair<std::string, std::int64_t>> totals;   // user id, total XP (after a revoke)
};

domain::RatingSet ratingSetOfRow(const db::Statement& st, int firstCol) {
  // columns: quest_type, overall, ratings, excluded
  domain::RatingSet s;
  s.questType = domain::questTypeFromStorage(st.columnText(firstCol)).value_or(domain::QuestType::Other);
  s.overall = static_cast<int>(st.columnInt(firstCol + 1));
  s.skills = domain::ratingsFromJson(st.columnText(firstCol + 2));
  s.excluded = st.columnInt(firstCol + 3) == 1;
  return s;
}

// Folds one list of deltas into agent_skill_stat, one Quest Type at a time (the table is keyed by it).
Result<void> applyDeltas(db::Database& db, const std::string& agentId, const std::vector<domain::Delta>& deltas) {
  std::map<int, std::vector<RatingDelta>> byType;
  for (const auto& d : deltas) byType[static_cast<int>(d.questType)].push_back({d.skill, d.sumDelta, d.countDelta});
  for (const auto& [type, list] : byType) {
    if (auto r = applyRatingDelta(db, agentId, static_cast<domain::QuestType>(type), list); !r) return r;
  }
  return {};
}

bool isKeeper(db::Database& db, const std::string& userId) {
  auto st = db.prepare("SELECT 1 FROM user WHERE id = ? AND role = 'KEEPER' AND kind <> 'NPC'");
  st.bind(1, userId);
  return st.step();
}

bool canPlay(db::Database& db, const std::string& userId) {
  auto st = db.prepare("SELECT 1 FROM user WHERE id = ? AND kind <> 'NPC'");
  st.bind(1, userId);
  return st.step();
}

void publish(app::EventBus& bus, ActivityService& activity, Pending& p) {
  for (const auto& agentId : p.ratingAgents) bus.publish(app::RatingsChanged{agentId});
  for (const auto& e : p.activity) activity.announce(e);   // same order as they were recorded
  for (const auto& [userId, g] : p.grants) {
    if (!g.awarded) continue;
    bus.publish(app::XpChanged{userId, g.totalXp, g.levelBefore, g.levelAfter});
    for (const auto& item : g.unlocked) bus.publish(app::ItemUnlocked{userId, item});
    for (const auto& e : g.activity) activity.announce(e);
  }
  for (const auto& [userId, total] : p.totals) {
    const int level = domain::levelForXp(total);
    bus.publish(app::XpChanged{userId, total, level, level});
  }
}

constexpr const char* ROW_COLUMNS =
    "q.id, q.author_id, u.name, q.agent_id, a.name, q.agent_version, q.quest_type, q.overall, q.ratings, q.title, q.body, "
    "q.recommend, q.tags, q.snapshot, q.helpful_count, q.hidden, q.excluded, q.created_at, q.updated_at, a.slug";

QuestLogRecord readRecord(const db::Statement& st) {
  QuestLogRecord r;
  r.id = st.columnText(0);
  r.authorId = st.columnText(1);
  r.authorName = st.columnText(2);
  r.input.agentId = st.columnText(3);
  r.agentName = st.columnText(4);
  r.input.version = st.columnText(5);
  r.input.questType = domain::questTypeFromStorage(st.columnText(6)).value_or(domain::QuestType::Other);
  r.input.overall = static_cast<int>(st.columnInt(7));
  r.input.skills = domain::ratingsFromJson(st.columnText(8));
  r.input.title = st.columnText(9);
  r.input.body = st.columnText(10);
  r.input.recommend = st.columnInt(11) == 1;
  domain::tagsFromJson(st.columnText(12), r.input.pros, r.input.cons);
  r.snapshot = st.columnText(13);
  r.helpfulCount = static_cast<int>(st.columnInt(14));
  r.hidden = st.columnInt(15) == 1;
  r.excluded = st.columnInt(16) == 1;
  r.createdAt = core::parseIso8601(st.columnText(17)).value_or(std::chrono::sys_seconds{});
  r.updatedAt = core::parseIso8601(st.columnText(18)).value_or(std::chrono::sys_seconds{});
  r.agentSlug = st.columnText(19);
  return r;
}

std::string snapshotJson(const Adventurer& a) { return domain::snapshotToJson({a.name, a.appearance, a.loadout}); }
}  // namespace

Result<QuestOutcome, QuestError> QuestService::createQuestLog(const std::string& userId, domain::QuestLogInput raw) {
  auto valid = domain::validateQuestLogInput(raw);
  if (!valid) {
    QuestError e = fail(GameErrorCode::InvalidInput, valid.error().empty() ? MSG_NOT_ALLOWED : valid.error().front().message);
    e.fields = valid.error();
    return tl::unexpected(std::move(e));
  }
  const domain::QuestLogInput in = std::move(*valid);

  Pending pending;
  auto result = [&]() -> Result<QuestOutcome, QuestError> {
    try {
      const auto now = clock_.now();
      db::Transaction tx{db_};
      if (!canPlay(db_, userId)) return tl::unexpected(fail(GameErrorCode::Forbidden, MSG_NOT_ALLOWED));

      auto agent = db_.prepare("SELECT name FROM agent WHERE id = ? AND status = 'APPROVED'");
      agent.bind(1, in.agentId);
      if (!agent.step()) return tl::unexpected(fail(GameErrorCode::NotFound, MSG_GONE));
      const std::string agentName = agent.columnText(0);

      auto active = adventurers_.getActive(userId);
      if (!active) return tl::unexpected(fromGame(active.error()));
      if (!*active) return tl::unexpected(fail(GameErrorCode::Forbidden, "You need an Adventurer to go questing."));
      const Adventurer& adventurer = **active;

      // One log per (user, Agent, version): open the existing one for editing instead.
      auto dup = db_.prepare("SELECT id FROM quest_log WHERE author_id = ? AND agent_id = ? AND agent_version = ?");
      dup.bind(1, userId).bind(2, in.agentId).bind(3, in.version);
      if (dup.step()) {
        QuestError e = fail(GameErrorCode::InvalidInput, "You've already logged this quest. Update it instead?");
        e.existingId = dup.columnText(0);
        return tl::unexpected(std::move(e));
      }

      std::vector<std::chrono::sys_seconds> history;
      auto recent = db_.prepare("SELECT created_at FROM quest_log WHERE author_id = ? AND created_at > ?");
      recent.bind(1, userId).bind(2, core::formatIso8601(now - std::chrono::hours{1}));
      while (recent.step()) {
        if (const auto t = core::parseIso8601(recent.columnText(0))) history.push_back(*t);
      }
      if (!domain::isAllowed(history, now, QUEST_LOGS_PER_HOUR, std::chrono::hours{1})) {
        return tl::unexpected(fail(GameErrorCode::RateLimited, "You're doing that too often. Rest a while, adventurer."));
      }

      const std::string id = core::newUlid(now, rng_);
      const std::string stamp = core::formatIso8601(now);
      db_.prepare("INSERT INTO quest_log(id, author_id, agent_id, agent_version, quest_type, overall, ratings, title, body, "
                  "recommend, tags, snapshot, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
          .bind(1, id)
          .bind(2, userId)
          .bind(3, in.agentId)
          .bind(4, in.version)
          .bind(5, domain::storageName(in.questType))
          .bind(6, in.overall)
          .bind(7, domain::ratingsToJson(in.skills))
          .bind(8, in.title)
          .bind(9, in.body)
          .bind(10, in.recommend ? 1 : 0)
          .bind(11, domain::tagsToJson(in.pros, in.cons))
          .bind(12, snapshotJson(adventurer))
          .bind(13, stamp)
          .bind(14, stamp)
          .run();

      if (auto r = applyDeltas(db_, in.agentId, domain::ratingDeltas(std::nullopt, domain::ratingSetOf(in))); !r) {
        return tl::unexpected(fromGame(r.error()));
      }
      // The quest is recorded before the XP, so a level-up announcement lands after "completed a quest".
      auto event = activity_.record(ActivityKind::QuestLogCreated,
                                    adventurer.name + " completed a " + domain::displayName(in.questType) + " quest with " +
                                        agentName + " (" + std::to_string(in.overall) + "/5).");
      if (!event) return tl::unexpected(fromGame(event.error()));
      auto grant = xp_.award(userId, domain::XpReason::QuestLog, id, domain::XP_REWARDS.questLog);
      if (!grant) return tl::unexpected(fromGame(grant.error()));

      tx.commit();
      pending.ratingAgents.push_back(in.agentId);
      pending.activity.push_back(std::move(*event));
      QuestOutcome out{id, grant->amount, grant->levelBefore, grant->levelAfter, grant->unlocked};
      pending.grants.emplace_back(userId, std::move(*grant));
      return out;
    } catch (const std::exception& e) {
      return tl::unexpected(fail(GameErrorCode::StorageFailed, MSG_SAVE_FAILED, e.what()));
    }
  }();
  if (result) publish(bus_, activity_, pending);
  return result;
}

Result<void, QuestError> QuestService::updateQuestLog(const std::string& userId, const std::string& questLogId,
                                                      domain::QuestLogInput raw) {
  auto valid = domain::validateQuestLogInput(raw);
  if (!valid) {
    QuestError e = fail(GameErrorCode::InvalidInput, valid.error().empty() ? MSG_NOT_ALLOWED : valid.error().front().message);
    e.fields = valid.error();
    return tl::unexpected(std::move(e));
  }
  const domain::QuestLogInput in = std::move(*valid);

  Pending pending;
  auto result = [&]() -> Result<void, QuestError> {
    try {
      const auto now = clock_.now();
      db::Transaction tx{db_};
      auto row = db_.prepare("SELECT author_id, agent_id, agent_version, quest_type, overall, ratings, excluded FROM quest_log WHERE id = ?");
      row.bind(1, questLogId);
      if (!row.step()) return tl::unexpected(fail(GameErrorCode::NotFound, MSG_GONE));
      if (row.columnText(0) != userId) return tl::unexpected(fail(GameErrorCode::Forbidden, MSG_NOT_ALLOWED));
      if (row.columnText(1) != in.agentId || row.columnText(2) != in.version) {
        QuestError e = fail(GameErrorCode::InvalidInput, "A quest scroll can't change which familiar or version it is about.");
        e.fields.push_back({"agent", e.error.message});
        return tl::unexpected(std::move(e));
      }
      const auto before = ratingSetOfRow(row, 3);
      auto after = domain::ratingSetOf(in);
      after.excluded = before.excluded;   // a Keeper's exclusion is not the author's to undo

      db_.prepare("UPDATE quest_log SET quest_type = ?, overall = ?, ratings = ?, title = ?, body = ?, recommend = ?, tags = ?, "
                  "updated_at = ? WHERE id = ?")
          .bind(1, domain::storageName(in.questType))
          .bind(2, in.overall)
          .bind(3, domain::ratingsToJson(in.skills))
          .bind(4, in.title)
          .bind(5, in.body)
          .bind(6, in.recommend ? 1 : 0)
          .bind(7, domain::tagsToJson(in.pros, in.cons))
          .bind(8, core::formatIso8601(now))
          .bind(9, questLogId)
          .run();
      const auto deltas = domain::ratingDeltas(before, after);
      if (auto r = applyDeltas(db_, in.agentId, deltas); !r) return tl::unexpected(fromGame(r.error()));
      tx.commit();
      if (!deltas.empty()) pending.ratingAgents.push_back(in.agentId);
      return {};
    } catch (const std::exception& e) {
      return tl::unexpected(fail(GameErrorCode::StorageFailed, MSG_SAVE_FAILED, e.what()));
    }
  }();
  if (result) publish(bus_, activity_, pending);
  return result;
}

Result<void> QuestService::deleteQuestLog(const std::string& userId, const std::string& questLogId) {
  Pending pending;
  auto result = db::guarded([&]() -> Result<void> {
    db::Transaction tx{db_};
    auto row = db_.prepare("SELECT author_id, agent_id, quest_type, overall, ratings, excluded FROM quest_log WHERE id = ?");
    row.bind(1, questLogId);
    if (!row.step()) return makeError(GameErrorCode::NotFound, MSG_GONE);
    if (row.columnText(0) != userId) return makeError(GameErrorCode::Forbidden, MSG_NOT_ALLOWED);
    const std::string agentId = row.columnText(1);
    const auto before = ratingSetOfRow(row, 2);

    if (auto r = applyDeltas(db_, agentId, domain::ratingDeltas(before, std::nullopt)); !r) return r;
    auto affected = xp_.revokeForQuestLog(questLogId);   // before the row goes, so helpful entries are still findable
    if (!affected) return tl::unexpected(affected.error());
    db_.prepare("DELETE FROM quest_log WHERE id = ?").bind(1, questLogId).run();   // helpful votes cascade
    for (const auto& u : *affected) {
      auto total = xp_.totalXp(u);
      if (!total) return tl::unexpected(total.error());
      pending.totals.emplace_back(u, *total);
    }
    tx.commit();
    pending.ratingAgents.push_back(agentId);
    return {};
  }).and_then([](Result<void> r) { return r; });
  if (result) publish(bus_, activity_, pending);
  return result;
}

Result<bool> QuestService::toggleHelpful(const std::string& userId, const std::string& questLogId) {
  Pending pending;
  bool nowHelpful = false;
  auto result = db::guarded([&]() -> Result<void> {
    const auto now = clock_.now();
    db::Transaction tx{db_};
    if (!canPlay(db_, userId)) return makeError(GameErrorCode::Forbidden, MSG_NOT_ALLOWED);
    auto log = db_.prepare("SELECT author_id FROM quest_log WHERE id = ? AND hidden = 0");
    log.bind(1, questLogId);
    if (!log.step()) return makeError(GameErrorCode::NotFound, MSG_GONE);
    const std::string authorId = log.columnText(0);
    if (authorId == userId) return makeError(GameErrorCode::Forbidden, "You can't vote on your own words.");

    const std::string ref = questLogId + ":" + userId;
    auto voted = db_.prepare("SELECT 1 FROM helpful_vote WHERE quest_log_id = ? AND user_id = ?");
    voted.bind(1, questLogId).bind(2, userId);
    if (voted.step()) {   // withdraw
      db_.prepare("DELETE FROM helpful_vote WHERE quest_log_id = ? AND user_id = ?").bind(1, questLogId).bind(2, userId).run();
      db_.prepare("UPDATE quest_log SET helpful_count = helpful_count - 1 WHERE id = ?").bind(1, questLogId).run();
      auto removed = xp_.revoke(authorId, domain::XpReason::HelpfulReceived, ref);
      if (!removed) return tl::unexpected(removed.error());
      if (*removed) {
        auto total = xp_.totalXp(authorId);
        if (!total) return tl::unexpected(total.error());
        pending.totals.emplace_back(authorId, *total);
      }
      nowHelpful = false;
    } else {              // add
      db_.prepare("INSERT INTO helpful_vote(quest_log_id, user_id, created_at) VALUES (?, ?, ?)")
          .bind(1, questLogId)
          .bind(2, userId)
          .bind(3, core::formatIso8601(now))
          .run();
      db_.prepare("UPDATE quest_log SET helpful_count = helpful_count + 1 WHERE id = ?").bind(1, questLogId).run();
      auto today = xp_.todaysXp(authorId, domain::XpReason::HelpfulReceived);
      if (!today) return tl::unexpected(today.error());
      if (const int allowed = domain::helpfulXpAllowed(*today); allowed > 0) {
        auto grant = xp_.award(authorId, domain::XpReason::HelpfulReceived, ref, allowed);
        if (!grant) return tl::unexpected(grant.error());
        pending.grants.emplace_back(authorId, std::move(*grant));
      }
      nowHelpful = true;
    }
    tx.commit();
    return {};
  }).and_then([](Result<void> r) { return r; });
  if (!result) return tl::unexpected(result.error());
  publish(bus_, activity_, pending);
  return nowHelpful;
}

Result<void> QuestService::setQuestLogModeration(const std::string& keeperId, const std::string& questLogId, bool hidden,
                                                 bool excluded) {
  Pending pending;
  auto result = db::guarded([&]() -> Result<void> {
    db::Transaction tx{db_};
    if (!isKeeper(db_, keeperId)) return makeError(GameErrorCode::Forbidden, "Only a Codex Keeper can do that.");
    auto row = db_.prepare("SELECT agent_id, quest_type, overall, ratings, excluded FROM quest_log WHERE id = ?");
    row.bind(1, questLogId);
    if (!row.step()) return makeError(GameErrorCode::NotFound, MSG_GONE);
    const std::string agentId = row.columnText(0);
    const auto before = ratingSetOfRow(row, 1);
    auto after = before;
    after.excluded = excluded;

    db_.prepare("UPDATE quest_log SET hidden = ?, excluded = ? WHERE id = ?")
        .bind(1, hidden ? 1 : 0)
        .bind(2, excluded ? 1 : 0)
        .bind(3, questLogId)
        .run();
    const auto deltas = domain::ratingDeltas(before, after);
    if (auto r = applyDeltas(db_, agentId, deltas); !r) return r;
    tx.commit();
    if (!deltas.empty()) pending.ratingAgents.push_back(agentId);
    return {};
  }).and_then([](Result<void> r) { return r; });
  if (result) publish(bus_, activity_, pending);
  return result;
}

Result<int> QuestService::purgeUser(const std::string& userId) {
  // 1. Votes this user gave: withdraw each (a hidden log still has its count and its author's XP to correct).
  std::vector<std::pair<std::string, std::string>> votes;   // quest log id, author id
  {
    auto listed = db::guarded([&] {
      auto st = db_.prepare("SELECT v.quest_log_id, q.author_id FROM helpful_vote v JOIN quest_log q ON q.id = v.quest_log_id "
                            "WHERE v.user_id = ?");
      st.bind(1, userId);
      while (st.step()) votes.emplace_back(st.columnText(0), st.columnText(1));
    });
    if (!listed) return tl::unexpected(listed.error());
  }
  for (const auto& [logId, authorId] : votes) {
    Pending pending;
    auto done = db::guarded([&]() -> Result<void> {
      db::Transaction tx{db_};
      db_.prepare("DELETE FROM helpful_vote WHERE quest_log_id = ? AND user_id = ?").bind(1, logId).bind(2, userId).run();
      db_.prepare("UPDATE quest_log SET helpful_count = helpful_count - 1 WHERE id = ?").bind(1, logId).run();
      auto removed = xp_.revoke(authorId, domain::XpReason::HelpfulReceived, logId + ":" + userId);
      if (!removed) return tl::unexpected(removed.error());
      if (*removed) {
        auto total = xp_.totalXp(authorId);
        if (!total) return tl::unexpected(total.error());
        pending.totals.emplace_back(authorId, *total);
      }
      tx.commit();
      return {};
    }).and_then([](Result<void> r) { return r; });
    if (!done) return tl::unexpected(done.error());
    publish(bus_, activity_, pending);
  }

  // 2. Their own logs, through the normal delete.
  std::vector<std::string> logs;
  auto listed = db::guarded([&] {
    auto st = db_.prepare("SELECT id FROM quest_log WHERE author_id = ?");
    st.bind(1, userId);
    while (st.step()) logs.push_back(st.columnText(0));
  });
  if (!listed) return tl::unexpected(listed.error());
  for (const auto& id : logs) {
    if (auto r = deleteQuestLog(userId, id); !r) return tl::unexpected(r.error());
  }
  return static_cast<int>(logs.size());
}

Result<QuestPage> QuestService::list(const QuestListQuery& query, const std::string& viewerId) {
  return db::guarded([&]() -> QuestPage {
    QuestPage out;
    std::string where = " WHERE q.hidden = 0 AND a.status = 'APPROVED'";
    if (query.agentId) where += " AND q.agent_id = ?";
    if (query.questType) where += " AND q.quest_type = ?";
    const auto bindFilters = [&](db::Statement& st) {
      int i = 1;
      if (query.agentId) st.bind(i++, *query.agentId);
      if (query.questType) st.bind(i++, domain::storageName(*query.questType));
      return i;
    };

    auto count = db_.prepare("SELECT COUNT(*) FROM quest_log q JOIN agent a ON a.id = q.agent_id" + where);
    bindFilters(count);
    count.step();
    out.total = static_cast<std::size_t>(count.columnInt(0));
    out.slice = domain::paginate(out.total, query.page, QUEST_PAGE_SIZE);

    const char* order = query.sort == QuestSort::MostHelpful ? " ORDER BY q.helpful_count DESC, q.created_at DESC, q.id DESC"
                                                             : " ORDER BY q.created_at DESC, q.id DESC";
    auto st = db_.prepare(std::string("SELECT ") + ROW_COLUMNS +
                          " FROM quest_log q JOIN user u ON u.id = q.author_id JOIN agent a ON a.id = q.agent_id" + where +
                          order + " LIMIT ? OFFSET ?");
    int next = bindFilters(st);
    st.bind(next++, static_cast<std::int64_t>(out.slice.count));
    st.bind(next, static_cast<std::int64_t>(out.slice.first));
    while (st.step()) out.logs.push_back(readRecord(st));

    if (!viewerId.empty()) {
      auto voted = db_.prepare("SELECT 1 FROM helpful_vote WHERE quest_log_id = ? AND user_id = ?");
      for (const auto& log : out.logs) {
        voted.bind(1, log.id).bind(2, viewerId);
        if (voted.step()) out.helpedByViewer.insert(log.id);
        voted.reset();
      }
    }
    return out;
  });
}

Result<std::vector<QuestLogRecord>> QuestService::topHelpful(const std::string& agentId, std::size_t limit) {
  return db::guarded([&]() -> std::vector<QuestLogRecord> {
    std::vector<QuestLogRecord> out;
    auto st = db_.prepare(std::string("SELECT ") + ROW_COLUMNS +
                          " FROM quest_log q JOIN user u ON u.id = q.author_id JOIN agent a ON a.id = q.agent_id "
                          "WHERE q.agent_id = ? AND q.hidden = 0 AND a.status = 'APPROVED' "
                          "ORDER BY q.helpful_count DESC, q.created_at DESC, q.id DESC LIMIT ?");
    st.bind(1, agentId).bind(2, static_cast<std::int64_t>(limit));
    while (st.step()) out.push_back(readRecord(st));
    return out;
  });
}

Result<bool> QuestService::hasVoted(const std::string& userId, const std::string& questLogId) {
  return db::guarded([&]() -> bool {
    auto st = db_.prepare("SELECT 1 FROM helpful_vote WHERE quest_log_id = ? AND user_id = ?");
    st.bind(1, questLogId).bind(2, userId);
    return st.step();
  });
}

Result<std::optional<QuestLogRecord>> QuestService::get(const std::string& questLogId) {
  return db::guarded([&]() -> std::optional<QuestLogRecord> {
    auto st = db_.prepare(std::string("SELECT ") + ROW_COLUMNS +
                          " FROM quest_log q JOIN user u ON u.id = q.author_id JOIN agent a ON a.id = q.agent_id WHERE q.id = ?");
    st.bind(1, questLogId);
    if (!st.step()) return std::nullopt;
    return readRecord(st);
  });
}

Result<std::optional<std::string>> QuestService::findOwn(const std::string& userId, const std::string& agentId,
                                                         const std::string& version) {
  return db::guarded([&]() -> std::optional<std::string> {
    auto st = db_.prepare("SELECT id FROM quest_log WHERE author_id = ? AND agent_id = ? AND agent_version = ?");
    st.bind(1, userId).bind(2, agentId).bind(3, version);
    if (!st.step()) return std::nullopt;
    return st.columnText(0);
  });
}

}  // namespace as::services
