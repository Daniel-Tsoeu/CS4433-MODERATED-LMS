#include "domain/UserFactory.hpp"

namespace lms::domain {

std::unique_ptr<User> UserFactory::create(Role role, UserData data) {
  switch (role) {
    case Role::Admin:
      return std::make_unique<Administrator>(std::move(data));
    case Role::Lecturer:
      return std::make_unique<Lecturer>(data);
    case Role::Hod:
      return std::make_unique<HeadOfDepartment>(data);
    case Role::Student:
      return std::make_unique<Student>(data);
  }
  return nullptr;  // unreachable: all enumerators handled
}

}  // namespace lms::domain
