#include "services/hiscores_service.hpp"

#include <algorithm>

#include "domain/ranking.hpp"
#include "domain/xp.hpp"

namespace as::services {

namespace {
// Shared by both boards: pick the page (from a search if there is one) and the highlighted row on it.
template <typename Rows>
void pageOf(const Rows& allRanked, std::size_t requestedPage, const std::string& search, domain::PageSlice& slice,
            std::optional<std::size_t>& matchOnPage, bool& missed) {
  std::vector<std::string> names;
  names.reserve(allRanked.size());
  for (const auto& r : allRanked) names.push_back(r.name);
  std::size_t page = requestedPage;
  std::optional<std::size_t> match;
  if (!search.empty()) {
    match = domain::findByName(names, search);
    missed = !match.has_value();
    if (match) page = *match / domain::HISCORES_PAGE_SIZE;
  }
  slice = domain::paginate(allRanked.size(), page, domain::HISCORES_PAGE_SIZE);
  if (match && *match >= slice.first && *match < slice.first + slice.count) matchOnPage = *match - slice.first;
}
}  // namespace

Result<AgentHiscores> HiscoresService::agents(const domain::AgentBoard& board, std::size_t page, const std::string& search) {
  auto all = codex_.allLevels(board.scope);
  if (!all) return tl::unexpected(all.error());

  struct Row {
    Agent agent;
    int score;
    std::string name;   // for the shared page helper
  };
  std::vector<Row> ranked;
  for (auto& r : *all) {
    if (const auto score = domain::boardScore(r.levels, board)) ranked.push_back({r.agent, *score, r.agent.name});
  }
  const auto order = domain::competitionRank(std::move(ranked), [](const Row& r) { return r.score; },
                                             [](const Row& r) { return r.name; });
  std::vector<Row> inOrder;
  inOrder.reserve(order.size());
  for (const auto& [row, rank] : order) inOrder.push_back(row);

  AgentHiscores out;
  out.total = inOrder.size();
  pageOf(inOrder, page, search, out.slice, out.match, out.searchMissed);
  for (std::size_t i = out.slice.first; i < out.slice.first + out.slice.count; ++i) {
    out.rows.push_back({order[i].second, order[i].first.agent, order[i].first.score});
  }
  return out;
}

Result<AdventurerHiscores> HiscoresService::adventurers(std::size_t page, const std::string& search) {
  auto rows = db::guarded([&]() -> std::vector<AdventurerRank> {
    std::vector<AdventurerRank> out;
    auto st = db_.prepare("SELECT u.id, u.name, u.kind, u.total_xp, a.name, a.appearance, a.loadout "
                          "FROM user u LEFT JOIN adventurer a ON a.user_id = u.id AND a.is_active = 1");
    while (st.step()) {
      AdventurerRank r;
      r.userId = st.columnText(0);
      r.npc = st.columnText(2) == "NPC";
      r.xp = st.columnInt(3);
      r.level = domain::levelForXp(r.xp);
      r.name = st.columnOptText(4).value_or(st.columnText(1));
      if (!st.columnIsNull(4)) r.look = domain::snapshotFromParts(r.name, st.columnText(5), st.columnText(6));
      out.push_back(std::move(r));
    }
    return out;
  });
  if (!rows) return tl::unexpected(rows.error());

  const auto order = domain::competitionRank(std::move(*rows), [](const AdventurerRank& r) { return r.xp; },
                                             [](const AdventurerRank& r) { return r.name; });
  std::vector<AdventurerRank> inOrder;
  inOrder.reserve(order.size());
  for (const auto& [row, rank] : order) inOrder.push_back(row);

  AdventurerHiscores out;
  out.total = inOrder.size();
  pageOf(inOrder, page, search, out.slice, out.match, out.searchMissed);
  for (std::size_t i = out.slice.first; i < out.slice.first + out.slice.count; ++i) {
    AdventurerRank r = order[i].first;
    r.rank = order[i].second;
    out.rows.push_back(std::move(r));
  }
  return out;
}

}  // namespace as::services
