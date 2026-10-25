#pragma once
#include <memory>

#include "domain/User.hpp"

namespace lms::domain {

/// Factory pattern: the only place that maps a Role to a User subclass.
/// A new role needs one new subclass and one new case here.
class UserFactory {
 public:
  static std::unique_ptr<User> create(Role role, UserData data);
};

}  // namespace lms::domain
