#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "app/event_bus.hpp"
#include "db/database.hpp"
#include "db/migrations.hpp"
#include "db/seed_loader.hpp"
#include "domain/xp.hpp"
#include "services/codex_cache.hpp"
#include "services/codex_service.hpp"
#include "services/hiscores_service.hpp"

using namespace as;
using namespace as::services;

namespace {
const std::chrono::sys_seconds NOW{std::chrono::seconds{1793828551}};

struct Fixture {
  db::Database db;
  app::EventBus bus;
  CodexCache cache;
  HiscoresService svc;

  Fixture() : db(open()), cache(bus), svc(db, cache) {}
  static db::Database open() {
    auto d = db::Database::open(":memory:");
    REQUIRE(d.has_value());
    REQUIRE(db::applyMigrations(*d).has_value());
    REQUIRE(db::runSeed(*d, NOW).has_value());
    return std::move(*d);
  }
  void player(const std::string& id, const std::string& name, std::int64_t xp, bool withLook = false) {
    db.exec("INSERT INTO user(id, name, kind, total_xp, created_at, last_seen_at) VALUES ('" + id + "', '" + name + "', 'PLAYER', " +
            std::to_string(xp) + ", 't', 't')");
    if (withLook) {
      db.exec("INSERT INTO adventurer(id, user_id, name, name_key, appearance, loadout, is_active, created_at, updated_at) VALUES ('adv-" + id +
              "', '" + id + "', '" + name + "Advent', '" + name + " advent', "
              "'{\"bodyType\":\"A\",\"hair\":1,\"facialHair\":0,\"torso\":1,\"arms\":1,\"legs\":1,"
              "\"colors\":{\"hair\":1,\"torso\":1,\"legs\":1,\"feet\":1,\"skin\":1}}', '{}', 1, 't', 't')");
    }
  }
  void addXp(const std::string& id, std::int64_t xp) { db.exec("UPDATE user SET total_xp = " + std::to_string(xp) + " WHERE id = '" + id + "'"); }
};
}  // namespace

TEST_CASE("the Overall board ranks approved Agents by level, best first, with competition ranks", "[integration][hiscores]") {
  Fixture f;
  const auto page = f.svc.agents({}, 0, "");
  REQUIRE(page.has_value());
  REQUIRE_FALSE(page->rows.empty());
  CHECK(page->rows.front().rank == 1);
  for (std::size_t i = 0; i + 1 < page->rows.size(); ++i) {
    CHECK(page->rows[i].score >= page->rows[i + 1].score);
    CHECK(page->rows[i].rank <= page->rows[i + 1].rank);
    // A rank is 1 + the number of rows strictly above it.
    int above = 0;
    for (const auto& r : page->rows) above += r.score > page->rows[i].score ? 1 : 0;
    CHECK(page->rows[i].rank == 1 + above);
  }
  CHECK(page->total == 14);   // every seed Agent has enough Quest Logs to be ranked overall
}

TEST_CASE("a Skill board leaves off Agents unranked in that Skill, and Quest Type narrows it", "[integration][hiscores]") {
  Fixture f;
  domain::AgentBoard board;
  board.kind = domain::AgentBoardKind::Skill;
  board.skill = domain::Skill::Speed;
  const auto all = f.svc.agents(board, 0, "");
  REQUIRE(all.has_value());
  board.scope = domain::QuestType::Debugging;
  const auto debugging = f.svc.agents(board, 0, "");
  REQUIRE(debugging.has_value());
  CHECK(debugging->total <= all->total);
  for (const auto& r : debugging->rows) CHECK(r.score >= 1);

  // Drop one Agent's Speed ratings below the 3-rating threshold: it must disappear from the Speed board.
  const auto victim = all->rows.front().agent;
  f.db.exec("UPDATE agent_skill_stat SET sum = 0, count = 0, seed_sum = 0, seed_count = 0 WHERE agent_id = '" + victim.id + "' AND skill = 'SPEED'");
  f.bus.publish(app::RatingsChanged{victim.id});
  board.scope.reset();
  const auto after = f.svc.agents(board, 0, "");
  CHECK(after->total == all->total - 1);
  for (const auto& r : after->rows) CHECK(r.agent.id != victim.id);
}

TEST_CASE("a Quest Type scope ranks by the levels within that Quest Type", "[integration][hiscores]") {
  Fixture f;
  CodexService codex(f.db, f.cache);
  domain::AgentBoard board;
  board.scope = domain::QuestType::Debugging;
  const auto scoped = f.svc.agents(board, 0, "");
  const auto everything = f.svc.agents({}, 0, "");
  REQUIRE(scoped.has_value());
  REQUIRE(everything.has_value());
  bool anyDifferent = false;
  for (const auto& r : scoped->rows) {
    const auto card = codex.card(r.agent.slug, domain::QuestType::Debugging);
    REQUIRE(card.has_value());
    REQUIRE(card->has_value());
    CHECK(std::optional<int>{r.score} == (*card)->levels.overall);   // the board shows the Debugging level
    for (const auto& e : everything->rows) {
      if (e.agent.id == r.agent.id && e.score != r.score) anyDifferent = true;
    }
  }
  CHECK(anyDifferent);   // and it is not just the all-types board under another name
}

TEST_CASE("a rejected Agent is not on any board", "[integration][hiscores]") {
  Fixture f;
  f.db.exec("UPDATE agent SET status = 'REJECTED' WHERE slug = 'gryphon'");
  f.bus.publish(app::AgentChanged{"seed:agent:gryphon"});
  const auto page = f.svc.agents({}, 0, "");
  CHECK(page->total == 13);
  for (const auto& r : page->rows) CHECK(r.agent.slug != "gryphon");
}

