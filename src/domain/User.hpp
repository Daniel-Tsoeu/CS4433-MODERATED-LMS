#pragma once
// Abstract User and its four role subclasses (class diagram, section 2).
// Role behaviour (permissions) is polymorphic: code that holds a `const User&`
// works for every role, and no other code branches on `if (role == ...)`.
#include <optional>
#include <set>
#include <string>

#include "domain/Types.hpp"

namespace lms::domain {

inline constexpr int kMaxFailedLogins = 5;
inline constexpr TimePoint kLockoutSeconds = 5LL * 60;

/// Plain data used to build / rebuild any kind of user (factory input).
struct UserData {
  Id id = 0;
  std::string username;
  std::string fullName;
  std::string email;  // empty = not supplied
  std::string passwordHash;
  bool active = true;
  int failedAttempts = 0;
  std::optional<TimePoint> lockedUntil;
  TimePoint createdAt = 0;
  std::string studentNumber;       // students only
  std::optional<Id> departmentId;  // lecturers and heads of department
};

class User {
 public:
  virtual ~User() = default;

  Id id() const { return d_.id; }
  const std::string& username() const { return d_.username; }
  const std::string& fullName() const { return d_.fullName; }
  const std::string& email() const { return d_.email; }
  const std::string& passwordHash() const { return d_.passwordHash; }
  bool isActive() const { return d_.active; }
  int failedAttempts() const { return d_.failedAttempts; }
  std::optional<TimePoint> lockedUntil() const { return d_.lockedUntil; }
  TimePoint createdAt() const { return d_.createdAt; }

  virtual Role role() const = 0;
  virtual const std::set<Permission>& permissions() const = 0;
  /// Profile data stored in the role's own table (class-table inheritance).
  virtual std::optional<std::string> studentNumber() const { return std::nullopt; }
  virtual std::optional<Id> departmentId() const { return std::nullopt; }

  bool can(Permission p) const { return permissions().count(p) > 0; }
  bool isLocked(TimePoint now) const;

  void recordFailedLogin(TimePoint now);
  void recordSuccessfulLogin();
  void setId(Id id) { d_.id = id; }
  void setFullName(std::string v) { d_.fullName = std::move(v); }
  void setEmail(std::string v) { d_.email = std::move(v); }
  void setPasswordHash(std::string v) { d_.passwordHash = std::move(v); }
  void setActive(bool v) { d_.active = v; }

 protected:
  explicit User(UserData d) : d_(std::move(d)) {}

 private:
  UserData d_;
};

class Administrator final : public User {
 public:
  explicit Administrator(UserData d) : User(std::move(d)) {}
  Role role() const override { return Role::Admin; }
  const std::set<Permission>& permissions() const override;
};

class Lecturer final : public User {
 public:
  explicit Lecturer(const UserData& d);
  Role role() const override { return Role::Lecturer; }
  const std::set<Permission>& permissions() const override;
  std::optional<Id> departmentId() const override { return department_; }

 private:
  Id department_;
};

/// Deliberately NOT derived from Lecturer: an HoD is not a drop-in lecturer
/// (Liskov), because substituting one would grant marking rights wrongly.
class HeadOfDepartment final : public User {
 public:
  explicit HeadOfDepartment(const UserData& d);
  Role role() const override { return Role::Hod; }
  const std::set<Permission>& permissions() const override;
  std::optional<Id> departmentId() const override { return department_; }

 private:
  Id department_;
};

class Student final : public User {
 public:
  explicit Student(const UserData& d);
  Role role() const override { return Role::Student; }
  const std::set<Permission>& permissions() const override;
  std::optional<std::string> studentNumber() const override { return number_; }

 private:
  std::string number_;
};

}  // namespace lms::domain
