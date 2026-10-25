#pragma once
// Repository interfaces (Repository pattern). Small and per-aggregate
// (interface segregation). Implemented in infrastructure with parameterised SQL.
#include <memory>
#include <optional>
#include <vector>

#include "domain/Entities.hpp"
#include "domain/Submission.hpp"
#include "domain/User.hpp"

namespace lms::domain {

class IUserRepository {
 public:
  virtual ~IUserRepository() = default;
  virtual std::size_t count() const = 0;
  virtual Id insert(const User& user) = 0;  // common row + role profile row
  virtual void update(const User& user) = 0;
  virtual std::unique_ptr<User> findById(Id id) const = 0;
  virtual std::unique_ptr<User> findByUsername(const std::string& username) const = 0;
  virtual std::unique_ptr<User> findHeadOfDepartment(Id departmentId) const = 0;
  virtual std::vector<std::unique_ptr<User>> listAll() const = 0;
};

class IDepartmentRepository {
 public:
  virtual ~IDepartmentRepository() = default;
  virtual Id insert(const Department& d) = 0;
  virtual void update(const Department& d) = 0;
  virtual std::optional<Department> findById(Id id) const = 0;
  virtual std::vector<Department> listAll() const = 0;
};

class ICourseRepository {
 public:
  virtual ~ICourseRepository() = default;
  virtual Id insert(const Course& c) = 0;
  virtual std::optional<Course> findById(Id id) const = 0;
  virtual std::vector<Course> listAll() const = 0;
  virtual std::vector<Course> listForLecturer(Id lecturerId) const = 0;
  virtual std::vector<Course> listForStudent(Id studentId) const = 0;
  virtual void assignLecturer(Id courseId, Id lecturerId) = 0;
  virtual bool isLecturerAssigned(Id courseId, Id lecturerId) const = 0;
  virtual std::vector<Id> lecturerIds(Id courseId) const = 0;
  virtual void enrol(Id courseId, Id studentId, TimePoint at) = 0;
  virtual bool isEnrolled(Id courseId, Id studentId) const = 0;
};

class IAssignmentRepository {
 public:
  virtual ~IAssignmentRepository() = default;
  virtual Id insert(const Assignment& a) = 0;
  virtual std::optional<Assignment> findById(Id id) const = 0;
  virtual std::vector<Assignment> listByCourse(Id courseId) const = 0;
};

class IExtensionRepository {
 public:
  virtual ~IExtensionRepository() = default;
  /// One extension per (assignment, student); a second grant replaces the first.
  virtual void upsert(const Extension& e) = 0;
  virtual std::optional<Extension> find(Id assignmentId, Id studentId) const = 0;
};

/// A submission joined with the names needed to display or authorise it.
struct SubmissionSummary {
  Submission submission;
  std::string studentName;
  std::string studentNumber;
  std::string assignmentTitle;
  Id courseId = 0;
  std::string courseCode;
  Id departmentId = 0;
  double maxMark = 0;
};

struct SubmissionFilter {
  std::optional<Id> assignmentId, courseId, departmentId, studentId;
  std::optional<SubmissionStatus> status;
};

struct ReportRow {
  std::string courseCode;
  std::string assignmentTitle;
  int moderated = 0;
  int released = 0;
  std::optional<double> averageFinalMark;
};

class ISubmissionRepository {
 public:
  virtual ~ISubmissionRepository() = default;
  virtual Id insert(const Submission& s) = 0;
  virtual void update(const Submission& s) = 0;
  virtual std::optional<Submission> findByAssignmentAndStudent(Id assignmentId, Id studentId) const = 0;
  virtual std::optional<SubmissionSummary> findSummary(Id submissionId) const = 0;
  virtual std::vector<SubmissionSummary> find(const SubmissionFilter& filter) const = 0;
  virtual void addVersion(const SubmissionVersion& v) = 0;
  virtual std::vector<SubmissionVersion> versions(Id submissionId) const = 0;
  virtual int latestVersionNo(Id submissionId) const = 0;
  virtual std::vector<ReportRow> report(Id departmentId) const = 0;
};

class IAuditRepository {
 public:
  virtual ~IAuditRepository() = default;
  virtual void append(const AuditEntry& e) = 0;  // append-only
  virtual std::vector<AuditEntry> listForSubmission(Id submissionId) const = 0;
  virtual std::vector<AuditEntry> listRecent(int limit) const = 0;
};

class INotificationRepository {
 public:
  virtual ~INotificationRepository() = default;
  virtual void add(const Notification& n) = 0;
  virtual std::vector<Notification> listForUser(Id userId, int limit) const = 0;
  virtual int countUnread(Id userId) const = 0;
  virtual void markAllRead(Id userId) = 0;
};

}  // namespace lms::domain