TEST_CASE("searching the Agent board jumps to the page with the match and marks the row", "[integration][hiscores]") {
  Fixture f;
  const auto page = f.svc.agents({}, 0, "pocket fam");
  REQUIRE(page.has_value());
  REQUIRE(page->match.has_value());
  CHECK(page->rows[*page->match].agent.name == "Pocket Familiar");
  const auto none = f.svc.agents({}, 0, "zzz");
  CHECK(none->searchMissed);
  CHECK_FALSE(none->match.has_value());
  CHECK(none->slice.page == 0);   // no match: the requested page stays
}

TEST_CASE("the Adventurer board ranks everyone by XP, NPCs included, with ties sharing a rank", "[integration][hiscores]") {
  Fixture f;
  f.player("p1", "Aaron", 1000, true);
  f.player("p2", "Bella", 1000);
  f.player("p3", "Cato", 0);
  const auto page = f.svc.adventurers(0, "");
  REQUIRE(page.has_value());
  CHECK(page->total == 11);   // 8 NPCs and 3 players
  CHECK(page->rows.front().name == "Mossbeard");
  CHECK(page->rows.front().npc);
  CHECK(page->rows.front().rank == 1);
  CHECK(page->rows.front().level == domain::levelForXp(page->rows.front().xp));
  // Aaron and Bella both have 1000 XP: they share a rank and are ordered by name; the next rank skips.
  std::vector<std::pair<std::string, int>> mid;
  for (const auto& r : page->rows) {
    if (r.xp == 1000) mid.emplace_back(r.name, r.rank);
  }
  REQUIRE(mid.size() == 2);
  CHECK(mid[0].first == "AaronAdvent");   // the active Adventurer's name is shown
  CHECK(mid[1].first == "Bella");         // no Adventurer: the user's own name
  CHECK(mid[0].second == mid[1].second);
  const auto last = page->rows.back();
  CHECK(last.name == "Cato");
  CHECK(last.rank == page->rows[page->rows.size() - 1].rank);
  CHECK(last.xp == 0);
}

TEST_CASE("only a player's active Adventurer names and draws them on the board", "[integration][hiscores]") {
  Fixture f;
  f.player("p1", "Aaron", 500, true);   // active: "AaronAdvent"
  f.db.exec("INSERT INTO adventurer(id, user_id, name, name_key, appearance, loadout, is_active, created_at, updated_at) VALUES "
            "('adv-old', 'p1', 'Retired', 'retired', '{\"bodyType\":\"B\",\"hair\":2,\"facialHair\":0,\"torso\":1,\"arms\":1,\"legs\":1,"
            "\"colors\":{\"hair\":2,\"torso\":2,\"legs\":2,\"feet\":2,\"skin\":2}}', '{}', 0, 't', 't')");
  const auto page = f.svc.adventurers(0, "");
  REQUIRE(page.has_value());
  int rowsForAaron = 0;
  for (const auto& r : page->rows) {
    if (r.userId != "p1") continue;
    ++rowsForAaron;
    CHECK(r.name == "AaronAdvent");   // not "Retired"
  }
  CHECK(rowsForAaron == 1);           // one row per person, not per Adventurer
  CHECK(page->total == 9);
}

TEST_CASE("the Adventurer board shows each NPC's look, and a player's Adventurer only if they have one", "[integration][hiscores]") {
  Fixture f;
  f.player("p1", "Aaron", 5, true);
  f.player("p2", "Bella", 4);
  const auto page = f.svc.adventurers(0, "");
  REQUIRE(page.has_value());
  int looks = 0;
  for (const auto& r : page->rows) {
    if (r.userId == "p1") CHECK(r.look.has_value());
    if (r.userId == "p2") CHECK_FALSE(r.look.has_value());
    if (r.npc) CHECK(r.look.has_value());
    looks += r.look.has_value() ? 1 : 0;
  }
  CHECK(looks == 9);   // 8 NPCs and Aaron
}

TEST_CASE("the Adventurer board is 25 a page and a search jumps to its page", "[integration][hiscores]") {
  Fixture f;
  for (int i = 0; i < 40; ++i) f.player("u" + std::to_string(i), "Adv" + std::string(i < 10 ? "0" : "") + std::to_string(i), 100 + i);
  const auto first = f.svc.adventurers(0, "");
  REQUIRE(first.has_value());
  CHECK(first->total == 48);
  CHECK(first->rows.size() == 25);
  CHECK(first->slice.pages == 2);
  const auto second = f.svc.adventurers(1, "");
  CHECK(second->rows.size() == 23);

  // "Adv00" has the least XP of the 40 players, so it ranks near the bottom, on page two.
  const auto found = f.svc.adventurers(0, "adv00");
  REQUIRE(found.has_value());
  CHECK(found->slice.page == 1);
  REQUIRE(found->match.has_value());
  CHECK(found->rows[*found->match].name == "Adv00");
  // A name near the top stays on page one.
  const auto top = f.svc.adventurers(1, "mossbeard");
  CHECK(top->slice.page == 0);
  CHECK(top->rows[*top->match].name == "Mossbeard");
  // A page past the end is clamped.
  CHECK(f.svc.adventurers(9, "")->slice.page == 1);
}

TEST_CASE("XP changes move people on the board straight away", "[integration][hiscores]") {
  Fixture f;
  f.player("p1", "Aaron", 100);
  auto page = f.svc.adventurers(0, "aaron");
  const auto before = page->rows[*page->match].rank;
  f.addXp("p1", 999999);
  page = f.svc.adventurers(0, "aaron");
  CHECK(page->rows[*page->match].rank == 1);
  CHECK(page->rows[*page->match].rank < before);
}
