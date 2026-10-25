#pragma once
#include "domain/Repositories.hpp"

namespace lms::domain {

/// Central role-based access control (FR2, BR2). Services call it first, so
/// no use case can forget a check. Two levels: the role's permission, then
/// the scope (this course, this department, this enrolment).
class AccessPolicy {
 public:
  explicit AccessPolicy(const ICourseRepository& courses) : courses_(courses) {}

  void require(const User& user, Permission p) const;
  void requireTeaches(const User& lecturer, Id courseId) const;
  void requireEnrolled(const User& student, Id courseId) const;
  void requireHeadOf(const User& hod, Id departmentId) const;

 private:
  const ICourseRepository& courses_;
};

}  // namespace lms::domain
