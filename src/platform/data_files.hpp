#pragma once

#include <chrono>
#include <filesystem>

#include "core/result.hpp"

namespace as::platform {

// Moves <db>, <db>-wal and <db>-shm aside as <name>.bak-<YYYYMMDDTHHMMSSZ> (game-shell Req 8.2).
// If a backup with that stamp exists, a numeric suffix is appended. Returns the backup path of the
// main database file, or an empty path if there was nothing to back up.
Result<std::filesystem::path> backupDatabase(const std::filesystem::path& dbPath, std::chrono::sys_seconds now);

// Creates the directory (and parents) if needed.
Result<void> ensureDirectory(const std::filesystem::path& dir);

}  // namespace as::platform
