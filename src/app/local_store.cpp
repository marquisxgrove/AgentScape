#include "app/local_store.hpp"

#include "db/migrations.hpp"
#include "platform/data_files.hpp"
#include "platform/paths.hpp"

namespace as::app {

namespace {
tl::unexpected<LocalStoreError> fail(const GameError& e, bool canStartFresh) {
  return tl::unexpected(LocalStoreError{e, canStartFresh});
}
}  // namespace

Result<OpenedStore, LocalStoreError> openLocalStore(const LocalStoreOptions& options, std::chrono::sys_seconds now) {
  std::string path = ":memory:";
  std::filesystem::path backup;

  if (options.dataDir) {
    if (auto ok = platform::ensureDirectory(*options.dataDir); !ok) return fail(ok.error(), false);
    const auto dbPath = *options.dataDir / platform::DB_FILE_NAME;
    if (options.resetData) {
      auto moved = platform::backupDatabase(dbPath, now);
      if (!moved) return fail(moved.error(), false);
      backup = *moved;
    }
    path = dbPath.string();
  }
  const bool fileBacked = options.dataDir.has_value();

  auto db = db::Database::open(path, options.journal);
  if (!db) return fail(db.error(), fileBacked);

  auto version = db::applyMigrations(*db);
  if (!version) return fail(version.error(), fileBacked);

  OpenedStore store{std::move(*db), *version, std::nullopt, backup};

  bool shouldSeed = options.reseed;
  if (!shouldSeed) {
    auto needs = db::guarded([&] { return db::needsSeed(store.db); });
    if (!needs) return fail(needs.error(), fileBacked);
    shouldSeed = *needs;
  }
  if (shouldSeed) {
    auto report = db::runSeed(store.db, now);
    if (!report) return fail(report.error(), fileBacked);
    store.seeded = std::move(*report);
  }
  return store;
}

}  // namespace as::app
