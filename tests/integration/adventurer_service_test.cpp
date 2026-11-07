#include <catch2/catch_test_macros.hpp>

#include <string>

#include "core/rng.hpp"
#include "db/database.hpp"
#include "db/migrations.hpp"
#include "domain/loadout.hpp"
#include "services/adventurer_service.hpp"

using namespace as;
using namespace as::services;

namespace {
struct Harness {
  db::Database db;
  core::FixedClock clock;
  core::SplitMix64 rng;
  AdventurerService svc;

  Harness()
      : db(openDb()),
        clock(std::chrono::sys_seconds{std::chrono::seconds{1793800000}}),
        rng(11),
        svc(db, clock, rng) {
    addUser("u1");
    addUser("u2");
  }

  static db::Database openDb() {
    auto d = db::Database::open(":memory:");
    REQUIRE(d.has_value());
    REQUIRE(db::applyMigrations(*d).has_value());
    return std::move(*d);
  }

  void addUser(const std::string& id) {
    db.prepare("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES (?, ?, 'PLAYER', 't', 't')")
        .bind(1, id)
        .bind(2, id)
        .run();
  }

  // Creates and returns an Adventurer, ticking the clock so updated_at differs between them.
  Adventurer make(const std::string& user, const std::string& name) {
    clock.advance(std::chrono::seconds{10});
    auto a = svc.createAdventurer(user, name);
    REQUIRE(a.has_value());
    return *a;
  }

  std::int64_t scalar(std::string_view sql) {
    auto st = db.prepare(sql);
    REQUIRE(st.step());
    return st.columnInt(0);
  }
};
}  // namespace

TEST_CASE("the first Adventurer is active with the default look and gear", "[integration][adventurer]") {
  Harness h;
  const auto a = h.make("u1", "Mossbeard");
  CHECK(a.isActive);
  CHECK(a.appearance == domain::Appearance{});
  CHECK(a.loadout == domain::defaultLoadout());
  CHECK_FALSE(h.make("u1", "Second").isActive);
}

TEST_CASE("names must be valid, unblocked and unique across all users", "[integration][adventurer]") {
  Harness h;
  h.make("u1", "Kiro Fan");
  for (const char* bad : {"kiro_fan", "KIRO-FAN", "", "bad!", "-start", "waytoolongname1", "Admin"}) {
    const auto r = h.svc.createAdventurer("u2", bad);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().message == "That name is not available.");
  }
  CHECK(h.svc.createAdventurer("u2", "Someone").has_value());
}

TEST_CASE("a 6th Adventurer is refused", "[integration][adventurer]") {
  Harness h;
  for (const char* n : {"One", "Two", "Three", "Four", "Five"}) h.make("u1", n);
  const auto sixth = h.svc.createAdventurer("u1", "Six");
  REQUIRE_FALSE(sixth.has_value());
  CHECK(sixth.error().message == "You can't have more than 5 Adventurers.");
  CHECK(h.svc.createAdventurer("u2", "Six").has_value());  // the limit is per user
}

TEST_CASE("save persists a valid look and loadout and re-validates ownership", "[integration][adventurer]") {
  Harness h;
  const auto a = h.make("u1", "Mossbeard");

  domain::Appearance look;
  look.bodyType = domain::BodyType::B;
  look.hair = 3;
  look.colors.skin = 2;
  auto gear = domain::defaultLoadout();
  const auto saved = h.svc.saveAdventurer("u1", a.id, look, gear);
  REQUIRE(saved.has_value());
  CHECK(saved->appearance == look);
  CHECK(h.svc.getActive("u1")->value().appearance == look);  // read back from the database

  // A level-locked item is refused, and nothing is written.
  auto locked = gear;
  locked[domain::Slot::Head] = "wizard_hat";
  const auto denied = h.svc.saveAdventurer("u1", a.id, look, locked);
  REQUIRE_FALSE(denied.has_value());
  CHECK(denied.error().message == "You don't have that item yet.");
  CHECK(h.svc.getActive("u1")->value().loadout == gear);

  // Out-of-range appearance and mis-slotted items are refused too.
  domain::Appearance wild = look;
  wild.hair = 99;
  CHECK_FALSE(h.svc.saveAdventurer("u1", a.id, wild, gear).has_value());
  auto misslotted = gear;
  misslotted[domain::Slot::Head] = "wooden_stick";
  CHECK_FALSE(h.svc.saveAdventurer("u1", a.id, look, misslotted).has_value());
}

