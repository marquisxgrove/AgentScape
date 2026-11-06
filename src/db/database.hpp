#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include "core/result.hpp"

struct sqlite3;
struct sqlite3_stmt;

namespace as::db {

// Thrown inside src/db only; converted to GameError at the db boundary (see guarded()).
class DbError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

enum class JournalMode { Wal, Delete };

class Statement;

// RAII SQLite connection. Every connection enables foreign keys.
class Database {
 public:
  // path may be ":memory:". Returns StorageFailed on error.
  static Result<Database> open(const std::string& path, JournalMode journal = JournalMode::Wal);

  Database(Database&& other) noexcept;
  Database& operator=(Database&& other) noexcept;
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  ~Database();

  // Executes one or more statements without parameters (migrations, pragmas). Throws DbError.
  void exec(std::string_view sql);
  Statement prepare(std::string_view sql);  // throws DbError

  int userVersion();
  void setUserVersion(int version);
  std::int64_t lastInsertRowId() const;
  int changes() const;
  bool inTransaction() const;

  sqlite3* handle() const { return db_; }

 private:
  explicit Database(sqlite3* db) : db_(db) {}
  sqlite3* db_ = nullptr;
};

// RAII prepared statement. Always bind parameters; never concatenate SQL.
class Statement {
 public:
  Statement(Statement&& other) noexcept;
  Statement& operator=(Statement&& other) noexcept;
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;
  ~Statement();

  // 1-based parameter index, as in sqlite3_bind_*.
  Statement& bind(int index, std::int64_t value);
  Statement& bind(int index, int value) { return bind(index, static_cast<std::int64_t>(value)); }
  Statement& bind(int index, double value);
  Statement& bind(int index, std::string_view value);
  Statement& bind(int index, const char* value) { return bind(index, std::string_view{value}); }
  Statement& bind(int index, const std::string& value) { return bind(index, std::string_view{value}); }
  Statement& bindNull(int index);
  template <typename T>
  Statement& bind(int index, const std::optional<T>& value) {
    return value ? bind(index, *value) : bindNull(index);
  }

  bool step();  // true when a row is available; false when done. Throws DbError.
  void run();   // steps to completion (INSERT/UPDATE/DELETE)
  void reset();

  // 0-based column index, as in sqlite3_column_*.
  std::int64_t columnInt(int col) const;
  double columnDouble(int col) const;
  std::string columnText(int col) const;
  bool columnIsNull(int col) const;
  std::optional<std::string> columnOptText(int col) const;
  std::optional<std::int64_t> columnOptInt(int col) const;

 private:
  friend class Database;
  Statement(sqlite3* db, sqlite3_stmt* stmt) : db_(db), stmt_(stmt) {}
  sqlite3* db_ = nullptr;
  sqlite3_stmt* stmt_ = nullptr;
};

// RAII write transaction: rolls back on destruction unless commit() was called.
class Transaction {
 public:
  explicit Transaction(Database& db);  // BEGIN IMMEDIATE
  ~Transaction();
  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;
  Transaction(Transaction&&) = delete;
  Transaction& operator=(Transaction&&) = delete;

  void commit();
  Database& db() { return db_; }

 private:
  Database& db_;
  bool done_ = false;
};

// Runs fn, converting DbError / std::exception into GameError::StorageFailed.
template <typename Fn>
auto guarded(Fn&& fn) -> Result<decltype(fn())> {
  try {
    if constexpr (std::is_void_v<decltype(fn())>) {
      fn();
      return {};
    } else {
      return fn();
    }
  } catch (const std::exception&) {
    return makeError(GameErrorCode::StorageFailed, "Something went wrong. Nothing was saved.");
  }
}

}  // namespace as::db
