#pragma once
// SQLite implementations of the domain repository interfaces. This file and
// SqliteRepositories.cpp are the ONLY place SQL text exists in the program.
#include "domain/Ports.hpp"
#include "domain/Repositories.hpp"
#include "infrastructure/SqliteDatabase.hpp"

namespace lms::infrastructure {

class SqliteUserRepository final : public domain::IUserRepository {
 public:
  explicit SqliteUserRepository(SqliteDatabase& db) : db_(db) {}
  std::size_t count() const override;
  domain::Id insert(const domain::User& user) override;
  void update(const domain::User& user) override;
  std::unique_ptr<domain::User> findById(domain::Id id) const override;
  std::unique_ptr<domain::User> findByUsername(const std::string& username) const override;
  std::unique_ptr<domain::User> findHeadOfDepartment(domain::Id departmentId) const override;
  std::vector<std::unique_ptr<domain::User>> listAll() const override;

 private:
  SqliteDatabase& db_;
};

class SqliteDepartmentRepository final : public domain::IDepartmentRepository {
 public:
  explicit SqliteDepartmentRepository(SqliteDatabase& db) : db_(db) {}
  domain::Id insert(const domain::Department& d) override;
  void update(const domain::Department& d) override;
  std::optional<domain::Department> findById(domain::Id id) const override;
  std::vector<domain::Department> listAll() const override;

 private:
  SqliteDatabase& db_;
};

class SqliteCourseRepository final : public domain::ICourseRepository {
 public:
  explicit SqliteCourseRepository(SqliteDatabase& db) : db_(db) {}
  domain::Id insert(const domain::Course& c) override;
  std::optional<domain::Course> findById(domain::Id id) const override;
  std::vector<domain::Course> listAll() const override;
  std::vector<domain::Course> listForLecturer(domain::Id lecturerId) const override;
  std::vector<domain::Course> listForStudent(domain::Id studentId) const override;
  void assignLecturer(domain::Id courseId, domain::Id lecturerId) override;
  bool isLecturerAssigned(domain::Id courseId, domain::Id lecturerId) const override;
  std::vector<domain::Id> lecturerIds(domain::Id courseId) const override;
  void enrol(domain::Id courseId, domain::Id studentId, domain::TimePoint at) override;
  bool isEnrolled(domain::Id courseId, domain::Id studentId) const override;

 private:
  SqliteDatabase& db_;
};

class SqliteAssignmentRepository final : public domain::IAssignmentRepository {
 public:
  explicit SqliteAssignmentRepository(SqliteDatabase& db) : db_(db) {}
  domain::Id insert(const domain::Assignment& a) override;
  std::optional<domain::Assignment> findById(domain::Id id) const override;
  std::vector<domain::Assignment> listByCourse(domain::Id courseId) const override;

 private:
  SqliteDatabase& db_;
};

class SqliteExtensionRepository final : public domain::IExtensionRepository {
 public:
  explicit SqliteExtensionRepository(SqliteDatabase& db) : db_(db) {}
  void upsert(const domain::Extension& e) override;
  std::optional<domain::Extension> find(domain::Id assignmentId, domain::Id studentId) const override;

 private:
  SqliteDatabase& db_;
};

class SqliteSubmissionRepository final : public domain::ISubmissionRepository {
 public:
  explicit SqliteSubmissionRepository(SqliteDatabase& db) : db_(db) {}
  domain::Id insert(const domain::Submission& s) override;
  void update(const domain::Submission& s) override;
  std::optional<domain::Submission> findByAssignmentAndStudent(domain::Id a, domain::Id s) const override;
  std::optional<domain::SubmissionSummary> findSummary(domain::Id submissionId) const override;
  std::vector<domain::SubmissionSummary> find(const domain::SubmissionFilter& f) const override;
  void addVersion(const domain::SubmissionVersion& v) override;
  std::vector<domain::SubmissionVersion> versions(domain::Id submissionId) const override;
  int latestVersionNo(domain::Id submissionId) const override;
  std::vector<domain::ReportRow> report(domain::Id departmentId) const override;

 private:
  SqliteDatabase& db_;
};

class SqliteAuditRepository final : public domain::IAuditRepository {
 public:
  explicit SqliteAuditRepository(SqliteDatabase& db) : db_(db) {}
  void append(const domain::AuditEntry& e) override;
  std::vector<domain::AuditEntry> listForSubmission(domain::Id submissionId) const override;
  std::vector<domain::AuditEntry> listRecent(int limit) const override;

 private:
  SqliteDatabase& db_;
};

class SqliteNotificationRepository final : public domain::INotificationRepository {
 public:
  explicit SqliteNotificationRepository(SqliteDatabase& db) : db_(db) {}
  void add(const domain::Notification& n) override;
  std::vector<domain::Notification> listForUser(domain::Id userId, int limit) const override;
  int countUnread(domain::Id userId) const override;
  void markAllRead(domain::Id userId) override;

 private:
  SqliteDatabase& db_;
};

class SqliteUnitOfWork final : public domain::IUnitOfWork {
 public:
  explicit SqliteUnitOfWork(SqliteDatabase& db) : db_(db) {}
  void begin() override;
  void commit() override;
  void rollback() noexcept override;

 private:
  SqliteDatabase& db_;
  bool active_ = false;
};

}  // namespace lms::infrastructure
