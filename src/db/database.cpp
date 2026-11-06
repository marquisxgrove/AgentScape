#include "db/database.hpp"

#include <sqlite3.h>

#include <limits>
#include <utility>

namespace as::db {

namespace {
[[noreturn]] void fail(sqlite3* db, std::string_view what) {
  std::string msg{what};
  if (db != nullptr) {
    msg += ": ";
    msg += sqlite3_errmsg(db);
  }
  throw DbError(msg);
}

int toSqliteLen(std::size_t n) {
  if (n > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw DbError("value too large");
  return static_cast<int>(n);
}
}  // namespace

// ------------------------------------------------------------------------------------------- Database

Result<Database> Database::open(const std::string& path, JournalMode journal) {
  sqlite3* raw = nullptr;
  const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI;
  if (sqlite3_open_v2(path.c_str(), &raw, flags, nullptr) != SQLITE_OK) {
    if (raw != nullptr) sqlite3_close(raw);
    return makeError(GameErrorCode::StorageFailed, "Your save could not be loaded.");
  }
  Database db{raw};
  try {
    db.exec("PRAGMA foreign_keys = ON;");
    if (path != ":memory:") {
      db.exec(journal == JournalMode::Wal ? "PRAGMA journal_mode = WAL;" : "PRAGMA journal_mode = DELETE;");
    }
    sqlite3_busy_timeout(raw, 2000);
  } catch (const DbError&) {
    return makeError(GameErrorCode::StorageFailed, "Your save could not be loaded.");
  }
  return db;
}

Database::Database(Database&& other) noexcept : db_(std::exchange(other.db_, nullptr)) {}

Database& Database::operator=(Database&& other) noexcept {
  if (this != &other) {
    if (db_ != nullptr) sqlite3_close_v2(db_);
    db_ = std::exchange(other.db_, nullptr);
  }
  return *this;
}

Database::~Database() {
  if (db_ != nullptr) sqlite3_close_v2(db_);
}

void Database::exec(std::string_view sql) {
  const std::string owned{sql};
  char* err = nullptr;
  if (sqlite3_exec(db_, owned.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err != nullptr ? err : "unknown error";
    sqlite3_free(err);
    throw DbError("exec failed: " + msg);
  }
}

Statement Database::prepare(std::string_view sql) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.data(), toSqliteLen(sql.size()), &stmt, nullptr) != SQLITE_OK) {
    fail(db_, "prepare failed");
  }
  return Statement{db_, stmt};
}

int Database::userVersion() {
  auto st = prepare("PRAGMA user_version;");
  st.step();
  return static_cast<int>(st.columnInt(0));
}

void Database::setUserVersion(int version) {
  // PRAGMA can't take bound parameters; the value is an int we control, never user text.
  exec("PRAGMA user_version = " + std::to_string(version) + ";");
}

std::int64_t Database::lastInsertRowId() const { return sqlite3_last_insert_rowid(db_); }
int Database::changes() const { return sqlite3_changes(db_); }
bool Database::inTransaction() const { return sqlite3_get_autocommit(db_) == 0; }

// ------------------------------------------------------------------------------------------- Statement

Statement::Statement(Statement&& other) noexcept
    : db_(std::exchange(other.db_, nullptr)), stmt_(std::exchange(other.stmt_, nullptr)) {}

Statement& Statement::operator=(Statement&& other) noexcept {
  if (this != &other) {
    if (stmt_ != nullptr) sqlite3_finalize(stmt_);
    db_ = std::exchange(other.db_, nullptr);
    stmt_ = std::exchange(other.stmt_, nullptr);
  }
  return *this;
}

Statement::~Statement() {
  if (stmt_ != nullptr) sqlite3_finalize(stmt_);
}

Statement& Statement::bind(int index, std::int64_t value) {
  if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) fail(db_, "bind failed");
  return *this;
}

Statement& Statement::bind(int index, double value) {
  if (sqlite3_bind_double(stmt_, index, value) != SQLITE_OK) fail(db_, "bind failed");
  return *this;
}

Statement& Statement::bind(int index, std::string_view value) {
  if (sqlite3_bind_text(stmt_, index, value.data(), toSqliteLen(value.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
    fail(db_, "bind failed");
  }
  return *this;
}

Statement& Statement::bindNull(int index) {
  if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) fail(db_, "bind failed");
  return *this;
}

bool Statement::step() {
  const int rc = sqlite3_step(stmt_);
  if (rc == SQLITE_ROW) return true;
  if (rc == SQLITE_DONE) return false;
  fail(db_, "step failed");
}

void Statement::run() {
  while (step()) {
  }
}

void Statement::reset() {
  sqlite3_reset(stmt_);
  sqlite3_clear_bindings(stmt_);
}

std::int64_t Statement::columnInt(int col) const { return sqlite3_column_int64(stmt_, col); }
double Statement::columnDouble(int col) const { return sqlite3_column_double(stmt_, col); }

std::string Statement::columnText(int col) const {
  const auto* text = sqlite3_column_text(stmt_, col);
  const int len = sqlite3_column_bytes(stmt_, col);
  if (text == nullptr) return {};
  return {reinterpret_cast<const char*>(text), static_cast<std::size_t>(len)};
}

bool Statement::columnIsNull(int col) const { return sqlite3_column_type(stmt_, col) == SQLITE_NULL; }

std::optional<std::string> Statement::columnOptText(int col) const {
  if (columnIsNull(col)) return std::nullopt;
  return columnText(col);
}

std::optional<std::int64_t> Statement::columnOptInt(int col) const {
  if (columnIsNull(col)) return std::nullopt;
  return columnInt(col);
}

// ----------------------------------------------------------------------------------------- Transaction

Transaction::Transaction(Database& db) : db_(db) { db_.exec("BEGIN IMMEDIATE;"); }

Transaction::~Transaction() {
  if (!done_) {
    try {
      db_.exec("ROLLBACK;");
    } catch (const DbError&) {
      // Destructor must not throw; a failed rollback leaves SQLite to roll back on close.
    }
  }
}

void Transaction::commit() {
  db_.exec("COMMIT;");
  done_ = true;
}

}  // namespace as::db
