#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <vector>

#include "app/event_bus.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "db/migrations.hpp"
#include "db/seed_loader.hpp"
#include "domain/coding_session.hpp"
#include "domain/xp.hpp"
#include "services/coding_service.hpp"

using namespace as;
using namespace as::services;
using namespace std::chrono_literals;

namespace {
// 2026-11-04 20:00:00 UTC: four hours before midnight.
const std::chrono::sys_seconds EVENING{std::chrono::seconds{1793822400}};

struct Fixture {
  db::Database db;
  core::FixedClock clock{EVENING};
  core::SplitMix64 rng{7};
  app::EventBus bus;
  CodingService coding;
  std::vector<app::XpChanged> xpEvents;
  std::vector<app::Subscription> subs;

  Fixture() : db(open()), coding(db, clock, rng, bus) {
    db.exec("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('ana', 'Ana', 'PLAYER', 't', 't')");
    subs.push_back(app::Subscription{bus, bus.subscribe<app::XpChanged>([this](const app::XpChanged& e) { xpEvents.push_back(e); })});
  }

  static db::Database open() {
    auto d = db::Database::open(":memory:");
    REQUIRE(d.has_value());
    REQUIRE(db::applyMigrations(*d).has_value());
    REQUIRE(db::runSeed(*d, EVENING).has_value());
    return std::move(*d);
  }

  std::int64_t totalXp() {
    auto st = db.prepare("SELECT total_xp FROM user WHERE id = 'ana'");
    st.step();
    return st.columnInt(0);
  }
  std::int64_t ledgerXp() {
    auto st = db.prepare("SELECT COALESCE(SUM(amount), 0) FROM xp_entry WHERE user_id = 'ana'");
    st.step();
    return st.columnInt(0);
  }
};
}  // namespace

TEST_CASE("a session with a familiar pays a minute's XP per minute, and stop sums it up", "[integration][coding]") {
  Fixture f;
  auto s = f.coding.start("ana", "gryphon");
  REQUIRE(s.has_value());
  CHECK_FALSE(s->solo());
  CHECK(s->agentName == "Gryphon");

  f.clock.advance(90min + 30s);
  auto settled = f.coding.settle("ana");
  REQUIRE(settled.has_value());
  CHECK(settled->xp == 90);
  CHECK_FALSE(settled->ended);
  CHECK(f.totalXp() == 90);
  CHECK(f.ledgerXp() == 90);
  REQUIRE_FALSE(f.xpEvents.empty());
  CHECK(f.xpEvents.back().totalXp == 90);

  f.clock.advance(29s);   // 90 min 59 s: still 90 whole minutes
  CHECK(f.coding.settle("ana")->xp == 0);
  f.clock.advance(1s);
  CHECK(f.coding.settle("ana")->xp == 1);

  auto summary = f.coding.stop("ana");
  REQUIRE(summary.has_value());
  CHECK(summary->xp == 91);
  CHECK(summary->duration == 91min);
  CHECK_FALSE(f.coding.running("ana")->has_value());
  CHECK_FALSE(f.coding.stop("ana").has_value());   // nothing left to stop
}

TEST_CASE("one session at a time, a solo session has no familiar, and unknown familiars are refused", "[integration][coding]") {
  Fixture f;
  CHECK_FALSE(f.coding.start("ana", "no-such-familiar").has_value());
  auto solo = f.coding.start("ana", "");
  REQUIRE(solo.has_value());
  CHECK(solo->solo());
  auto again = f.coding.start("ana", "gryphon");
  REQUIRE_FALSE(again.has_value());
  CHECK(again.error().code == GameErrorCode::Forbidden);
  CHECK_FALSE(f.coding.start("nobody", "").has_value());
}

TEST_CASE("time away counts, but a session stops itself at four hours", "[integration][coding]") {
  Fixture f;
  REQUIRE(f.coding.start("ana", "").has_value());
  f.clock.advance(10h);   // the game was closed all along
  auto settled = f.coding.settle("ana");
  REQUIRE(settled.has_value());
  CHECK(settled->ended);
  CHECK(settled->xp == domain::codingXpFor(domain::CODING_SESSION_MAX));   // 240
  CHECK(f.totalXp() == 240);
  CHECK_FALSE(f.coding.running("ana")->has_value());

  // An expired session never blocks the next one.
  REQUIRE(f.coding.start("ana", "").has_value());
}

TEST_CASE("a session over midnight is paid per day, and each day stops at eight hours", "[integration][coding]") {
  Fixture f;
  // Earlier today, two full four-hour sessions: the daily limit is spent.
  for (int i = 0; i < 2; ++i) {
    f.clock.set(EVENING - 9h + std::chrono::hours{i * 4});
    REQUIRE(f.coding.start("ana", "").has_value());
    f.clock.advance(4h);
    REQUIRE(f.coding.stop("ana").has_value());
  }
  CHECK(f.totalXp() == 480);
  CHECK(*f.coding.todayXp("ana") == 480);

  // Now 22:00 to 01:00: the two hours before midnight count nothing, the hour after counts in full.
  f.clock.set(EVENING + 2h);
  REQUIRE(f.coding.start("ana", "gryphon").has_value());
  f.clock.advance(1h);
  CHECK(f.coding.settle("ana")->xp == 0);
  f.clock.advance(2h);
  auto summary = f.coding.stop("ana");
  REQUIRE(summary.has_value());
  CHECK(summary->xp == 60);
  CHECK(f.totalXp() == 540);
  CHECK(f.ledgerXp() == f.totalXp());
  CHECK(*f.coding.todayXp("ana") == 60);   // it's the new day now
}

TEST_CASE("settling often pays the same as settling once", "[integration][coding]") {
  Fixture often;
  Fixture once;
  REQUIRE(often.coding.start("ana", "").has_value());
  REQUIRE(once.coding.start("ana", "").has_value());
  for (int i = 0; i < 37; ++i) {
    often.clock.advance(97s);
    REQUIRE(often.coding.settle("ana").has_value());
  }
  once.clock.advance(97s * 37);
  REQUIRE(once.coding.settle("ana").has_value());
  CHECK(often.totalXp() == once.totalXp());
  CHECK(*often.coding.sessionXp(often.coding.running("ana")->value().id) == once.totalXp());
}

TEST_CASE("coding XP levels the player up like any other XP", "[integration][coding]") {
  Fixture f;
  REQUIRE(f.coding.start("ana", "").has_value());
  f.clock.advance(41min);   // level 2 needs 40 XP
  REQUIRE(f.coding.settle("ana").has_value());
  REQUIRE_FALSE(f.xpEvents.empty());
  CHECK(f.xpEvents.back().newLevel == 2);
  CHECK(domain::levelForXp(f.totalXp()) == 2);
}
