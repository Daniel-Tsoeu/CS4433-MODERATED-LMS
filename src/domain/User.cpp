#include "domain/User.hpp"

#include "domain/Errors.hpp"

namespace lms::domain {

bool User::isLocked(TimePoint now) const {
  return d_.lockedUntil.has_value() && now < *d_.lockedUntil;
}

void User::recordFailedLogin(TimePoint now) {
  ++d_.failedAttempts;
  if (d_.failedAttempts >= kMaxFailedLogins) {
    d_.lockedUntil = now + kLockoutSeconds;
    d_.failedAttempts = 0;  // start counting afresh once the lock expires
  }
}

void User::recordSuccessfulLogin() {
  d_.failedAttempts = 0;
  d_.lockedUntil.reset();
}

const std::set<Permission>& Administrator::permissions() const {
  static const std::set<Permission> p{Permission::ManageUsers, Permission::ManageDepartments,
                                      Permission::ManageCourses, Permission::ManageEnrolments,
                                      Permission::ViewAudit};
  return p;
}

Lecturer::Lecturer(const UserData& d) : User(d), department_(0) {
  if (!d.departmentId) throw ValidationError("A lecturer must belong to a department.");
  department_ = *d.departmentId;
}
const std::set<Permission>& Lecturer::permissions() const {
  static const std::set<Permission> p{Permission::CreateAssignment, Permission::GrantExtension,
                                      Permission::MarkSubmission, Permission::ViewNotifications};
  return p;
}

HeadOfDepartment::HeadOfDepartment(const UserData& d) : User(d), department_(0) {
  if (!d.departmentId) throw ValidationError("A head of department must head a department.");
  department_ = *d.departmentId;
}
const std::set<Permission>& HeadOfDepartment::permissions() const {
  static const std::set<Permission> p{Permission::ModerateMarks, Permission::ReleaseMarks,
                                      Permission::ViewDepartmentReports, Permission::ViewAudit,
                                      Permission::ViewNotifications};
  return p;
}

Student::Student(const UserData& d) : User(d) {
  if (d.studentNumber.empty()) throw ValidationError("A student must have a student number.");
  number_ = d.studentNumber;
}
const std::set<Permission>& Student::permissions() const {
  static const std::set<Permission> p{Permission::ViewOwnCourses, Permission::SubmitWork,
                                      Permission::ViewReleasedMarks, Permission::ViewNotifications};
  return p;
}

}  // namespace lms::domain
