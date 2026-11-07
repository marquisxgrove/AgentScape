#include <catch2/catch_test_macros.hpp>

#include <string>

#include "app/event_bus.hpp"
#include "db/database.hpp"
#include "db/migrations.hpp"
#include "db/seed_loader.hpp"
#include "services/codex_cache.hpp"
#include "services/codex_service.hpp"
#include "services/proving_service.hpp"

using namespace as;
using namespace as::services;
using domain::QuestType;

namespace {
const std::chrono::sys_seconds NOW{std::chrono::seconds{1793828551}};

struct Fixture {
  db::Database db;
  core::FixedClock clock{NOW};
  app::EventBus bus;
  CodexCache cache;
  CodexService codex;
  ProvingService svc;

  Fixture() : db(open()), cache(bus), codex(db, cache), svc(db, clock) {
    db.exec("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('alice', 'alice', 'PLAYER', 't', 't')");
    db.exec("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('bob', 'bob', 'PLAYER', 't', 't')");
  }
  static db::Database open() {
    auto d = db::Database::open(":memory:");
    REQUIRE(d.has_value());
    REQUIRE(db::applyMigrations(*d).has_value());
    REQUIRE(db::runSeed(*d, NOW).has_value());
    return std::move(*d);
  }
  std::int64_t scalar(const std::string& sql) {
    auto st = db.prepare(sql);
    REQUIRE(st.step());
    return st.columnInt(0);
  }
};
}  // namespace

TEST_CASE("nothing is remembered until a comparison is saved", "[integration][proving]") {
  Fixture f;
  const auto s = f.svc.restore("alice");
  REQUIRE(s.has_value());
  CHECK(s->slugs.empty());
  CHECK_FALSE(s->questType.has_value());
}

TEST_CASE("the last comparison comes back with its Agents and Quest Type", "[integration][proving]") {
  Fixture f;
  REQUIRE(f.svc.remember("alice", {{"gryphon", "cairn"}, QuestType::Debugging}).has_value());
  const auto s = f.svc.restore("alice");
  REQUIRE(s.has_value());
  CHECK(s->slugs == std::vector<std::string>{"gryphon", "cairn"});
  CHECK(s->questType == std::optional{QuestType::Debugging});

  REQUIRE(f.svc.remember("alice", {{"mossling"}, std::nullopt}).has_value());   // replaced, not appended
  const auto t = f.svc.restore("alice");
  CHECK(t->slugs == std::vector<std::string>{"mossling"});
  CHECK_FALSE(t->questType.has_value());
  CHECK(f.scalar("SELECT COUNT(*) FROM proving_selection") == 1);
}

TEST_CASE("each user has their own comparison", "[integration][proving]") {
  Fixture f;
  REQUIRE(f.svc.remember("alice", {{"gryphon", "cairn"}, std::nullopt}).has_value());
  REQUIRE(f.svc.remember("bob", {{"quillback", "twinwick", "deepwell"}, QuestType::Writing}).has_value());
  CHECK(f.svc.restore("alice")->slugs == std::vector<std::string>{"gryphon", "cairn"});
  CHECK(f.svc.restore("bob")->questType == std::optional{QuestType::Writing});
}

TEST_CASE("duplicates and extras are dropped, in order", "[integration][proving]") {
  Fixture f;
  const auto s = f.svc.remember("alice", {{"gryphon", "gryphon", "", "cairn", "twinwick", "deepwell"}, std::nullopt});
  REQUIRE(s.has_value());
  CHECK(s->slugs == std::vector<std::string>{"gryphon", "cairn", "twinwick"});
  CHECK(f.svc.restore("alice")->slugs == s->slugs);
}

TEST_CASE("an unknown user or NPC can't save one, and a damaged row restores as empty", "[integration][proving]") {
  Fixture f;
  CHECK_FALSE(f.svc.remember("ghost", {{"gryphon"}, std::nullopt}).has_value());
  CHECK_FALSE(f.svc.remember("seed:npc:mossbeard", {{"gryphon"}, std::nullopt}).has_value());
  f.db.exec("INSERT INTO proving_selection(user_id, slugs, quest_type, updated_at) VALUES ('alice', 'not json', 'NOPE', 't')");
  const auto s = f.svc.restore("alice");
  REQUIRE(s.has_value());
  CHECK(s->slugs.empty());
  CHECK_FALSE(s->questType.has_value());
}

TEST_CASE("deleting a user deletes their remembered comparison", "[integration][proving]") {
  Fixture f;
  REQUIRE(f.svc.remember("alice", {{"gryphon", "cairn"}, std::nullopt}).has_value());
  f.db.exec("DELETE FROM user WHERE id = 'alice'");
  CHECK(f.scalar("SELECT COUNT(*) FROM proving_selection") == 0);
}

TEST_CASE("comparison columns use the chosen scope and skip Agents that are no longer approved", "[integration][proving]") {
  Fixture f;
  const auto all = f.codex.compareColumns({"gryphon", "cairn"}, std::nullopt);
  REQUIRE(all.has_value());
  REQUIRE(all->size() == 2);
  CHECK((*all)[0].name == "Gryphon");
  CHECK((*all)[1].name == "Cairn");
  const auto dbg = f.codex.compareColumns({"gryphon", "cairn"}, QuestType::Debugging);
  CHECK((*dbg)[0].levels.overallCount < (*all)[0].levels.overallCount);   // narrower scope, fewer Quest Logs

  const auto stale = f.codex.compareColumns({"gryphon", "no-such-agent", "cairn"}, std::nullopt);
  REQUIRE(stale.has_value());
  CHECK(stale->size() == 2);
  f.db.exec("UPDATE agent SET status = 'REJECTED' WHERE slug = 'cairn'");
  f.bus.publish(app::AgentChanged{"seed:agent:cairn"});
  CHECK(f.codex.compareColumns({"gryphon", "cairn"}, std::nullopt)->size() == 1);
}

TEST_CASE("the picker list is every approved Agent by name", "[integration][proving]") {
  Fixture f;
  const auto agents = f.codex.approvedAgents();
  REQUIRE(agents.has_value());
  CHECK(agents->size() == 14);
  CHECK(agents->front().name == "Cairn");
}
