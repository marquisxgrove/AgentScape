#include <catch2/catch_test_macros.hpp>

#include <array>

#include "db/database.hpp"
#include "db/migrations.hpp"
#include "db/seed_loader.hpp"

using namespace as;
using namespace as::db;

namespace {
const std::chrono::sys_seconds NOW{std::chrono::seconds{1793828551}};

Database freshDb() {
  auto db = Database::open(":memory:");
  REQUIRE(db.has_value());
  REQUIRE(applyMigrations(*db).has_value());
  return std::move(*db);
}

std::int64_t scalar(Database& db, std::string_view sql) {
  auto st = db.prepare(sql);
  REQUIRE(st.step());
  return st.columnInt(0);
}

void insertPlayer(Database& db, std::string_view id, std::string_view name) {
  db.prepare("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES (?, ?, 'PLAYER', ?, ?)")
      .bind(1, id)
      .bind(2, name)
      .bind(3, "2026-11-01T00:00:00Z")
      .bind(4, "2026-11-01T00:00:00Z")
      .run();
}
}  // namespace

TEST_CASE("the embedded seed creates NPC adventurers and marks the save as seeded", "[integration][seed]") {
  auto db = freshDb();
  CHECK(needsSeed(db));
  const auto report = runSeed(db, NOW);
  REQUIRE(report.has_value());
  CHECK(report->npcsUpserted > 0);
  CHECK(scalar(db, "SELECT COUNT(*) FROM user WHERE kind = 'NPC'") == report->npcsUpserted);
  CHECK(scalar(db, "SELECT COUNT(*) FROM user WHERE kind = 'NPC' AND id NOT LIKE 'seed:npc:%'") == 0);
  CHECK_FALSE(needsSeed(db));
}

TEST_CASE("seeding twice gives the same row counts and leaves player rows untouched", "[integration][seed]") {
  auto db = freshDb();
  insertPlayer(db, "01JCZ0000000000000000PLAYR", "Marquis");
  REQUIRE(runSeed(db, NOW).has_value());
  const auto users = scalar(db, "SELECT COUNT(*) FROM user");
  REQUIRE(runSeed(db, NOW).has_value());
  CHECK(scalar(db, "SELECT COUNT(*) FROM user") == users);
  auto st = db.prepare("SELECT name, kind FROM user WHERE id = '01JCZ0000000000000000PLAYR'");
  REQUIRE(st.step());
  CHECK(st.columnText(0) == "Marquis");
  CHECK(st.columnText(1) == "PLAYER");
}

TEST_CASE("a reseed restores edited NPC names but never converts a non-NPC row", "[integration][seed]") {
  auto db = freshDb();
  REQUIRE(runSeed(db, NOW).has_value());
  db.exec("UPDATE user SET name = 'Renamed' WHERE id = 'seed:npc:mossbeard'");
  // A (hypothetical) player row that somehow holds a seed id must not be overwritten.
  db.exec("UPDATE user SET kind = 'PLAYER', name = 'Mine' WHERE id = 'seed:npc:quillwright'");
  REQUIRE(runSeed(db, NOW).has_value());
  auto st = db.prepare("SELECT name FROM user WHERE id = ?");
  st.bind(1, "seed:npc:mossbeard");
  REQUIRE(st.step());
  CHECK(st.columnText(0) == "Mossbeard");
  auto st2 = db.prepare("SELECT name, kind FROM user WHERE id = 'seed:npc:quillwright'");
  REQUIRE(st2.step());
  CHECK(st2.columnText(0) == "Mine");
  CHECK(st2.columnText(1) == "PLAYER");
}

TEST_CASE("invalid seed files are rejected before anything is written", "[integration][seed]") {
  auto db = freshDb();
  const auto rejects = [&](std::string_view name, std::string_view json) {
    const std::array<SeedFile, 1> files{SeedFile{name, json}};
    const auto r = runSeed(db, files, NOW);
    CHECK_FALSE(r.has_value());
    if (!r) CHECK_FALSE(r.error().detail.empty());
    CHECK(scalar(db, "SELECT COUNT(*) FROM user") == 0);
    CHECK(needsSeed(db));
  };
  rejects("npcs.json", "{ not json");
  rejects("npcs.json", R"({"snapshotAt": "yesterday", "npcs": []})");
  rejects("npcs.json", R"({"snapshotAt": "2026-10-01T00:00:00Z", "npcs": [{"id": "npc:bad", "name": "X"}]})");
  rejects("npcs.json", R"({"snapshotAt": "2026-10-01T00:00:00Z", "npcs": [{"id": "seed:npc:a", "name": ""}]})");
  rejects("npcs.json", R"({"snapshotAt": "2026-10-01T00:00:00Z",
                           "npcs": [{"id": "seed:npc:a", "name": "A"}, {"id": "seed:npc:a", "name": "B"}]})");
  rejects("mystery.json", "{}");
}

TEST_CASE("every embedded seed file has a loader", "[integration][seed]") {
  auto db = freshDb();
  const auto report = runSeed(db, NOW);
  REQUIRE(report.has_value());
  CHECK(report->files.size() == embeddedSeedFiles().size());
}
