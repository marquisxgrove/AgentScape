#pragma once

#include <chrono>
#include <filesystem>
#include <optional>

#include "core/result.hpp"
#include "db/database.hpp"
#include "db/seed_loader.hpp"

namespace as::app {

struct LocalStoreOptions {
  // Directory holding agentscape.db. std::nullopt = in-memory (web without IndexedDB, tests).
  std::optional<std::filesystem::path> dataDir;
  db::JournalMode journal = db::JournalMode::Wal;  // web uses Delete
  bool resetData = false;                           // back up the existing file, start a new one
  bool reseed = false;                              // force the seed even if already seeded
};

struct OpenedStore {
  db::Database db;
  int schemaVersion = 0;
  std::optional<db::SeedReport> seeded;   // set when the seed ran
  std::filesystem::path backupPath;       // set when an old save was moved aside
};

struct LocalStoreError {
  GameError error;
  bool canStartFresh = false;  // an existing file could be backed up and replaced
};

// game-shell Req 8.1/8.2: open the Local_Store, apply pending migrations in one transaction,
// seed on first launch / new SEED_VERSION / --reseed. Never deletes data: resets move the old
// file aside as agentscape.db.bak-<timestamp>.
Result<OpenedStore, LocalStoreError> openLocalStore(const LocalStoreOptions& options, std::chrono::sys_seconds now);

}  // namespace as::app
