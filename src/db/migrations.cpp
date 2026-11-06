#include "db/migrations.hpp"

#include <string>

namespace as::db {

Result<int> applyMigrations(Database& db, std::span<const Migration> migrations) {
  try {
    const int current = db.userVersion();
    // Versions must be contiguous (1, 2, 3, ...): a gap means a missing or misnamed file.
    for (std::size_t i = 0; i < migrations.size(); ++i) {
      if (migrations[i].version != static_cast<int>(i) + 1) {
        return makeError(GameErrorCode::Internal, "Your save could not be loaded.",
                         "migration " + std::string(migrations[i].name) + " is out of sequence");
      }
    }
    const int latest = static_cast<int>(migrations.size());
    if (current > latest) {
      // Save comes from a newer build; refuse rather than corrupt it.
      return makeError(GameErrorCode::StorageFailed, "Your save could not be loaded.",
                       "save schema " + std::to_string(current) + " is newer than this build (" +
                           std::to_string(latest) + ")");
    }
    if (current == latest) return latest;

    Transaction tx{db};
    for (const auto& m : migrations) {
      if (m.version > current) db.exec(m.sql);
    }
    db.setUserVersion(latest);  // PRAGMA user_version is transactional in SQLite
    tx.commit();
    return latest;
  } catch (const std::exception& e) {
    return makeError(GameErrorCode::StorageFailed, "Your save could not be loaded.", e.what());
  }
}

}  // namespace as::db
