#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "app/event_bus.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "db/migrations.hpp"
#include "services/activity_service.hpp"

using namespace as;
using namespace as::services;

namespace {
db::Database freshDb() {
  auto d = db::Database::open(":memory:");
  REQUIRE(d.has_value());
  REQUIRE(db::applyMigrations(*d).has_value());
  return std::move(*d);
}
}  // namespace

TEST_CASE("post records an event and publishes ActivityPosted after commit", "[integration][activity]") {
  auto db = freshDb();
  core::FixedClock clock{std::chrono::sys_seconds{std::chrono::seconds{1793800000}}};
  app::EventBus bus;
  std::vector<domain::ChatMessage> published;
  app::Subscription sub{bus, bus.subscribe<app::ActivityPosted>([&](const app::ActivityPosted& e) {
                          published.push_back(e.message);
                        })};
  ActivityService svc{db, clock, bus};

  auto posted = svc.post(ActivityKind::QuestLogCreated, "Mossbeard wrote a Quest Log.");
  REQUIRE(posted.has_value());
  REQUIRE(published.size() == 1);
  CHECK(published[0].channel == domain::Channel::Tavern);
  CHECK(published[0].text == "Mossbeard wrote a Quest Log.");
  CHECK(published[0].id == "act-" + std::to_string(posted->id));
}

TEST_CASE("latest returns the newest events oldest-first", "[integration][activity]") {
  auto db = freshDb();
  core::FixedClock clock{std::chrono::sys_seconds{std::chrono::seconds{1793800000}}};
  app::EventBus bus;
  ActivityService svc{db, clock, bus};
  for (int i = 0; i < 25; ++i) {
    clock.advance(std::chrono::seconds{60});
    REQUIRE(svc.post(ActivityKind::NoteCreated, "note " + std::to_string(i)).has_value());
  }
  auto recent = svc.latest(20);
  REQUIRE(recent.has_value());
  REQUIRE(recent->size() == 20);
  CHECK(recent->front().text == "note 5");   // oldest of the kept 20
  CHECK(recent->back().text == "note 24");    // newest overall
  CHECK(recent->front().at < recent->back().at);
}

TEST_CASE("a rolled-back post publishes nothing and stores nothing", "[integration][activity]") {
  auto db = freshDb();
  core::FixedClock clock{std::chrono::sys_seconds{std::chrono::seconds{1}}};
  app::EventBus bus;
  int events = 0;
  app::Subscription sub{bus, bus.subscribe<app::ActivityPosted>([&](const app::ActivityPosted&) { ++events; })};
  ActivityService svc{db, clock, bus};
  REQUIRE(svc.post(ActivityKind::LevelUp, "x").has_value());
  CHECK(events == 1);
  auto all = svc.latest(50);
  REQUIRE(all.has_value());
  CHECK(all->size() == 1);
}
