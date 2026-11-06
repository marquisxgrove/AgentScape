#include <catch2/catch_test_macros.hpp>

#include <array>

#include "db/database.hpp"
#include "db/migrations.hpp"

using namespace as;
using namespace as::db;

namespace {
Database freshDb() {
  auto db = Database::open(":memory:");
  REQUIRE(db.has_value());
  auto version = applyMigrations(*db);
  REQUIRE(version.has_value());
  return std::move(*db);
}

std::int64_t count(Database& db, std::string_view table) {
  auto st = db.prepare(std::string("SELECT COUNT(*) FROM ") + std::string(table));
  st.step();
  return st.columnInt(0);
}
}  // namespace

TEST_CASE("migrations create the shell tables and set user_version", "[integration][db]") {
  auto db = freshDb();
  CHECK(db.userVersion() == static_cast<int>(embeddedMigrations().size()));
  CHECK(count(db, "user") == 0);
  CHECK(count(db, "app_state") == 0);
  CHECK(count(db, "activity_event") == 0);
}

TEST_CASE("applying migrations twice is a no-op", "[integration][db]") {
  auto db = freshDb();
  const auto again = applyMigrations(db);
  REQUIRE(again.has_value());
  CHECK(*again == db.userVersion());
}

TEST_CASE("a failing migration leaves the database untouched", "[integration][db]") {
  auto db = Database::open(":memory:");
  REQUIRE(db.has_value());
  const std::array<Migration, 2> bad{
      Migration{1, "0001_ok.sql", "CREATE TABLE a (x INTEGER) STRICT;"},
      Migration{2, "0002_broken.sql", "CREATE TABLE b (oops;"},
  };
  const auto result = applyMigrations(*db, bad);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().message == "Your save could not be loaded.");
  CHECK(db->userVersion() == 0);
  auto st = db->prepare("SELECT COUNT(*) FROM sqlite_master WHERE name = 'a'");
  st.step();
  CHECK(st.columnInt(0) == 0);
}

TEST_CASE("a save from a newer build is refused", "[integration][db]") {
  auto db = Database::open(":memory:");
  REQUIRE(db.has_value());
  db->setUserVersion(999);
  CHECK_FALSE(applyMigrations(*db).has_value());
}

TEST_CASE("a transaction rolls back unless committed", "[integration][db]") {
  auto db = freshDb();
  {
    Transaction tx{db};
    db.prepare("INSERT INTO app_state(key, value) VALUES (?, ?)").bind(1, "remembered_user_id").bind(2, "x").run();
  }
  CHECK(count(db, "app_state") == 0);
  {
    Transaction tx{db};
    db.prepare("INSERT INTO app_state(key, value) VALUES (?, ?)").bind(1, "remembered_user_id").bind(2, "x").run();
    tx.commit();
  }
  CHECK(count(db, "app_state") == 1);
}

TEST_CASE("bound parameters store user text verbatim", "[integration][db]") {
  auto db = freshDb();
  const std::string evil = "Robert'); DROP TABLE user;-- %s %n";
  db.prepare(
        "INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES (?, ?, 'GUEST', ?, ?)")
      .bind(1, "01J0000000000000000000000")
      .bind(2, evil)
      .bind(3, "2026-11-06T00:00:00Z")
      .bind(4, "2026-11-06T00:00:00Z")
      .run();
  auto st = db.prepare("SELECT name FROM user");
  REQUIRE(st.step());
  CHECK(st.columnText(0) == evil);
}

TEST_CASE("STRICT tables and CHECK constraints reject bad rows", "[integration][db]") {
  auto db = freshDb();
  auto insertKind = [&](std::string_view kind) {
    return guarded([&] {
      db.prepare("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('u', 'n', ?, 't', 't')")
          .bind(1, kind)
          .run();
    });
  };
  const auto bad = insertKind("ROBOT");
  REQUIRE_FALSE(bad.has_value());
  CHECK(bad.error().code == GameErrorCode::StorageFailed);
  CHECK(insertKind("NPC").has_value());
}

TEST_CASE("foreign keys are enforced on every connection", "[integration][db]") {
  auto db = freshDb();
  auto st = db.prepare("PRAGMA foreign_keys;");
  st.step();
  CHECK(st.columnInt(0) == 1);
}
