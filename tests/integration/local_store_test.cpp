#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "app/local_store.hpp"
#include "platform/data_files.hpp"

using namespace as;
using namespace as::app;
namespace fs = std::filesystem;

namespace {
const std::chrono::sys_seconds NOW{std::chrono::seconds{1793828551}};  // 2026-11-04T21:42:31Z

// A unique scratch directory per test case, removed afterwards. Never the user's data dir.
struct TempDir {
  fs::path path;
  TempDir() {
    static std::atomic<int> counter{0};
    path = fs::temp_directory_path() /
           ("agentscape-test-" + std::to_string(std::random_device{}()) + "-" + std::to_string(counter++));
    fs::remove_all(path);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
};

std::int64_t scalar(db::Database& db, std::string_view sql) {
  auto st = db.prepare(sql);
  REQUIRE(st.step());
  return st.columnInt(0);
}

LocalStoreOptions optionsFor(const TempDir& dir) {
  LocalStoreOptions o;
  o.dataDir = dir.path / "nested" / "data";
  return o;
}
}  // namespace

TEST_CASE("first launch creates the data dir, migrates and seeds", "[integration][local-store]") {
  TempDir dir;
  auto store = openLocalStore(optionsFor(dir), NOW);
  REQUIRE(store.has_value());
  CHECK(fs::exists(dir.path / "nested" / "data" / "agentscape.db"));
  CHECK(store->schemaVersion >= 1);
  REQUIRE(store->seeded.has_value());
  CHECK(scalar(store->db, "SELECT COUNT(*) FROM user WHERE kind = 'NPC'") > 0);
}

TEST_CASE("data survives a restart and the seed does not re-run", "[integration][local-store]") {
  TempDir dir;
  {
    auto store = openLocalStore(optionsFor(dir), NOW);
    REQUIRE(store.has_value());
    db::Transaction tx{store->db};
    store->db.prepare("INSERT INTO app_state(key, value) VALUES ('remembered_user_id', 'u1')").run();
    tx.commit();
  }
  auto again = openLocalStore(optionsFor(dir), NOW);
  REQUIRE(again.has_value());
  CHECK_FALSE(again->seeded.has_value());
  CHECK(scalar(again->db, "SELECT COUNT(*) FROM app_state WHERE key = 'remembered_user_id'") == 1);
}

TEST_CASE("the reseed flag runs the seed again without duplicating rows", "[integration][local-store]") {
  TempDir dir;
  std::int64_t users = 0;
  {
    auto store = openLocalStore(optionsFor(dir), NOW);
    REQUIRE(store.has_value());
    users = scalar(store->db, "SELECT COUNT(*) FROM user");
  }
  auto opts = optionsFor(dir);
  opts.reseed = true;
  auto store = openLocalStore(opts, NOW);
  REQUIRE(store.has_value());
  CHECK(store->seeded.has_value());
  CHECK(scalar(store->db, "SELECT COUNT(*) FROM user") == users);
}

TEST_CASE("the reset-data flag backs up the old save and starts a new one", "[integration][local-store]") {
  TempDir dir;
  {
    auto store = openLocalStore(optionsFor(dir), NOW);
    REQUIRE(store.has_value());
    store->db.exec("INSERT INTO app_state(key, value) VALUES ('marker', 'old')");
  }
  auto opts = optionsFor(dir);
  opts.resetData = true;
  auto store = openLocalStore(opts, NOW);
  REQUIRE(store.has_value());
  CHECK(store->backupPath.filename() == "agentscape.db.bak-20261104T214231Z");
  CHECK(fs::exists(store->backupPath));
  CHECK(scalar(store->db, "SELECT COUNT(*) FROM app_state WHERE key = 'marker'") == 0);
  CHECK(store->seeded.has_value());
}

TEST_CASE("a corrupt save fails to load, then Start fresh recovers it", "[integration][local-store]") {
  TempDir dir;
  const auto dataDir = dir.path / "nested" / "data";
  fs::create_directories(dataDir);
  {
    std::ofstream junk(dataDir / "agentscape.db", std::ios::binary);
    junk << std::string(4096, 'x');
  }
  auto failed = openLocalStore(optionsFor(dir), NOW);
  REQUIRE_FALSE(failed.has_value());
  CHECK(failed.error().canStartFresh);
  CHECK(failed.error().error.message == "Your save could not be loaded.");

  auto opts = optionsFor(dir);
  opts.resetData = true;  // what the "Start fresh" button does
  auto recovered = openLocalStore(opts, NOW);
  REQUIRE(recovered.has_value());
  CHECK(fs::file_size(recovered->backupPath) == 4096);  // the old bytes are kept, not deleted
}

TEST_CASE("backups never overwrite an earlier backup with the same stamp", "[integration][local-store]") {
  TempDir dir;
  fs::create_directories(dir.path);
  const auto dbPath = dir.path / "agentscape.db";
  std::ofstream(dbPath) << "one";
  const auto first = platform::backupDatabase(dbPath, NOW);
  std::ofstream(dbPath) << "two";
  const auto second = platform::backupDatabase(dbPath, NOW);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  CHECK(*first != *second);
  CHECK(fs::exists(*first));
  CHECK(fs::exists(*second));
  CHECK_FALSE(fs::exists(dbPath));
}

TEST_CASE("an in-memory store works when there is no persistent storage", "[integration][local-store]") {
  auto store = openLocalStore(LocalStoreOptions{}, NOW);
  REQUIRE(store.has_value());
  CHECK(store->seeded.has_value());
}

TEST_CASE("committed transactions notify the persistence hook", "[integration][local-store]") {
  auto store = openLocalStore(LocalStoreOptions{}, NOW);
  REQUIRE(store.has_value());
  int flushes = 0;
  store->db.setAfterCommit([&] { ++flushes; });
  {
    db::Transaction rolledBack{store->db};
    store->db.exec("INSERT INTO app_state(key, value) VALUES ('a', '1')");
  }
  CHECK(flushes == 0);
  {
    db::Transaction tx{store->db};
    store->db.exec("INSERT INTO app_state(key, value) VALUES ('a', '1')");
    tx.commit();
  }
  CHECK(flushes == 1);
}
