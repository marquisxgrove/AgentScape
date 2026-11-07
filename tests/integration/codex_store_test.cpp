#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>
#include <vector>

#include "app/event_bus.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "db/migrations.hpp"
#include "domain/stat_table.hpp"
#include "services/codex_cache.hpp"
#include "services/codex_store.hpp"

using namespace as;
using namespace as::services;
using domain::Agg;
using domain::QuestType;
using domain::StatKey;

namespace {
db::Database freshDb() {
  auto d = db::Database::open(":memory:");
  REQUIRE(d.has_value());
  REQUIRE(db::applyMigrations(*d).has_value());
  return std::move(*d);
}

void addAgent(db::Database& db, const std::string& id, const std::string& slug, const char* status = "APPROVED") {
  db.prepare(
        "INSERT INTO agent(id, slug, name, vendor, kind, examine, status, created_at, updated_at) "
        "VALUES (?, ?, ?, 'Vendor', 'MODEL', 'Examine.', ?, 't', 't')")
      .bind(1, id)
      .bind(2, slug)
      .bind(3, slug)
      .bind(4, status)
      .run();
}

// A Quest Log, reduced to what the aggregates see: an agent, a quest type, and a rating per stat.
struct Log {
  int agent;
  QuestType type;
  std::map<int, int> ratings;  // stat index (0 = Overall, 1..10 = Skills) -> 1..5
};

StatKey keyOf(int idx) { return idx == 0 ? StatKey{domain::Overall{}} : StatKey{domain::SKILLS[static_cast<std::size_t>(idx - 1)]}; }

std::vector<RatingDelta> createDeltas(const Log& l) {
  std::vector<RatingDelta> d;
  for (const auto& [idx, r] : l.ratings) d.push_back({keyOf(idx), r, 1});
  return d;
}
std::vector<RatingDelta> deleteDeltas(const Log& l) {
  std::vector<RatingDelta> d;
  for (const auto& [idx, r] : l.ratings) d.push_back({keyOf(idx), -r, -1});
  return d;
}
std::vector<RatingDelta> editDeltas(const Log& before, const Log& after) {
  std::vector<RatingDelta> d;
  for (const auto& [idx, r] : before.ratings) {
    const auto it = after.ratings.find(idx);
    if (it == after.ratings.end()) d.push_back({keyOf(idx), -r, -1});          // rating removed
    else if (it->second != r) d.push_back({keyOf(idx), it->second - r, 0});    // rating changed
  }
  for (const auto& [idx, r] : after.ratings) {
    if (before.ratings.count(idx) == 0) d.push_back({keyOf(idx), r, 1});       // rating added
  }
  return d;
}

Log randomLog(core::Rng& rng, int agents) {
  Log l;
  l.agent = static_cast<int>(rng.below(static_cast<std::uint64_t>(agents)));
  l.type = domain::QUEST_TYPES[rng.below(domain::QUEST_TYPES.size())];
  l.ratings[0] = 1 + static_cast<int>(rng.below(5));  // every Quest Log has an overall rating
  for (int idx = 1; idx <= 10; ++idx) {
    if (rng.below(3) == 0) l.ratings[idx] = 1 + static_cast<int>(rng.below(5));
  }
  return l;
}

// The ground truth: recompute every aggregate from the surviving logs.
std::map<std::tuple<int, int, int>, Agg> recompute(const std::vector<Log>& logs) {
  std::map<std::tuple<int, int, int>, Agg> out;
  for (const auto& l : logs) {
    for (const auto& [idx, r] : l.ratings) {
      auto& a = out[{l.agent, idx, static_cast<int>(l.type)}];
      a.sum += r;
      ++a.count;
    }
  }
  return out;
}

void requireTableMatches(db::Database& db, const std::vector<Log>& logs, int agents) {
  const auto expected = recompute(logs);
  std::map<std::tuple<int, int, int>, Agg> actual;
  for (int a = 0; a < agents; ++a) {
    const auto table = loadStats(db, "agent-" + std::to_string(a));
    REQUIRE(table.has_value());
    for (const auto& [stat, byType] : *table) {
      const auto key = domain::statKeyFromStorage(stat);
      REQUIRE(key.has_value());
      const int idx = std::holds_alternative<domain::Overall>(*key)
                          ? 0
                          : 1 + static_cast<int>(std::get<domain::Skill>(*key));
      for (const auto& [type, agg] : byType) {
        const auto qt = domain::questTypeFromStorage(type);
        REQUIRE(qt.has_value());
        if (agg.count != 0 || agg.sum != 0) actual[{a, idx, static_cast<int>(*qt)}] = agg;  // zero rows are fine
      }
    }
  }
  REQUIRE(actual == expected);
}
}  // namespace

