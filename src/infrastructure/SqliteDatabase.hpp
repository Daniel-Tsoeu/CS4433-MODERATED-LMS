#pragma once
// Thin RAII wrapper over SQLite. All values reach SQL through bound
// parameters (Statement::bind*), never through string concatenation.
#include <sqlite3.h>

#include <cstdint>
#include <optional>
#include <string>

namespace lms::infrastructure {

class Statement {
 public:
  Statement(sqlite3* db, const std::string& sql);
  ~Statement();
  Statement(Statement&& o) noexcept;
  Statement& operator=(Statement&& o) noexcept;
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  // Parameter indexes are 1-based, column indexes 0-based (SQLite convention).
  Statement& bindInt(int i, std::int64_t v);
  Statement& bindDouble(int i, double v);
  Statement& bindText(int i, const std::string& v);
  Statement& bindNull(int i);
  Statement& bindOptInt(int i, const std::optional<std::int64_t>& v);
  Statement& bindOptDouble(int i, const std::optional<double>& v);

  /// Executes one step; true if a row is available.
  bool step();
  /// Runs a statement that returns no rows.
  void run();

  std::int64_t intAt(int c) const;
  double doubleAt(int c) const;
  std::string textAt(int c) const;
  bool isNull(int c) const;
  std::optional<std::int64_t> optIntAt(int c) const;
  std::optional<double> optDoubleAt(int c) const;

 private:
  sqlite3* db_ = nullptr;
  sqlite3_stmt* st_ = nullptr;
};

class SqliteDatabase {
 public:
  /// Opens (creating if needed) the file, or ":memory:" for tests.
  explicit SqliteDatabase(const std::string& path);
  ~SqliteDatabase();
  SqliteDatabase(const SqliteDatabase&) = delete;
  SqliteDatabase& operator=(const SqliteDatabase&) = delete;

  /// Creates the schema on a new database; verifies the version on an old one.
  void initialiseSchema();
  /// For constant SQL only (BEGIN, COMMIT, schema); never for user data.
  void exec(const char* constantSql);
  Statement prepare(const std::string& sql) const { return Statement(db_, sql); }
  std::int64_t lastInsertId() const { return sqlite3_last_insert_rowid(db_); }
  int changes() const { return sqlite3_changes(db_); }
  sqlite3* handle() const { return db_; }

  static constexpr int kSchemaVersion = 1;

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace lms::infrastructure
