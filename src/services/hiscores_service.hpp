#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/result.hpp"
#include "db/database.hpp"
#include "domain/codex_query.hpp"
#include "domain/hiscores.hpp"
#include "domain/snapshot.hpp"
#include "services/codex_cache.hpp"
#include "services/codex_service.hpp"

namespace as::services {

struct AgentRank {
  int rank = 0;            // standard competition rank (1, 2, 2, 4)
  Agent agent;
  int score = 0;           // the level on this board
};

struct AgentHiscores {
  std::vector<AgentRank> rows;         // this page only
  std::size_t total = 0;               // ranked Agents on the board, all pages
  domain::PageSlice slice;
  std::optional<std::size_t> match;    // index in `rows` of the entry a name search found
  bool searchMissed = false;           // a search was given and nothing matched
};

struct AdventurerRank {
  int rank = 0;
  std::string userId;
  std::string name;                    // the active Adventurer's name, or the user's own name if there is none
  bool npc = false;
  std::int64_t xp = 0;
  int level = 1;
  std::optional<domain::AdventurerSnapshot> look;   // the active Adventurer, for the thumbnail
};

struct AdventurerHiscores {
  std::vector<AdventurerRank> rows;
  std::size_t total = 0;
  domain::PageSlice slice;
  std::optional<std::size_t> match;
  bool searchMissed = false;
};

// The leaderboards (quest-logs Requirement 7). Agents are ranked from the cached rating tables (so they follow
// RatingsChanged through the CodexCache); adventurers are ranked by total Questing XP straight from the database.
class HiscoresService {
 public:
  HiscoresService(db::Database& db, const CodexCache& cache) : db_(db), codex_(db, cache) {}

  // Ranked Agents on one board, 25 a page. A non-empty `search` jumps to the page holding the first name match and
  // marks it (Requirement 7.4); with no match the requested page is shown and searchMissed is set.
  Result<AgentHiscores> agents(const domain::AgentBoard& board, std::size_t page, const std::string& search);

  // Every user (players, Guests and NPCs) by total XP.
  Result<AdventurerHiscores> adventurers(std::size_t page, const std::string& search);

 private:
  db::Database& db_;
  CodexService codex_;
};

}  // namespace as::services