TEST_CASE("applyRatingDelta matches a full recompute across random create, edit and delete",
          "[integration][codex]") {
  auto db = freshDb();
  constexpr int AGENTS = 3;
  for (int a = 0; a < AGENTS; ++a) addAgent(db, "agent-" + std::to_string(a), "agent-" + std::to_string(a));

  core::SplitMix64 rng(2026);
  std::vector<Log> logs;
  const auto apply = [&](const Log& l, const std::vector<RatingDelta>& deltas) {
    db::Transaction tx{db};
    const auto r = applyRatingDelta(db, "agent-" + std::to_string(l.agent), l.type, deltas);
    INFO("op detail: " << (r ? std::string() : r.error().detail));
    REQUIRE(r.has_value());
    tx.commit();
  };

  for (int step = 0; step < 600; ++step) {
    const auto op = rng.below(logs.empty() ? 1 : 3);
    if (op == 0) {                                   // create
      Log l = randomLog(rng, AGENTS);
      apply(l, createDeltas(l));
      logs.push_back(std::move(l));
    } else if (op == 1) {                            // edit: re-rate (same agent and type, as in Quest Logs)
      const auto i = rng.below(logs.size());
      Log after = randomLog(rng, AGENTS);
      after.agent = logs[i].agent;
      after.type = logs[i].type;
      apply(after, editDeltas(logs[i], after));
      logs[i] = std::move(after);
    } else {                                         // delete
      const auto i = rng.below(logs.size());
      apply(logs[i], deleteDeltas(logs[i]));
      logs.erase(logs.begin() + static_cast<std::ptrdiff_t>(i));
    }
    if (step % 25 == 0) requireTableMatches(db, logs, AGENTS);
  }
  requireTableMatches(db, logs, AGENTS);
  CHECK_FALSE(logs.empty());
}

TEST_CASE("levels derived from the stored table follow the scope", "[integration][codex]") {
  auto db = freshDb();
  addAgent(db, "agent-0", "agent-0");
  {
    db::Transaction tx{db};
    const RatingDelta coding[] = {{StatKey{domain::Skill::Coding}, 5, 1}};
    for (int i = 0; i < 3; ++i) REQUIRE(applyRatingDelta(db, "agent-0", QuestType::Coding, coding).has_value());
    const RatingDelta debug[] = {{StatKey{domain::Skill::Coding}, 1, 1}};
    for (int i = 0; i < 2; ++i) REQUIRE(applyRatingDelta(db, "agent-0", QuestType::Debugging, debug).has_value());
    tx.commit();
  }
  const auto table = loadStats(db, "agent-0");
  REQUIRE(table.has_value());
  const auto coding = domain::levelsFor(*table, QuestType::Coding);
  CHECK(coding.skills[0] == 68);                       // three 5-star Coding ratings
  CHECK(coding.counts[0] == 3);
  const auto debugging = domain::levelsFor(*table, QuestType::Debugging);
  CHECK_FALSE(debugging.skills[0].has_value());        // only two ratings: unranked in this scope
  const auto all = domain::levelsFor(*table, std::nullopt);
  CHECK(all.counts[0] == 5);
  CHECK(all.skills[0].has_value());                    // the union has five ratings (sum 17)
  CHECK(all.total == 10 - 1 + *all.skills[0]);         // the other nine Skills are unranked: 1 each
  CHECK_FALSE(all.overall.has_value());
}

TEST_CASE("applyRatingDelta refuses to run outside a transaction", "[integration][codex]") {
  auto db = freshDb();
  addAgent(db, "agent-0", "agent-0");
  const RatingDelta d[] = {{StatKey{domain::Skill::Speed}, 4, 1}};
  const auto r = applyRatingDelta(db, "agent-0", QuestType::Other, d);
  REQUIRE_FALSE(r.has_value());
  CHECK(r.error().code == GameErrorCode::Internal);
  CHECK(loadStats(db, "agent-0")->empty());
}

