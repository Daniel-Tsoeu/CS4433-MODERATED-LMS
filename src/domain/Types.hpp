#pragma once
#include <cstdint>
#include <string>

namespace lms::domain {

using Id = std::int64_t;
/// Seconds since the Unix epoch (UTC). The class diagram's TimePoint maps to
/// an INTEGER column, so the domain uses the same representation.
using TimePoint = std::int64_t;
inline constexpr TimePoint kSecondsPerDay = 24LL * 60 * 60;

enum class Role : std::uint8_t { Admin, Lecturer, Hod, Student };
enum class SubmissionStatus : std::uint8_t { Submitted, Late, Marked, Returned, Moderated, Released };
enum class AuditField : std::uint8_t { RawMark, FinalMark };

enum class Permission : std::uint8_t {
  ManageUsers,
  ManageDepartments,
  ManageCourses,
  ManageEnrolments,
  CreateAssignment,
  GrantExtension,
  MarkSubmission,
  ViewOwnCourses,
  SubmitWork,
  ViewReleasedMarks,
  ModerateMarks,
  ReleaseMarks,
  ViewDepartmentReports,
  ViewAudit,
  ViewNotifications
};

// Database representation (matches the CHECK constraints in sql/schema.sql).
std::string toDb(Role r);
std::string toDb(SubmissionStatus s);
std::string toDb(AuditField f);
Role roleFromDb(const std::string& s);
SubmissionStatus statusFromDb(const std::string& s);
AuditField auditFieldFromDb(const std::string& s);

// Human-readable names for the user interface.
std::string displayName(Role r);
std::string displayName(SubmissionStatus s);
std::string displayName(AuditField f);

}  // namespace lms::domain
