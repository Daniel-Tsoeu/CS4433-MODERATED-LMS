#pragma once
// Ports owned by the domain (dependency inversion). Infrastructure implements
// them; services depend only on these interfaces.
#include <string>

#include "domain/Entities.hpp"

namespace lms::domain {

class IClock {
 public:
  virtual ~IClock() = default;
  virtual TimePoint now() const = 0;
};

class IPasswordHasher {
 public:
  virtual ~IPasswordHasher() = default;
  /// Returns a self-describing, salted hash. Never the password.
  virtual std::string hash(const std::string& password) const = 0;
  virtual bool verify(const std::string& hash, const std::string& password) const = 0;
};

struct StoredFile {
  std::string relativePath;
  std::string originalName;
  std::int64_t sizeBytes = 0;
};

class IFileStore {
 public:
  virtual ~IFileStore() = default;
  /// Describes a file chosen by the user. Throws ValidationError if unreadable.
  virtual FileInfo inspect(const std::string& sourcePath) const = 0;
  /// Copies the file into managed storage under a generated, safe name.
  virtual StoredFile store(const std::string& sourcePath, const std::string& storedName) = 0;
  virtual void remove(const std::string& relativePath) noexcept = 0;
};

class IUnitOfWork {
 public:
  virtual ~IUnitOfWork() = default;
  virtual void begin() = 0;
  virtual void commit() = 0;
  virtual void rollback() noexcept = 0;
};

/// RAII guard: rolls back unless commit() was reached, so an exception in the
/// middle of a use case can never leave half a mark change in the database.
class Transaction {
 public:
  explicit Transaction(IUnitOfWork& uow) : uow_(uow) { uow_.begin(); }
  ~Transaction() {
    if (!done_) uow_.rollback();
  }
  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;
  void commit() {
    uow_.commit();
    done_ = true;
  }

 private:
  IUnitOfWork& uow_;
  bool done_ = false;
};

}  // namespace lms::domain
