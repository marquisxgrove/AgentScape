#include "services/codex_service.hpp"

#include <algorithm>

#include "core/civil_time.hpp"
#include "domain/voting.hpp"

namespace as::services {

const char* noteKindToDb(NoteKind kind) { return kind == NoteKind::Bane ? "BANE" : "BOON"; }

Result<const CodexService::Loaded*> CodexService::loaded() {
  if (cachedGeneration_ == cache_.generation()) return &cached_;
  Loaded fresh;
  auto agents = listAgents(db_, AgentStatus::Approved);
  if (!agents) return tl::unexpected(agents.error());
  for (auto& a : *agents) {
    auto table = loadStats(db_, a.id);
    if (!table) return tl::unexpected(table.error());
    fresh.tables.push_back(std::move(*table));
    fresh.agents.push_back(std::move(a));
  }
  cached_ = std::move(fresh);
  cachedGeneration_ = cache_.generation();
  return &cached_;
}

Result<CodexPage> CodexService::list(const domain::ListQuery& query, std::optional<domain::QuestType> scope) {
  auto data = loaded();
  if (!data) return tl::unexpected(data.error());
  const Loaded& d = **data;

  std::vector<domain::Candidate> candidates;
  candidates.reserve(d.agents.size());
  for (std::size_t i = 0; i < d.agents.size(); ++i) {
    candidates.push_back({d.agents[i].name, d.agents[i].vendor, d.agents[i].kind, domain::levelsFor(d.tables[i], scope)});
  }

  const std::vector<std::size_t> order = domain::selectAndSort(candidates, query);
  CodexPage page;
  page.total = order.size();
  page.slice = domain::paginate(order.size(), query.page);
  page.vendors = domain::distinctVendors(candidates);
  for (std::size_t n = 0; n < page.slice.count; ++n) {
    const std::size_t i = order[page.slice.first + n];
    page.rows.push_back({d.agents[i], candidates[i].levels});
  }
  return page;
}

Result<std::vector<NoteView>> CodexService::notes(const std::string& agentId, NoteKind kind, std::size_t limit) {
  return db::guarded([&] {
    std::vector<NoteView> out;
    auto st = db_.prepare(
        "SELECT n.id, n.agent_id, n.kind, n.skill, n.version, n.text, n.agree, n.disagree, n.author_id, u.name, "
        "n.created_at FROM note n JOIN user u ON u.id = n.author_id "
        "WHERE n.agent_id = ? AND n.kind = ? AND n.hidden = 0");   // hidden notes never appear (Requirement 8.4)
    st.bind(1, agentId).bind(2, noteKindToDb(kind));
    while (st.step()) {
      NoteView v;
      v.id = st.columnText(0);
      v.agentId = st.columnText(1);
      v.kind = kind;
      v.skill = st.columnOptText(3);
      v.version = st.columnOptText(4);
      v.text = st.columnText(5);
      v.agree = st.columnInt(6);
      v.disagree = st.columnInt(7);
      v.authorId = st.columnText(8);
      v.authorName = st.columnText(9);
      v.createdAt = core::parseIso8601(st.columnText(10)).value_or(std::chrono::sys_seconds{});
      v.score = domain::wilsonLowerBound(v.agree, v.disagree);
      out.push_back(std::move(v));
    }
    // Best score first, then newest (Requirement 5.5); the id breaks exact ties so the order is stable.
    std::sort(out.begin(), out.end(), [](const NoteView& a, const NoteView& b) {
      if (a.score != b.score) return a.score > b.score;
      if (a.createdAt != b.createdAt) return a.createdAt > b.createdAt;
      return a.id > b.id;
    });
    if (limit != 0 && out.size() > limit) out.resize(limit);
    return out;
  });
}

Result<std::vector<Tombstone>> CodexService::tombstones() {
  return db::guarded([&]() -> std::vector<Tombstone> {
    std::vector<Tombstone> all;
    std::vector<domain::BaneCandidate> candidates;
    auto st = db_.prepare("SELECT n.id, a.slug, a.name, n.skill, n.text, n.agree, n.disagree, n.hidden "
                          "FROM note n JOIN agent a ON a.id = n.agent_id "
                          "WHERE n.kind = 'BANE' AND a.status = 'APPROVED'");
    while (st.step()) {
      all.push_back({st.columnText(0), st.columnText(1), st.columnText(2), st.columnOptText(3), st.columnText(4),
                     st.columnInt(5), st.columnInt(6)});
      candidates.push_back({st.columnText(0), st.columnInt(5), st.columnInt(6), st.columnInt(7) == 1});
    }
    std::vector<Tombstone> out;
    for (const auto i : domain::pickTombstones(candidates)) out.push_back(std::move(all[i]));
    return out;
  });
}

Result<std::vector<Agent>> CodexService::approvedAgents() {
  auto data = loaded();
  if (!data) return tl::unexpected(data.error());
  return (*data)->agents;   // loaded() already holds approved Agents only, in name order
}

Result<std::vector<domain::CompareColumn>> CodexService::compareColumns(const std::vector<std::string>& slugs,
                                                                         std::optional<domain::QuestType> scope) {
  auto data = loaded();
  if (!data) return tl::unexpected(data.error());
  const Loaded& d = **data;
  std::vector<domain::CompareColumn> out;
  for (const auto& slug : slugs) {
    const auto it = std::find_if(d.agents.begin(), d.agents.end(), [&](const Agent& a) { return a.slug == slug; });
    if (it == d.agents.end()) continue;
    out.push_back({it->name, domain::levelsFor(d.tables[static_cast<std::size_t>(it - d.agents.begin())], scope)});
  }
  return out;
}

Result<std::optional<AgentCard>> CodexService::card(const std::string& slug, std::optional<domain::QuestType> scope,
                                                    std::size_t topNotes) {
  auto data = loaded();
  if (!data) return tl::unexpected(data.error());
  const Loaded& d = **data;
  const auto it = std::find_if(d.agents.begin(), d.agents.end(), [&](const Agent& a) { return a.slug == slug; });
  if (it == d.agents.end()) return std::optional<AgentCard>{};

  const auto index = static_cast<std::size_t>(it - d.agents.begin());
  AgentCard card;
  card.agent = *it;
  card.levels = domain::levelsFor(d.tables[index], scope);

  auto boons = notes(it->id, NoteKind::Boon);
  auto banes = notes(it->id, NoteKind::Bane);
  if (!boons) return tl::unexpected(boons.error());
  if (!banes) return tl::unexpected(banes.error());
  card.boonTotal = boons->size();
  card.baneTotal = banes->size();
  if (boons->size() > topNotes) boons->resize(topNotes);
  if (banes->size() > topNotes) banes->resize(topNotes);
  card.boons = std::move(*boons);
  card.banes = std::move(*banes);
  return std::optional<AgentCard>{std::move(card)};
}

}  // namespace as::services