TEST_CASE("a two-handed weapon with a shield is rejected on save", "[integration][adventurer]") {
  Harness h;
  const auto a = h.make("u1", "Mossbeard");
  REQUIRE(h.svc.grantItem("u1", "prompt_staff", "GRANT").has_value());
  REQUIRE(h.svc.grantItem("u1", "codex_tome", "GRANT").has_value());

  auto gear = domain::defaultLoadout();
  gear[domain::Slot::Weapon] = "prompt_staff";
  gear[domain::Slot::Shield] = "codex_tome";
  CHECK_FALSE(h.svc.saveAdventurer("u1", a.id, domain::Appearance{}, gear).has_value());
  gear.erase(domain::Slot::Shield);
  CHECK(h.svc.saveAdventurer("u1", a.id, domain::Appearance{}, gear).has_value());
}

TEST_CASE("you cannot touch another user's Adventurer", "[integration][adventurer]") {
  Harness h;
  const auto theirs = h.make("u2", "Theirs");
  h.make("u1", "Mine");
  CHECK_FALSE(h.svc.saveAdventurer("u1", theirs.id, domain::Appearance{}, domain::defaultLoadout()).has_value());
  CHECK_FALSE(h.svc.setActiveAdventurer("u1", theirs.id).has_value());
  CHECK_FALSE(h.svc.deleteAdventurer("u1", theirs.id, "Theirs").has_value());
  CHECK(h.svc.list("u2")->size() == 1);
}

TEST_CASE("exactly one Adventurer is active after any number of switches", "[integration][adventurer]") {
  Harness h;
  const auto a = h.make("u1", "Alpha");
  const auto b = h.make("u1", "Bravo");
  const auto c = h.make("u1", "Charlie");
  for (const auto* id : {&b.id, &c.id, &a.id, &c.id, &b.id}) {
    REQUIRE(h.svc.setActiveAdventurer("u1", *id).has_value());
    CHECK(h.scalar("SELECT COUNT(*) FROM adventurer WHERE user_id = 'u1' AND is_active = 1") == 1);
  }
  CHECK(h.svc.getActive("u1")->value().id == b.id);
  CHECK(h.scalar("SELECT COUNT(*) FROM adventurer WHERE user_id = 'u2' AND is_active = 1") == 0);
}

TEST_CASE("the database itself forbids two active Adventurers", "[integration][adventurer]") {
  Harness h;
  const auto a = h.make("u1", "Alpha");
  const auto b = h.make("u1", "Bravo");
  REQUIRE(a.isActive);
  // Bypass the service: the partial unique index is the backstop.
  const auto bad = db::guarded([&] {
    h.db.prepare("UPDATE adventurer SET is_active = 1 WHERE id = ?").bind(1, b.id).run();
  });
  CHECK_FALSE(bad.has_value());
}

TEST_CASE("deleting needs the typed name and promotes the most recently updated", "[integration][adventurer]") {
  Harness h;
  const auto a = h.make("u1", "Alpha");
  const auto b = h.make("u1", "Bravo");
  const auto c = h.make("u1", "Charlie");
  REQUIRE(h.svc.setActiveAdventurer("u1", a.id).has_value());
  // Touch Bravo last, so it is the most recently updated of the remaining two.
  h.clock.advance(std::chrono::seconds{60});
  REQUIRE(h.svc.saveAdventurer("u1", b.id, domain::Appearance{}, domain::defaultLoadout()).has_value());

  const auto wrong = h.svc.deleteAdventurer("u1", a.id, "Nope");
  REQUIRE_FALSE(wrong.has_value());
  CHECK(h.svc.list("u1")->size() == 3);  // nothing deleted

  REQUIRE(h.svc.deleteAdventurer("u1", a.id, "alpha").has_value());  // name match ignores case
  CHECK(h.svc.list("u1")->size() == 2);
  CHECK(h.svc.getActive("u1")->value().id == b.id);
  CHECK(h.scalar("SELECT COUNT(*) FROM adventurer WHERE user_id = 'u1' AND is_active = 1") == 1);

  // Deleting a non-active one leaves the active alone; deleting the last leaves none.
  REQUIRE(h.svc.deleteAdventurer("u1", c.id, "Charlie").has_value());
  CHECK(h.svc.getActive("u1")->value().id == b.id);
  REQUIRE(h.svc.deleteAdventurer("u1", b.id, "Bravo").has_value());
  CHECK_FALSE(h.svc.getActive("u1")->has_value());
}

