#pragma once
#include <algorithm>
#include <optional>
#include <string>

#include "domain/Types.hpp"

namespace lms::domain {

struct Department {
  Id id = 0;
  std::string code;
  std::string name;
};

struct Course {
  Id id = 0;
  std::string code;
  std::string title;
  Id departmentId = 0;
  int academicYear = 0;
};

struct Extension {
  Id id = 0;
  Id assignmentId = 0;
  Id studentId = 0;
  TimePoint newDeadline = 0;
  std::string reason;
  Id grantedBy = 0;
  TimePoint grantedAt = 0;
};

struct Assignment {
  Id id = 0;
  Id courseId = 0;
  std::string title;
  std::string description;
  TimePoint deadline = 0;
  double maxMark = 0;
  Id createdBy = 0;
  TimePoint createdAt = 0;

  /// The student's own deadline: the extension if one exists, else the original.
  TimePoint effectiveDeadline(const std::optional<Extension>& ext) const {
    return ext ? std::max(deadline, ext->newDeadline) : deadline;
  }
  /// Throws ValidationError if the data may not be stored.
  void validateNew(TimePoint now) const;
};

struct SubmissionVersion {
  Id submissionId = 0;
  int versionNo = 0;
  std::string filePath;
  std::string originalName;
  std::int64_t sizeBytes = 0;
  TimePoint uploadedAt = 0;
};

struct AuditEntry {
  Id id = 0;
  Id submissionId = 0;
  Id changedBy = 0;
  TimePoint changedAt = 0;
  AuditField field = AuditField::FinalMark;
  std::optional<double> oldValue;
  double newValue = 0;
  std::string reason;
  std::string changedByName;  // filled by read queries (username), not stored
};

struct Notification {
  Id id = 0;
  Id recipientId = 0;
  std::string message;
  TimePoint createdAt = 0;
  bool isRead = false;
};

/// A candidate upload, described before it is stored.
struct FileInfo {
  std::string originalName;
  std::int64_t sizeBytes = 0;
};

}  // namespace lms::domain
