#include "domain/AccessPolicy.hpp"

#include "domain/Errors.hpp"

namespace lms::domain {

void AccessPolicy::require(const User& user, Permission p) const {
  if (!user.isActive()) throw AuthorizationError("Your account is deactivated.");
  if (!user.can(p)) throw AuthorizationError("Your role is not allowed to perform this action.");
}

void AccessPolicy::requireTeaches(const User& lecturer, Id courseId) const {
  if (!courses_.isLecturerAssigned(courseId, lecturer.id()))
    throw AuthorizationError("You are not assigned to this course.");
}

void AccessPolicy::requireEnrolled(const User& student, Id courseId) const {
  if (!courses_.isEnrolled(courseId, student.id()))
    throw AuthorizationError("You are not enrolled in this course.");
}

void AccessPolicy::requireHeadOf(const User& hod, Id departmentId) const {
  const auto own = hod.departmentId();
  if (hod.role() != Role::Hod || !own.has_value() || *own != departmentId)
    throw AuthorizationError("This item belongs to another department.");
}

}  // namespace lms::domain