TEST_CASE("a freed name can be reused", "[integration][adventurer]") {
  Harness h;
  const auto a = h.make("u1", "Alpha");
  REQUIRE(h.svc.deleteAdventurer("u1", a.id, "Alpha").has_value());
  CHECK(h.svc.createAdventurer("u2", "alpha").has_value());
}

TEST_CASE("Kiroween gear can be claimed only during the event, and is kept forever", "[integration][adventurer]") {
  Harness h;
  h.make("u1", "Alpha");

  const auto early = h.svc.claimKiroweenItem("u1", "witch_hat", /*kiroweenActive=*/false, 13);
  REQUIRE_FALSE(early.has_value());
  CHECK(early.error().message == "Returns next Kiroween.");
  CHECK(h.svc.ownedItems("u1")->count("witch_hat") == 0);

  REQUIRE(h.svc.claimKiroweenItem("u1", "witch_hat", true, 1).has_value());
  REQUIRE(h.svc.claimKiroweenItem("u1", "witch_hat", true, 1).has_value());  // idempotent
  CHECK(h.scalar("SELECT COUNT(*) FROM item_unlock WHERE user_id = 'u1'") == 1);
  CHECK(h.svc.ownedItems("u1")->count("witch_hat") == 1);

  // Owned items can then be saved even when the event is over (the claim persists).
  auto gear = domain::defaultLoadout();
  gear[domain::Slot::Head] = "witch_hat";
  CHECK(h.svc.saveAdventurer("u1", h.svc.getActive("u1")->value().id, domain::Appearance{}, gear).has_value());
}

TEST_CASE("the Reaper's Scythe needs Questing level 13; other items are not claimable", "[integration][adventurer]") {
  Harness h;
  const auto low = h.svc.claimKiroweenItem("u1", "reapers_scythe", true, 12);
  REQUIRE_FALSE(low.has_value());
  CHECK(low.error().message == "Requires Questing level 13.");
  CHECK(h.svc.claimKiroweenItem("u1", "reapers_scythe", true, 13).has_value());

  CHECK_FALSE(h.svc.claimKiroweenItem("u1", "wizard_hat", true, 99).has_value());  // a level item
  CHECK_FALSE(h.svc.claimKiroweenItem("u1", "no_such_item", true, 99).has_value());
}

TEST_CASE("stored data that no longer validates is repaired on load", "[integration][adventurer]") {
  Harness h;
  const auto a = h.make("u1", "Alpha");
  // Simulate a catalog change and a bad write from an older build.
  h.db.prepare("UPDATE adventurer SET loadout = ?, appearance = ? WHERE id = ?")
      .bind(1, R"({"Weapon":"wooden_stick","Head":"removed_item","Body":"scholar_robe"})")
      .bind(2, R"({"bodyType":"A","hair":99})")
      .bind(3, a.id)
      .run();
  const auto loaded = h.svc.getActive("u1");
  REQUIRE(loaded.has_value());
  REQUIRE(loaded->has_value());
  const auto& r = loaded->value();
  CHECK(r.repaired);
  CHECK(r.appearance == domain::Appearance{});            // bad appearance -> default
  CHECK(r.loadout.count(domain::Slot::Head) == 0);       // unknown item dropped
  CHECK(r.loadout.count(domain::Slot::Body) == 0);       // unowned item dropped
  CHECK(r.loadout.at(domain::Slot::Weapon) == "wooden_stick");  // valid entries kept

  CHECK_FALSE(h.svc.list("u1")->front().id.empty());
  CHECK_FALSE(h.make("u1", "Fresh").repaired);
}

TEST_CASE("deleting a user removes their Adventurers and unlocks", "[integration][adventurer]") {
  Harness h;
  h.make("u1", "Alpha");
  REQUIRE(h.svc.grantItem("u1", "leather_gloves", "GRANT").has_value());
  h.db.prepare("DELETE FROM user WHERE id = 'u1'").run();
  CHECK(h.scalar("SELECT COUNT(*) FROM adventurer") == 0);
  CHECK(h.scalar("SELECT COUNT(*) FROM item_unlock") == 0);
}

TEST_CASE("an unknown user cannot create an Adventurer", "[integration][adventurer]") {
  Harness h;
  CHECK_FALSE(h.svc.createAdventurer("ghost", "Nobody").has_value());
}
