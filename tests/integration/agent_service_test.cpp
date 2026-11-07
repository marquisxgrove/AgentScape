#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "app/event_bus.hpp"
#include "db/database.hpp"
#include "db/migrations.hpp"
#include "db/seed_loader.hpp"
#include "services/agent_service.hpp"

using namespace as;
using namespace as::services;

namespace {
const std::chrono::sys_seconds NOW{std::chrono::seconds{1793828551}};

struct Fixture {
  db::Database db;
  core::FixedClock clock{NOW};
  core::SplitMix64 rng{11};
  app::EventBus bus;
  AgentService svc;
  std::vector<std::string> changed;
  app::Subscription sub;

  Fixture() : db(open()), svc(db, clock, rng, bus) {
    user("alice");
    user("bob");
    sub = app::Subscription{bus, bus.subscribe<app::AgentChanged>([this](const app::AgentChanged& e) { changed.push_back(e.agentId); })};
  }
  static db::Database open() {
    auto d = db::Database::open(":memory:");
    REQUIRE(d.has_value());
    REQUIRE(db::applyMigrations(*d).has_value());
    REQUIRE(db::runSeed(*d, NOW).has_value());
    return std::move(*d);
  }
  void user(const std::string& id) {
    db.exec("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('" + id + "', '" + id + "', 'PLAYER', 't', 't')");
  }
  std::int64_t scalar(const std::string& sql) {
    auto st = db.prepare(sql);
    REQUIRE(st.step());
    return st.columnInt(0);
  }
  static domain::AgentInput input(const std::string& name = "Lumen Scribe") {
    domain::AgentInput in;
    in.name = name;
    in.vendor = "Candlewick";
    in.kind = domain::AgentKind::Tool;
    in.examine = "A tiny note-taking helper that glows when it is unsure.";
    return in;
  }
};
}  // namespace

TEST_CASE("a proposal creates a PENDING Agent with a slug from its name", "[integration][propose]") {
  Fixture f;
  auto in = Fixture::input("  Lumen   Scribe 2 ");
  in.about = "Line one.\nLine two.";
  in.website = "https://lumen.example/docs";
  const auto id = f.svc.proposeAgent("alice", in);
  REQUIRE(id.has_value());
  CHECK(f.scalar("SELECT COUNT(*) FROM agent WHERE id = '" + *id + "' AND slug = 'lumen-scribe-2' AND status = 'PENDING' "
                 "AND proposed_by_id = 'alice' AND kind = 'TOOL' AND website = 'https://lumen.example/docs'") == 1);
  CHECK(f.changed == std::vector<std::string>{*id});
}

TEST_CASE("a pending proposal is not in the Codex", "[integration][propose]") {
  Fixture f;
  const auto before = f.scalar("SELECT COUNT(*) FROM agent WHERE status = 'APPROVED'");
  REQUIRE(f.svc.proposeAgent("alice", Fixture::input()).has_value());
  CHECK(f.scalar("SELECT COUNT(*) FROM agent WHERE status = 'APPROVED'") == before);
}

TEST_CASE("invalid proposals are refused with the field to mark, and nothing is written", "[integration][propose]") {
  Fixture f;
  const auto before = f.scalar("SELECT COUNT(*) FROM agent");
  auto bad = Fixture::input();
  bad.website = "http://insecure.example";
  auto r = f.svc.proposeAgent("alice", bad);
  REQUIRE_FALSE(r.has_value());
  CHECK(r.error().field == "website");

  bad = Fixture::input();
  bad.examine = std::string(201, 'x');
  r = f.svc.proposeAgent("alice", bad);
  REQUIRE_FALSE(r.has_value());
  CHECK(r.error().field == "examine");

  r = f.svc.proposeAgent("alice", Fixture::input("!!!"));   // no letters or digits: no slug
  REQUIRE_FALSE(r.has_value());
  CHECK(r.error().field == "name");

  CHECK(f.svc.proposeAgent("ghost", Fixture::input()).error().error.code == GameErrorCode::Forbidden);
  CHECK(f.scalar("SELECT COUNT(*) FROM agent") == before);
  CHECK(f.changed.empty());
}

TEST_CASE("a taken slug is refused in any status, offering to open only an approved one", "[integration][propose]") {
  Fixture f;
  const auto approved = f.svc.proposeAgent("alice", Fixture::input("GRYPHON"));   // slug 'gryphon' is a seed Agent
  REQUIRE_FALSE(approved.has_value());
  CHECK(approved.error().existingSlug == std::optional<std::string>{"gryphon"});

  REQUIRE(f.svc.proposeAgent("alice", Fixture::input("Lumen Scribe")).has_value());
  const auto pending = f.svc.proposeAgent("bob", Fixture::input("lumen-scribe"));   // same slug, other spelling
  REQUIRE_FALSE(pending.has_value());
  CHECK_FALSE(pending.error().existingSlug.has_value());
  CHECK(pending.error().error.code == GameErrorCode::InvalidInput);   // refused by the rule, not by a db error
  CHECK(pending.error().field == "name");

  f.db.exec("UPDATE agent SET status = 'REJECTED' WHERE slug = 'lumen-scribe'");
  const auto rejected = f.svc.proposeAgent("bob", Fixture::input("Lumen Scribe"));
  REQUIRE_FALSE(rejected.has_value());
  CHECK_FALSE(rejected.error().existingSlug.has_value());
  CHECK(rejected.error().error.code == GameErrorCode::InvalidInput);
  CHECK(rejected.error().field == "name");
}

TEST_CASE("three proposals per rolling 24 hours", "[integration][propose]") {
  Fixture f;
  for (int i = 0; i < PROPOSALS_PER_DAY; ++i) {
    REQUIRE(f.svc.proposeAgent("alice", Fixture::input("Helper " + std::string(1, static_cast<char>('a' + i)))).has_value());
    f.clock.advance(std::chrono::hours{1});
  }
  const auto fourth = f.svc.proposeAgent("alice", Fixture::input("Helper d"));
  REQUIRE_FALSE(fourth.has_value());
  CHECK(fourth.error().error.code == GameErrorCode::RateLimited);
  CHECK(f.svc.proposeAgent("bob", Fixture::input("Helper d")).has_value());   // per user
  f.clock.advance(std::chrono::hours{22});                                     // 25 h after the first
  CHECK(f.svc.proposeAgent("alice", Fixture::input("Helper e")).has_value());
}

TEST_CASE("a rejected proposal still counts toward the daily limit", "[integration][propose]") {
  Fixture f;
  for (int i = 0; i < PROPOSALS_PER_DAY; ++i) {
    REQUIRE(f.svc.proposeAgent("alice", Fixture::input("Helper " + std::string(1, static_cast<char>('a' + i)))).has_value());
  }
  f.db.exec("UPDATE agent SET status = 'REJECTED' WHERE proposed_by_id = 'alice'");
  CHECK_FALSE(f.svc.proposeAgent("alice", Fixture::input("Helper z")).has_value());
}
