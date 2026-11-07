#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "core/ids.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "db/migrations.hpp"
#include "services/profile_service.hpp"

using namespace as;
using namespace as::services;

namespace {
struct Harness {
  db::Database db;
  core::FixedClock clock;
  core::SplitMix64 rng;
  ProfileService profiles;

  Harness()
      : db(openDb()), clock(std::chrono::sys_seconds{std::chrono::seconds{1793800000}}), rng(42), profiles(db, clock, rng) {}

  static db::Database openDb() {
    auto d = db::Database::open(":memory:");
    REQUIRE(d.has_value());
    REQUIRE(db::applyMigrations(*d).has_value());
    return std::move(*d);
  }

  void insertNpc(const std::string& id, const std::string& name) {
    db.prepare("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES (?, ?, 'NPC', 't', 't')")
        .bind(1, id)
        .bind(2, name)
        .run();
  }
};
}  // namespace

TEST_CASE("createGuest makes an isolated Guest with a Guest-XXXX name", "[integration][profile]") {
  Harness h;
  auto a = h.profiles.createGuest();
  auto b = h.profiles.createGuest();
  REQUIRE(a.has_value());
  REQUIRE(b.has_value());
  CHECK(a->kind == UserKind::Guest);
  CHECK(a->name.rfind("Guest-", 0) == 0);
  CHECK(a->name.size() == 10);
  CHECK(a->id != b->id);
  CHECK(core::isUlid(a->id));
}

TEST_CASE("a new guest adopts the provided setting defaults", "[integration][profile]") {
  Harness h;
  domain::Settings defaults;
  defaults.reduceMotion = true;  // e.g. the browser's prefers-reduced-motion
  auto guest = h.profiles.createGuest(defaults);
  REQUIRE(guest.has_value());
  CHECK(guest->settings.reduceMotion);
  CHECK(h.profiles.getUser(guest->id)->settings.reduceMotion);
}

TEST_CASE("listLocalUsers returns players only, most recent first", "[integration][profile]") {
  Harness h;
  h.profiles.createGuest();  // excluded
  h.insertNpc("seed:npc:x", "Mossbeard");  // excluded
  h.db.prepare(
         "INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES "
         "('01P0000000000000000000000A', 'Alde', 'PLAYER', 't', '2026-11-01T00:00:00Z'),"
         "('01P0000000000000000000000B', 'Bryn', 'PLAYER', 't', '2026-11-05T00:00:00Z')")
      .run();
  auto users = h.profiles.listLocalUsers();
  REQUIRE(users.has_value());
  REQUIRE(users->size() == 2);
  CHECK((*users)[0].name == "Bryn");  // more recent last_seen_at
  CHECK((*users)[1].name == "Alde");
}

TEST_CASE("players and guests can be remembered; NPCs and missing users are refused", "[integration][profile]") {
  Harness h;
  auto guest = h.profiles.createGuest();
  h.insertNpc("seed:npc:y", "Quill");
  REQUIRE(h.profiles.rememberUser(guest->id).has_value());
  auto rememberedGuest = h.profiles.rememberedUser();
  REQUIRE(rememberedGuest.has_value());
  REQUIRE(rememberedGuest->has_value());
  CHECK((*rememberedGuest)->id == guest->id);

  CHECK_FALSE(h.profiles.rememberUser("seed:npc:y").has_value());
  CHECK_FALSE(h.profiles.rememberUser("01P0000000000000000000000Z").has_value());  // missing

  h.db.prepare("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('p1', 'Marq', 'PLAYER', 't', 't')")
      .run();
  REQUIRE(h.profiles.rememberUser("p1").has_value());
  auto remembered = h.profiles.rememberedUser();
  REQUIRE(remembered.has_value());
  REQUIRE(remembered->has_value());
  CHECK((*remembered)->name == "Marq");
}

TEST_CASE("updateSettings persists and signIn refreshes last_seen_at", "[integration][profile]") {
  Harness h;
  auto guest = h.profiles.createGuest();
  domain::Settings s;
  s.plainFont = true;
  s.sound = true;
  REQUIRE(h.profiles.updateSettings(guest->id, s).has_value());
  CHECK(h.profiles.getUser(guest->id)->settings == s);

  h.clock.advance(std::chrono::hours{2});
  auto signed_in = h.profiles.signIn(guest->id);
  REQUIRE(signed_in.has_value());
  CHECK(signed_in->lastSeenAt == h.clock.now());
  CHECK_FALSE(h.profiles.signIn("seed:npc:none").has_value());
}

TEST_CASE("cleanupGuests removes only guests idle beyond 24h and clears a dangling pointer",
          "[integration][profile]") {
  Harness h;
  auto fresh = h.profiles.createGuest();
  auto stale = h.profiles.createGuest();
  h.db.prepare("UPDATE user SET last_seen_at = ? WHERE id = ?")  // idle for 25+ hours
      .bind(1, "2026-11-03T00:00:00Z")
      .bind(2, stale->id)
      .run();
  // A remembered pointer to a user that no longer exists (shouldn't normally happen).
  h.db.prepare("INSERT INTO app_state(key, value) VALUES ('remembered_user_id', 'gone')").run();

  auto removed = h.profiles.cleanupGuests();
  REQUIRE(removed.has_value());
  CHECK(*removed == 1);
  CHECK(h.profiles.getUser(fresh->id).has_value());
  CHECK_FALSE(h.profiles.getUser(stale->id).has_value());
  auto remembered = h.profiles.rememberedUser();
  REQUIRE(remembered.has_value());
  CHECK_FALSE(remembered->has_value());  // pointer cleared
}

TEST_CASE("the remembered guest survives the idle cleanup until it is forgotten", "[integration][profile]") {
  Harness h;
  auto kept = h.profiles.createGuest();
  auto other = h.profiles.createGuest();
  REQUIRE(h.profiles.rememberUser(kept->id).has_value());
  h.clock.advance(std::chrono::hours{48});  // both are now idle beyond 24h

  auto removed = h.profiles.cleanupGuests();
  REQUIRE(removed.has_value());
  CHECK(*removed == 1);
  CHECK(h.profiles.getUser(kept->id).has_value());
  CHECK_FALSE(h.profiles.getUser(other->id).has_value());
  auto remembered = h.profiles.rememberedUser();
  REQUIRE(remembered.has_value());
  REQUIRE(remembered->has_value());
  CHECK((*remembered)->id == kept->id);

  REQUIRE(h.profiles.forgetRemembered().has_value());  // Log out
  removed = h.profiles.cleanupGuests();
  REQUIRE(removed.has_value());
  CHECK(*removed == 1);
  CHECK_FALSE(h.profiles.getUser(kept->id).has_value());
}
