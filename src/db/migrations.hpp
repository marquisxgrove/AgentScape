#pragma once

#include <span>
#include <string_view>

#include "core/result.hpp"
#include "db/database.hpp"

namespace as::db {

struct Migration {
  int version;            // 1-based, from the file name prefix (0001_init.sql -> 1)
  std::string_view name;  // file name
  std::string_view sql;
};

// Migrations embedded from data/migrations/*.sql at build time, sorted by version.
std::span<const Migration> embeddedMigrations();

// Applies every migration with version > PRAGMA user_version, in order, inside ONE transaction.
// On failure nothing is applied and user_version is unchanged.
// Returns the resulting schema version.
Result<int> applyMigrations(Database& db, std::span<const Migration> migrations);
inline Result<int> applyMigrations(Database& db) { return applyMigrations(db, embeddedMigrations()); }

}  // namespace as::db
