#include "platform/data_files.hpp"

#include <array>
#include <string>
#include <system_error>

#include "core/civil_time.hpp"

namespace as::platform {

namespace fs = std::filesystem;

Result<fs::path> backupDatabase(const fs::path& dbPath, std::chrono::sys_seconds now) {
  std::error_code ec;
  if (!fs::exists(dbPath, ec)) return fs::path{};

  const std::string base = dbPath.string() + ".bak-" + core::formatCompactStamp(now);
  std::string chosen = base;
  for (int n = 1; fs::exists(chosen, ec); ++n) {
    chosen = base + "-" + std::to_string(n);
  }

  // Main file first: if that fails nothing has moved. The sidecars belong to the same snapshot.
  fs::rename(dbPath, chosen, ec);
  if (ec) return makeError(GameErrorCode::StorageFailed, "Your old save could not be moved aside.");

  constexpr std::array<const char*, 2> kSidecars{"-wal", "-shm"};
  for (const char* suffix : kSidecars) {
    const fs::path side{dbPath.string() + suffix};
    if (fs::exists(side, ec)) {
      fs::rename(side, fs::path{chosen + suffix}, ec);
      if (ec) {
        // A stale -wal next to a fresh database would be replayed into it, so remove it instead.
        fs::remove(side, ec);
      }
    }
  }
  return fs::path{chosen};
}

Result<void> ensureDirectory(const fs::path& dir) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec || !fs::is_directory(dir, ec)) {
    return makeError(GameErrorCode::StorageFailed, "Your save could not be loaded.");
  }
  return {};
}

}  // namespace as::platform
