#pragma once

#include <chrono>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "db/database.hpp"

namespace as::db {

struct SeedFile {
  std::string_view name;  // e.g. "npcs.json"
  std::string_view json;
};

// Bump whenever the content of data/seed/ changes, so existing saves pick up the new snapshot.
inline constexpr int SEED_VERSION = 3;

// Prefix of every seeded row id. Generated ids are ULIDs (Crockford base32, never ':'),
// so seed ids can never collide with rows the player created.
inline constexpr std::string_view SEED_ID_PREFIX = "seed:";

struct SeedReport {
  int npcsUpserted = 0;
  int agentsUpserted = 0;
  int agentsSkipped = 0;   // a seed Agent whose slug a player already took (the player's Agent wins)
  int statRows = 0;        // rating aggregates whose seeded baseline was written
  int notesUpserted = 0;
  std::vector<std::string> files;  // files applied, in application order
};

// Seed files embedded from data/seed/*.json at build time.
std::span<const SeedFile> embeddedSeedFiles();

// True when the database has never been seeded, or was seeded by a different SEED_VERSION.
bool needsSeed(Database& db);

// Validates every file first, then upserts all of them in ONE transaction (idempotent: upsert by
// stable seed id; never modifies or deletes rows the player created). Unknown files are an error,
// so a new seed file can't be silently ignored. Records seed_version/seeded_at in app_state.
Result<SeedReport> runSeed(Database& db, std::span<const SeedFile> files, std::chrono::sys_seconds now);
inline Result<SeedReport> runSeed(Database& db, std::chrono::sys_seconds now) {
  return runSeed(db, embeddedSeedFiles(), now);
}

}  // namespace as::db
