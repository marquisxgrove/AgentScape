#include "services/codex_store.hpp"

#include "core/civil_time.hpp"

namespace as::services {

namespace {
constexpr const char* AGENT_COLUMNS =
    "id, slug, name, vendor, kind, examine, about, website, status, proposed_by_id, created_at, updated_at";

AgentKind kindFromDb(const std::string& s) { return s == "TOOL" ? AgentKind::Tool : AgentKind::Model; }

AgentStatus statusFromDb(const std::string& s) {
  if (s == "APPROVED") return AgentStatus::Approved;
  if (s == "REJECTED") return AgentStatus::Rejected;
  return AgentStatus::Pending;
}

std::chrono::sys_seconds timeFrom(const std::string& s) {
  return core::parseIso8601(s).value_or(std::chrono::sys_seconds{});
}

Agent readAgent(const db::Statement& st) {
  Agent a;
  a.id = st.columnText(0);
  a.slug = st.columnText(1);
  a.name = st.columnText(2);
  a.vendor = st.columnText(3);
  a.kind = kindFromDb(st.columnText(4));
  a.examine = st.columnText(5);
  a.about = st.columnText(6);
  a.website = st.columnOptText(7);
  a.status = statusFromDb(st.columnText(8));
  a.proposedById = st.columnOptText(9);
  a.createdAt = timeFrom(st.columnText(10));
  a.updatedAt = timeFrom(st.columnText(11));
  return a;
}
}  // namespace

const char* kindToDb(AgentKind kind) { return kind == AgentKind::Tool ? "TOOL" : "MODEL"; }
const char* kindDisplay(AgentKind kind) { return kind == AgentKind::Tool ? "Tool" : "Model"; }

const char* statusToDb(AgentStatus status) {
  switch (status) {
    case AgentStatus::Approved: return "APPROVED";
    case AgentStatus::Rejected: return "REJECTED";
    case AgentStatus::Pending: break;
  }
  return "PENDING";
}

Result<std::optional<Agent>> findAgentBySlug(db::Database& db, const std::string& slug) {
  return db::guarded([&]() -> std::optional<Agent> {
    auto st = db.prepare(std::string("SELECT ") + AGENT_COLUMNS + " FROM agent WHERE slug = ?");
    st.bind(1, slug);
    if (!st.step()) return std::nullopt;
    return readAgent(st);
  });
}

Result<std::optional<Agent>> findAgentById(db::Database& db, const std::string& id) {
  return db::guarded([&]() -> std::optional<Agent> {
    auto st = db.prepare(std::string("SELECT ") + AGENT_COLUMNS + " FROM agent WHERE id = ?");
    st.bind(1, id);
    if (!st.step()) return std::nullopt;
    return readAgent(st);
  });
}

Result<std::vector<Agent>> listAgents(db::Database& db, AgentStatus status) {
  return db::guarded([&] {
    std::vector<Agent> out;
    auto st = db.prepare(std::string("SELECT ") + AGENT_COLUMNS +
                         " FROM agent WHERE status = ? ORDER BY name COLLATE NOCASE ASC, id ASC");
    st.bind(1, statusToDb(status));
    while (st.step()) out.push_back(readAgent(st));
    return out;
  });
}

Result<domain::StatTable> loadStats(db::Database& db, const std::string& agentId) {
  return db::guarded([&] {
    domain::StatTable table;
    auto st = db.prepare("SELECT skill, quest_type, sum, count FROM agent_skill_stat WHERE agent_id = ?");
    st.bind(1, agentId);
    while (st.step()) table[st.columnText(0)][st.columnText(1)] = domain::Agg{st.columnInt(2), st.columnInt(3)};
    return table;
  });
}

Result<void> applyRatingDelta(db::Database& db, const std::string& agentId, domain::QuestType type,
                              std::span<const RatingDelta> deltas) {
  if (!db.inTransaction()) {
    return makeError(GameErrorCode::Internal, "Something went wrong. Nothing was saved.",
                     "applyRatingDelta must run inside the caller's transaction");
  }
  return db::guarded([&] {
    // UPDATE first, INSERT only when the aggregate does not exist yet. (A single INSERT ... ON CONFLICT
    // would not work: SQLite checks CHECK constraints against the proposed row before it notices the key
    // conflict, so a negative delta would be rejected even though the existing row could absorb it.)
    auto update = db.prepare(
        "UPDATE agent_skill_stat SET sum = sum + ?, count = count + ? "
        "WHERE agent_id = ? AND skill = ? AND quest_type = ?");
    auto insert = db.prepare(
        "INSERT INTO agent_skill_stat(agent_id, skill, quest_type, sum, count) VALUES (?, ?, ?, ?, ?)");
    const std::string typeName = domain::storageName(type);
    for (const RatingDelta& d : deltas) {
      const std::string statName = domain::storageName(d.key);
      update.bind(1, d.sumDelta).bind(2, d.countDelta).bind(3, agentId).bind(4, statName).bind(5, typeName).run();
      update.reset();
      if (db.changes() == 0) {  // first rating for this (agent, stat, quest type)
        insert.bind(1, agentId).bind(2, statName).bind(3, typeName).bind(4, d.sumDelta).bind(5, d.countDelta).run();
        insert.reset();
      }
    }
  });
}

}  // namespace as::services
