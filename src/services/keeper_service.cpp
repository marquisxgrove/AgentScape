#include "services/keeper_service.hpp"

#include "core/civil_time.hpp"
#include "core/ids.hpp"
#include "domain/codex_validation.hpp"

namespace as::services {

namespace {
constexpr const char* MSG_NOT_KEEPER = "Only a Codex Keeper can do that.";
constexpr const char* MSG_GONE = "That is no longer there.";
constexpr const char* MSG_GENERIC = "You can't do that right now.";

template <typename T, typename Fn>
Result<T> safely(Fn&& fn) {
  try {
    return fn();
  } catch (const std::exception& e) {
    return makeError(GameErrorCode::StorageFailed, "Something went wrong. Nothing was saved.", e.what());
  }
}

bool holdsKeeperRole(db::Database& db, const std::string& userId) {
  auto st = db.prepare("SELECT 1 FROM user WHERE id = ? AND role = 'KEEPER' AND kind <> 'NPC'");
  st.bind(1, userId);
  return st.step();
}
}  // namespace

Result<void> KeeperService::setKeeper(const std::string& userId, bool on) {
  return safely<void>([&]() -> Result<void> {
    db::Transaction tx{db_};
    auto user = db_.prepare("SELECT 1 FROM user WHERE id = ? AND kind <> 'NPC'");
    user.bind(1, userId);
    if (!user.step()) return makeError(GameErrorCode::Forbidden, MSG_GENERIC);
    db_.prepare("UPDATE user SET role = ? WHERE id = ?").bind(1, on ? "KEEPER" : "USER").bind(2, userId).run();
    tx.commit();
    return {};
  });
}

Result<bool> KeeperService::isKeeper(const std::string& userId) {
  return safely<bool>([&]() -> Result<bool> { return holdsKeeperRole(db_, userId); });
}

Result<void> KeeperService::reportContent(const std::string& userId, ReportTarget target, const std::string& targetId,
                                          const std::string& rawReason) {
  // Reuse the Note-text rules' shape: trimmed, valid UTF-8, no control characters.
  std::string reason = rawReason;
  const auto b = reason.find_first_not_of(" \t");
  reason = b == std::string::npos ? std::string{} : reason.substr(b, reason.find_last_not_of(" \t") - b + 1);
  const auto len = domain::utf8Length(reason);
  bool control = false;
  for (const char c : reason) control |= static_cast<unsigned char>(c) < 0x20 || c == 0x7f;
  if (!len || control || *len < MIN_REPORT_REASON || *len > MAX_REPORT_REASON) {
    return makeError(GameErrorCode::InvalidInput, "Tell the Keepers why, in 3 to 200 characters.");
  }

  return safely<void>([&]() -> Result<void> {
    const auto now = clock_.now();
    db::Transaction tx{db_};
    auto user = db_.prepare("SELECT 1 FROM user WHERE id = ? AND kind <> 'NPC'");
    user.bind(1, userId);
    if (!user.step()) return makeError(GameErrorCode::Forbidden, MSG_GENERIC);

    auto exists = db_.prepare(target == ReportTarget::Note
                                  ? "SELECT 1 FROM note WHERE id = ? AND hidden = 0"
                                  : "SELECT 1 FROM agent WHERE id = ? AND status = 'APPROVED'");
    exists.bind(1, targetId);
    if (!exists.step()) return makeError(GameErrorCode::NotFound, MSG_GONE);

    auto dup = db_.prepare("SELECT 1 FROM report WHERE target_type = ? AND target_id = ? AND reporter_id = ?");
    dup.bind(1, target == ReportTarget::Note ? "NOTE" : "AGENT").bind(2, targetId).bind(3, userId);
    if (dup.step()) return makeError(GameErrorCode::InvalidInput, "You have already reported that.");

    db_.prepare("INSERT INTO report(id, target_type, target_id, reporter_id, reason, created_at) VALUES (?, ?, ?, ?, ?, ?)")
        .bind(1, core::newUlid(now, rng_))
        .bind(2, target == ReportTarget::Note ? "NOTE" : "AGENT")
        .bind(3, targetId)
        .bind(4, userId)
        .bind(5, reason)
        .bind(6, core::formatIso8601(now))
        .run();
    tx.commit();
    return {};
  });
}

Result<KeeperQueue> KeeperService::queue(const std::string& keeperId) {
  return safely<KeeperQueue>([&]() -> Result<KeeperQueue> {
    if (!holdsKeeperRole(db_, keeperId)) return makeError(GameErrorCode::Forbidden, MSG_NOT_KEEPER);
    KeeperQueue q;

    auto pending = db_.prepare("SELECT a.id, COALESCE(u.name, 'someone') FROM agent a LEFT JOIN user u ON u.id = a.proposed_by_id "
                               "WHERE a.status = 'PENDING' ORDER BY a.created_at, a.id");
    std::vector<std::pair<std::string, std::string>> ids;
    while (pending.step()) ids.emplace_back(pending.columnText(0), pending.columnText(1));
    for (const auto& [id, who] : ids) {
      auto agent = findAgentById(db_, id);
      if (agent && *agent) q.proposals.push_back({**agent, who});
    }

    auto reports = db_.prepare(
        "SELECT r.id, r.target_type, r.target_id, r.reason, r.created_at, COALESCE(ru.name, 'someone'), "
        "       n.text, n.hidden, n.agent_id, na.name, a.name "
        "FROM report r LEFT JOIN user ru ON ru.id = r.reporter_id "
        "LEFT JOIN note n ON r.target_type = 'NOTE' AND n.id = r.target_id "
        "LEFT JOIN agent na ON na.id = n.agent_id "
        "LEFT JOIN agent a ON r.target_type = 'AGENT' AND a.id = r.target_id "
        "WHERE r.resolved = 0 ORDER BY r.created_at, r.id");
    while (reports.step()) {
      ReportView v;
      v.id = reports.columnText(0);
      v.target = reports.columnText(1) == "NOTE" ? ReportTarget::Note : ReportTarget::Agent;
      v.targetId = reports.columnText(2);
      v.reason = reports.columnText(3);
      v.createdAt = core::parseIso8601(reports.columnText(4)).value_or(std::chrono::sys_seconds{});
      v.reporterName = reports.columnText(5);
      if (v.target == ReportTarget::Note) {
        v.targetText = reports.columnOptText(6).value_or("(deleted)");
        v.targetHidden = reports.columnInt(7) == 1;
        v.agentId = reports.columnOptText(8).value_or("");
        v.agentName = reports.columnOptText(9).value_or("");
      } else {
        v.targetText = reports.columnOptText(10).value_or("(deleted)");
        v.agentId = v.targetId;
        v.agentName = v.targetText;
      }
      q.reports.push_back(std::move(v));
    }

    auto hidden = db_.prepare("SELECT n.id, a.name, n.text, COALESCE(u.name, 'someone') FROM note n "
                              "JOIN agent a ON a.id = n.agent_id LEFT JOIN user u ON u.id = n.author_id "
                              "WHERE n.hidden = 1 ORDER BY n.created_at, n.id");
    while (hidden.step()) {
      q.hiddenNotes.push_back({hidden.columnText(0), hidden.columnText(1), hidden.columnText(2), hidden.columnText(3)});
    }
    return q;
  });
}

Result<void> KeeperService::moderateAgent(const std::string& keeperId, const std::string& agentId, bool approve) {
  std::string name;
  auto result = safely<void>([&]() -> Result<void> {
    const auto now = clock_.now();
    db::Transaction tx{db_};
    if (!holdsKeeperRole(db_, keeperId)) return makeError(GameErrorCode::Forbidden, MSG_NOT_KEEPER);
    auto agent = db_.prepare("SELECT name FROM agent WHERE id = ? AND status = 'PENDING'");
    agent.bind(1, agentId);
    if (!agent.step()) return makeError(GameErrorCode::NotFound, MSG_GONE);   // also stops a double decision
    name = agent.columnText(0);
    db_.prepare("UPDATE agent SET status = ?, updated_at = ? WHERE id = ?")
        .bind(1, approve ? "APPROVED" : "REJECTED")
        .bind(2, core::formatIso8601(now))
        .bind(3, agentId)
        .run();
    tx.commit();
    return {};
  });
  if (!result) return result;
  bus_.publish(app::AgentChanged{agentId});
  if (approve) {
    // A separate transaction after the approval commits: if the feed write fails, the approval still stands.
    (void)activity_.post(ActivityKind::AgentApproved, name + " has joined the Grand Codex.");
  }
  return {};
}

Result<void> KeeperService::setNoteHidden(const std::string& keeperId, const std::string& noteId, bool hidden) {
  std::string agentId;
  auto result = safely<void>([&]() -> Result<void> {
    db::Transaction tx{db_};
    if (!holdsKeeperRole(db_, keeperId)) return makeError(GameErrorCode::Forbidden, MSG_NOT_KEEPER);
    auto note = db_.prepare("SELECT agent_id FROM note WHERE id = ?");
    note.bind(1, noteId);
    if (!note.step()) return makeError(GameErrorCode::NotFound, MSG_GONE);
    agentId = note.columnText(0);
    db_.prepare("UPDATE note SET hidden = ? WHERE id = ?").bind(1, hidden ? 1 : 0).bind(2, noteId).run();
    tx.commit();
    return {};
  });
  if (result) bus_.publish(app::NotesChanged{agentId});
  return result;
}

Result<void> KeeperService::resolveReport(const std::string& keeperId, const std::string& reportId) {
  return safely<void>([&]() -> Result<void> {
    db::Transaction tx{db_};
    if (!holdsKeeperRole(db_, keeperId)) return makeError(GameErrorCode::Forbidden, MSG_NOT_KEEPER);
    db_.prepare("UPDATE report SET resolved = 1 WHERE id = ? AND resolved = 0").bind(1, reportId).run();
    if (db_.changes() == 0) return makeError(GameErrorCode::NotFound, MSG_GONE);
    tx.commit();
    return {};
  });
}

}  // namespace as::services
