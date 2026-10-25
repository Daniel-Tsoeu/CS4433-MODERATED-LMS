#include "infrastructure/SqliteDatabase.hpp"

#include <filesystem>

#include "domain/Errors.hpp"
#include "infrastructure/SchemaSql.hpp"

namespace lms::infrastructure {

using domain::ConflictError;
using domain::PersistenceError;

namespace {
[[noreturn]] void raise(sqlite3* db, int rc, const char* what) {
  const std::string detail = db ? sqlite3_errmsg(db) : sqlite3_errstr(rc);
  if ((rc & 0xff) == SQLITE_CONSTRAINT)
    throw ConflictError("The record conflicts with existing data (" + detail + ").");
  throw PersistenceError(std::string("Database error in ") + what + ": " + detail);
}
}  // namespace

Statement::Statement(sqlite3* db, const std::string& sql) : db_(db) {
  const int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &st_, nullptr);
  if (rc != SQLITE_OK) raise(db_, rc, "prepare");
}
Statement::~Statement() {
  if (st_) sqlite3_finalize(st_);
}
Statement::Statement(Statement&& o) noexcept : db_(o.db_), st_(o.st_) {
  o.st_ = nullptr;
}
Statement& Statement::operator=(Statement&& o) noexcept {
  if (this != &o) {
    if (st_) sqlite3_finalize(st_);
    db_ = o.db_;
    st_ = o.st_;
    o.st_ = nullptr;
  }
  return *this;
}

Statement& Statement::bindInt(int i, std::int64_t v) {
  if (int rc = sqlite3_bind_int64(st_, i, v); rc != SQLITE_OK) raise(db_, rc, "bind");
  return *this;
}
Statement& Statement::bindDouble(int i, double v) {
  if (int rc = sqlite3_bind_double(st_, i, v); rc != SQLITE_OK) raise(db_, rc, "bind");
  return *this;
}
Statement& Statement::bindText(int i, const std::string& v) {
  if (int rc = sqlite3_bind_text(st_, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
      rc != SQLITE_OK)
    raise(db_, rc, "bind");
  return *this;
}
Statement& Statement::bindNull(int i) {
  if (int rc = sqlite3_bind_null(st_, i); rc != SQLITE_OK) raise(db_, rc, "bind");
  return *this;
}
Statement& Statement::bindOptInt(int i, const std::optional<std::int64_t>& v) {
  return v ? bindInt(i, *v) : bindNull(i);
}
Statement& Statement::bindOptDouble(int i, const std::optional<double>& v) {
  return v ? bindDouble(i, *v) : bindNull(i);
}

bool Statement::step() {
  const int rc = sqlite3_step(st_);
  if (rc == SQLITE_ROW) return true;
  if (rc == SQLITE_DONE) return false;
  raise(db_, rc, "step");
}
void Statement::run() {
  if (step()) throw PersistenceError("Statement unexpectedly returned a row.");
}

std::int64_t Statement::intAt(int c) const {
  return sqlite3_column_int64(st_, c);
}
double Statement::doubleAt(int c) const {
  return sqlite3_column_double(st_, c);
}
std::string Statement::textAt(int c) const {
  const unsigned char* t = sqlite3_column_text(st_, c);
  return t ? std::string(reinterpret_cast<const char*>(t)) : std::string();
}
bool Statement::isNull(int c) const {
  return sqlite3_column_type(st_, c) == SQLITE_NULL;
}
std::optional<std::int64_t> Statement::optIntAt(int c) const {
  return isNull(c) ? std::nullopt : std::optional<std::int64_t>(intAt(c));
}
std::optional<double> Statement::optDoubleAt(int c) const {
  return isNull(c) ? std::nullopt : std::optional<double>(doubleAt(c));
}

SqliteDatabase::SqliteDatabase(const std::string& path) {
  const bool isNewFile = path != ":memory:" && !std::filesystem::exists(path);
  const int rc = sqlite3_open_v2(path.c_str(), &db_,
                                 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
  if (rc != SQLITE_OK) {
    const std::string msg = db_ ? sqlite3_errmsg(db_) : "out of memory";
    if (db_) sqlite3_close(db_);
    db_ = nullptr;
    throw PersistenceError("Cannot open database '" + path + "': " + msg);
  }
  sqlite3_busy_timeout(db_, 5000);  // wait for another user on a shared drive
  exec("PRAGMA foreign_keys = ON;");
  if (isNewFile) {  // security: only the owner may read the database file
    std::error_code ec;
    namespace fs = std::filesystem;
    fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ec);
  }
}

SqliteDatabase::~SqliteDatabase() {
  if (db_) sqlite3_close(db_);
}

void SqliteDatabase::exec(const char* sql) {
  char* err = nullptr;
  const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    const std::string msg = err ? err : "unknown error";
    sqlite3_free(err);
    if ((rc & 0xff) == SQLITE_CONSTRAINT) throw ConflictError("Constraint violated: " + msg);
    throw PersistenceError("Database error: " + msg);
  }
}

void SqliteDatabase::initialiseSchema() {
  int version = 0;
  {
    auto st = prepare("PRAGMA user_version");
    if (st.step()) version = static_cast<int>(st.intAt(0));
  }
  if (version == kSchemaVersion) return;
  if (version != 0)
    throw PersistenceError("The database has schema version " + std::to_string(version) +
                           " but this program expects " + std::to_string(kSchemaVersion) + ".");
  exec("BEGIN IMMEDIATE");
  try {
    exec(schemaSql());
    exec(("PRAGMA user_version = " + std::to_string(kSchemaVersion)).c_str());
    exec("COMMIT");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
    throw;
  }
}

}  // namespace lms::infrastructure
