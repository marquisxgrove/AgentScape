#include "db/migrations.hpp"

#include <algorithm>

namespace as::db {

Result<int> applyMigrations(Database& db, std::span<const Migration> migrations) {
  try {
    const int current = db.userVersion();
    // Versions must be contiguous (1, 2, 3, ...): a gap means a missing or misnamed file.
    for (std::size_t i = 0; i < migrations.size(); ++i) {
      if (migrations[i].version != static_cast<int>(i) + 1) {
        return makeError(GameErrorCode::Internal, "Your save could not be loaded.");
      }
    }
    const int latest = static_cast<int>(migrations.size());
    if (current > latest) {
      // Save comes from a newer build; refuse rather than corrupt it.
      return makeError(GameErrorCode::StorageFailed, "Your save could not be loaded.");
    }
    if (current == latest) return latest;

    Transaction tx{db};
    for (const auto& m : migrations) {
      if (m.version > current) db.exec(m.sql);
    }
    db.setUserVersion(latest);  // PRAGMA user_version is transactional in SQLite
    tx.commit();
    return latest;
  } catch (const std::exception&) {
    return makeError(GameErrorCode::StorageFailed, "Your save could not be loaded.");
  }
}

}  // namespace as::db