TEST_CASE("the table rejects a result that is not a possible set of 1-5 ratings", "[integration][codex]") {
  auto db = freshDb();
  addAgent(db, "agent-0", "agent-0");
  const auto tryDelta = [&](domain::Skill skill, std::int64_t sum, std::int64_t count) {
    db::Transaction tx{db};
    const RatingDelta d[] = {{StatKey{skill}, sum, count}};
    const auto r = applyRatingDelta(db, "agent-0", QuestType::Data, d);
    if (r) tx.commit();
    return r.has_value();
  };
  using S = domain::Skill;
  // Creating an aggregate: the first rating must itself be 1..5.
  CHECK_FALSE(tryDelta(S::Speed, 6, 1));     // a lone rating of 6
  CHECK_FALSE(tryDelta(S::Speed, 0, 1));     // a lone rating of 0
  CHECK_FALSE(tryDelta(S::Speed, -3, -1));   // deleting from an aggregate that does not exist
  CHECK(tryDelta(S::Speed, 4, 1));           // a lone rating of 4
  // Changing an existing aggregate (4 over 1 rating):
  CHECK_FALSE(tryDelta(S::Speed, 6, 0));     // a changed rating would have to be 10
  CHECK_FALSE(tryDelta(S::Speed, -4, 0));    // ...or 0
  CHECK_FALSE(tryDelta(S::Speed, -5, -1));   // deleting more than was ever added
  CHECK_FALSE(tryDelta(S::Speed, 0, -1));    // removing the rating but leaving its points behind
  CHECK(tryDelta(S::Speed, 1, 0));           // re-rating 4 -> 5 is fine
  CHECK(tryDelta(S::Speed, 3, 1));           // adding a rating of 3 (now 8 over 2)
  const auto table = loadStats(db, "agent-0");
  CHECK(domain::scopedAgg(*table, StatKey{S::Speed}, QuestType::Data) == Agg{8, 2});
  CHECK(domain::scopedAgg(*table, StatKey{S::Value}, QuestType::Data) == Agg{});     // nothing leaked in
}

TEST_CASE("agents are found by slug and id, and listed by status", "[integration][codex]") {
  auto db = freshDb();
  addAgent(db, "a1", "zeta");
  addAgent(db, "a2", "alpha");
  addAgent(db, "a3", "pending-one", "PENDING");
  CHECK(findAgentBySlug(db, "alpha")->value().id == "a2");
  CHECK(findAgentById(db, "a1")->value().slug == "zeta");
  CHECK_FALSE(findAgentBySlug(db, "nope")->has_value());
  const auto approved = listAgents(db, AgentStatus::Approved);
  REQUIRE(approved.has_value());
  REQUIRE(approved->size() == 2);
  CHECK((*approved)[0].slug == "alpha");     // alphabetical
  CHECK(listAgents(db, AgentStatus::Pending)->size() == 1);
  CHECK(listAgents(db, AgentStatus::Rejected)->empty());
}

TEST_CASE("the cache generation moves when ratings, agents or notes change", "[integration][codex]") {
  app::EventBus bus;
  CodexCache cache{bus};
  const auto g0 = cache.generation();
  bus.publish(app::GameMessage{"unrelated"});
  CHECK(cache.generation() == g0);
  bus.publish(app::RatingsChanged{"a"});
  const auto g1 = cache.generation();
  CHECK(g1 > g0);
  bus.publish(app::AgentChanged{"a"});
  const auto g2 = cache.generation();
  CHECK(g2 > g1);
  bus.publish(app::NotesChanged{"a"});
  CHECK(cache.generation() > g2);
}

TEST_CASE("deleting an agent removes its stats", "[integration][codex]") {
  auto db = freshDb();
  addAgent(db, "agent-0", "agent-0");
  {
    db::Transaction tx{db};
    const RatingDelta d[] = {{StatKey{domain::Overall{}}, 3, 1}};
    REQUIRE(applyRatingDelta(db, "agent-0", QuestType::Coding, d).has_value());
    tx.commit();
  }
  db.exec("DELETE FROM agent WHERE id = 'agent-0'");
  auto st = db.prepare("SELECT COUNT(*) FROM agent_skill_stat");
  st.step();
  CHECK(st.columnInt(0) == 0);
}
